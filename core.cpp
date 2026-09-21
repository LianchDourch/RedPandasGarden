#include "core.h"
#include "esimanager.h"
#include <QMessageBox>
#include "mainwindow.h"
#include "util.h"
#include "redpandasgarden.h"

size_t qHash(const ItemStack& itemStack, size_t seed) {
    return qHashMulti(seed, itemStack.getItem(), itemStack.getQuantity());
}

bool itemMetaLess(const ItemMeta& a, const ItemMeta& b)
{
    auto ia = a.begin();
    auto ib = b.begin();

    while (ia != a.end() && ib != b.end()) {
        if (ia.key() != ib.key())
            return ia.key() < ib.key();

        if (ia.value() != ib.value())
            return ia.value().toString() < ib.value().toString();

        ++ia;
        ++ib;
    }

    return a.size() < b.size();
}

QString moneyToString(double money, bool fullNumber) {
    if (fullNumber) {
        QLocale frenchLocale(QLocale::French, QLocale::France);

        return frenchLocale.toString(money, 'f', 2)
                   .replace(",", ".")
                   .replace(frenchLocale.toString(1000).at(1), " ")
               + " ISK";
    } else {
        double absVal = std::abs(money);
        QString sign = (money < 0) ? "-" : "";

        if (absVal < 1.0) {
            return sign + QString::number(absVal, 'f', 2);
        }

        const char* suffixes[] = { "", "k", " mil", "b", "t" };

        int exp = static_cast<int>(std::floor(std::log10(absVal)));
        int group = (exp >= 0) ? (exp / 3) : 0;

        if (group > 4) group = 4;

        double mantissa = absVal / std::pow(10.0, group * 3);

        int decimals = 2;
        if (mantissa >= 100.0) {
            decimals = 0;
        } else if (mantissa >= 10.0) {
            decimals = 1;
        }

        double factor = std::pow(10.0, decimals);
        mantissa = std::round(mantissa * factor) / factor;

        if (decimals == 2 && mantissa >= 10.0) {
            decimals = 1;
        } else if (decimals == 1 && mantissa >= 100.0) {
            decimals = 0;
        } else if (decimals == 0 && mantissa >= 1000.0) {
            mantissa /= 1000.0;
            decimals = 2;
            group++;
        }

        return sign + QString::number(mantissa, 'f', decimals) + suffixes[group] + " ISK";
    }
}

void Station::fetchDatas(std::function<void(Station*)> then, ERROR_LISTENER_CPP) {
    fetchSystemCostIndex(then);
}

void Station::fetchSystemCostIndex(std::function<void(Station*)> then, ERROR_LISTENER_CPP) {
    QSet<int> work = {getSystemId()};

    EsiManager::fetchSystemCostIndices(work, [this, then] (const QMap<int, VariableCostIndices>& map) {
        setSystemCostIndices(map.value(getSystemId(), VariableCostIndices{}));
        then(this);
    }, errorListener);
}

void Item::fetchId(std::function<void(Item*)> then, std::function<void(const QString&)> error) {
    bool ok;
    QVariant res = EsiManager::requestTypeId(getName(), &ok, [error] (QSqlError v) { error(v.text()); });
    if (ok) {
        this->id = res.toInt(&ok);
        if (ok) {
            then(this);
            return;
        }
    }
    error("Something went bad when fetching id from SDE.");
}

double Blueprint::getTotalMaterialsPrice(Station* hub, double modifier, int runcount, bool sellPrices) {
    double res = 0.;
    for (const auto& [datas, qt]: getRecipeInput().asKeyValueRange()) {
        if (!datas.isValid()) continue;
        double temp = datas.getItem()->priceFor(hub, std::ceil(static_cast<double>(qt) * modifier) * runcount, sellPrices);
        if (temp == -1) return -1;

        res += temp;
    }
    return res;
}

void Blueprint::getTotalMaterialsPrice(Station* hub, std::function<void(double)> receiver, double modifier, int runcount, bool wtb) {
    bool* error = new bool(false);
    int* remainingOrders = getRecipeInput().isEmpty() ? nullptr : new int(getRecipeInput().size());
    double* res = new double(0.);
    for (const auto& [datas, qt]: getRecipeInput().asKeyValueRange()) {
        if (!datas.isValid()) {
            Q_ASSERT("Invalid itemStack in recipe input.");
        }
        datas.getItem()->priceFor(hub, std::ceil(static_cast<double>(qt) * modifier) * runcount, [this, receiver, remainingOrders, error, res] (double price) {
            *remainingOrders -= 1;
            if (price == -1) *error = true;
            *res += price;
            if (*remainingOrders <= 0) {
                delete remainingOrders;
                double r = *res;
                delete res;
                bool e = *error;
                delete error;

                if (e) receiver(-1);
                else {
                    receiver(r);
                }
            }
        }, wtb);
    }
}

bool Item::checkValidity() {
    if (getId() != 0) return true;
    bool res = false;
    fetchId([&res] (Item*) { res = true; }, [&res] (const QString&) { res = false; });
    return res;
}

double Item::getAdjustedPrice(bool refreshIfPossible) {
    return EsiManager::getAdjustedPrice(getTypeId(), refreshIfPossible);
}

double Item::getEstimatedPrice(bool refreshIfPossible) {
    return EsiManager::getEstimatedPrice(getTypeId(), refreshIfPossible);
}

void Item::fetchIcon(std::function<void(Item*)> then, int size, std::function<void(const QString&)> error) {
    EsiManager::requestIcon(getId(), size, [this, then] (const QPixmap& pixmap) {
        this->icon = pixmap;
        then(this);
    }, error, isBlueprint() ? "bp" : "icon");
}

void Item::fetchDatas(Station* hub, std::function<void(Item*)> then, std::function<void(const QString&)> error) {
    if (!isLoaded()) {
        fetchId([this, then, error, hub] (Item* item) {
            item->fetchIcon([this, then, error, hub] (Item* item) {
                this->loaded = true;
                item->fetchPrices(hub, then, error);
            }, DEFAULT_ICON_SIZE, error);
        }, error);
    } else {
        if (hasOrder(hub, false, 0) && hasOrder(hub, true, 0)) {
            then(this);
        } else {
            fetchPrices(hub, then, error);
        }
    }
}

bool ItemStack::isSimilar(const ItemStackDatas& o) const {
    return isSimilar(o.itemStack);
}

ItemStackDatas ItemStack::getDatas() const {
    return ItemStackDatas{*this};
}

void Item::fetchPrices(Station* hub, std::function<void(Item*)> then, std::function<void(const QString&)> error) {
    if (hub == nullptr) then(this);
    else {
        EsiManager::fetchRegionalOrders(getId(), hub->getRegionId(), [this, then, hub] (const QByteArray& datas) {
            QPair<std::vector<MarketOrder>, std::vector<MarketOrder>> work = EsiManager::parseMarketOrders(datas, hub->getStationId());

            this->prices[hub] = MarketPrices{
                QVector<MarketOrder>(work.first.begin(), work.first.begin() + std::min(work.first.size(), Item::MAX_ORDER_HISTORY)),
                QVector<MarketOrder>(work.second.begin(), work.second.begin() + std::min(work.second.size(), Item::MAX_ORDER_HISTORY))
            };

            then(this);
        });
    }
}

void Items::registerItem(Item* item, bool fetchDatasIfAdded) {
    if (item->isBlueprint() && dynamic_cast<Blueprint*>(item) == nullptr) throw new std::exception;
    VALUES.insert(item->getName(), item);
    if (fetchDatasIfAdded) item->fetchDatas(nullptr, [] (Item*) {});
}

void Items::registerItem(const QString& name, bool fetchDatasIfAdded) {
    Item* work = nullptr;
    if (EsiManager::isBlueprint(name)) work = new Blueprint(name);
    else work = new Item(name);

    if (work->checkValidity()) {
        registerItem(work, fetchDatasIfAdded);
    } else delete work;
}

bool Item::isBlueprint() const {
    return EsiManager::isBlueprint(this->getId());
}

void Blueprint::readRecipeItems() {
    bool ok;
    QSqlQuery query = EsiManager::requestSDE(
        "SELECT t.typeID, t.typeName, m.quantity "
        "FROM industryActivityMaterials m "
        "INNER JOIN invTypes t ON m.materialtypeID = t.typeID "
        "WHERE m.typeID = :blueprintid AND m.activityID = 1 ",
        {{"blueprintid", getId()}}, &ok);

    if (!ok) Util::error("Unable to gather recipe items for " + getName());
    else {
        int typeId;
        QString typeName;
        int quantity;
        Item* item;
        while (query.next()) {
            item = Items::getOrCreate(query.value("typename").toString(), query.value("typeid").toInt());
            quantity = query.value("quantity").toInt();

            input.insert({item}, quantity);
        }
    }
}

void Blueprint::readRecipeOutput() {
    bool ok;
    QSqlQuery query = EsiManager::requestSDE(
        "SELECT t.typeName, m.quantity "
        "FROM industryactivityproducts m "
        "INNER JOIN invTypes t ON m.productTypeId = t.typeID "
        "WHERE m.typeID = :blueprintid AND m.activityID = 1; ",
        {{"blueprintid", getTypeId()}}, &ok);

    if (!ok) {
        Util::error("Unable to gather blueprint output for " + getName());
    } else {
        if (query.next()) {
            this->output = {Items::fromName(query.value("typename").toString()), query.value("quantity").toInt()};
        } else {
            Util::error("No output found for " + getName());
        }
    }
}

void Blueprint::readCraftTime() {
    bool ok;
    QSqlQuery query = EsiManager::requestSDE(
        "SELECT \"time\" FROM industryactivity WHERE typeID = :blueprintid AND activityID = 1",
        {{"blueprintid", getTypeId()}}, &ok);

    if (!ok) {
        Util::error("Unable to gather blueprint craft time for " + getName());
    } else {
        if (query.next()) {
            int time = query.value("time").toInt();
            this->duration = time;
        } else {
            Util::error("No duration found for " + getName());
        }
    }
}

Blueprint* ItemStack::getBlueprint() const { return dynamic_cast<Blueprint*>(item); }

QPixmap ItemStack::getIcon() const { return isValid() ? (isBPC() ? getBlueprint()->getBpcIcon() : getItem()->getIcon()) : QPixmap(); }

ItemStack::ItemStack(const ItemStackDatas& datas, int quantity) : item(datas.itemStack.item), quantity(quantity), itemMeta(datas.itemStack.itemMeta) {

}

void Blueprint::fetchBpcIcon(std::function<void(Item*)> then, std::function<void(const QString&)> error) {
        EsiManager::requestIcon(getId(), DEFAULT_ICON_SIZE, [this, then] (const QPixmap& pixmap) {
            this->bpcTexture = pixmap;
            then(this);
        }, error, "bpc");
}

void Item::priceFor(Station* hub, int quantity, std::function<void(double)> receiver, bool wtb) {
    if (hub == nullptr) receiver(-1);
    else {
        EsiManager::fetchRegionalOrders(getId(), hub->getRegionId(), [this, receiver, hub, quantity, wtb] (const QByteArray& datas) mutable {
            QPair<std::vector<MarketOrder>, std::vector<MarketOrder>> work = EsiManager::parseMarketOrders(datas, hub->getStationId());

            double res = 0.;
            unsigned long long counter = 0;
            for (MarketOrder order: (wtb ? work.first : work.second)) {
                if (order.isNull()) continue;

                counter += 1;

                if (order.volumeRemain < quantity) {
                    res += order.price * order.volumeRemain;
                    quantity -= order.volumeRemain;
                } else {
                    res += order.price * quantity;
                    quantity = 0;

                    if (counter > (wtb ? this->prices[hub].sell : this->prices[hub].buy).size()) {
                        (wtb ? this->prices[hub].sell : this->prices[hub].buy) = wtb ?
                                QVector<MarketOrder>(work.first.begin(), work.first.begin() + std::min(work.first.size(), counter)) :
                                QVector<MarketOrder>(work.second.begin(), work.second.begin() + std::min(work.second.size(), counter));
                    }
                    receiver(res);
                    return;
                }
            }
            receiver(-1);
        });
    }
}

void Blueprint::fetchManufacturingInformations(Station* buy_hub, Station* sell_hub, Station *facility, Character* character, int runcount, double materialEfficiciency, double timeEfficiency, bool sellPrices, bool buyImmediately,
                                               std::function<void (double, double, double, Duration)> receiver) {
    getTotalMaterialsPrice(buy_hub, [this, buy_hub, sell_hub, facility, character, runcount, receiver, materialEfficiciency, sellPrices, buyImmediately, timeEfficiency] (double value) {
        double totalMaterialsPrice = value;

        if (getOutput().isNull()) readRecipeOutput();

        getOutput().getItem()->priceFor(sell_hub, sellPrices ? 1 : runcount * getOutput().getQuantity(), [this, facility, timeEfficiency, character, sell_hub, sellPrices, receiver, totalMaterialsPrice, runcount, materialEfficiciency] (double value) {
            double outputPrice = sellPrices ? value * runcount * getOutput().getQuantity() : value;

            fetchJobCost(facility, character, materialEfficiciency, [this, totalMaterialsPrice, timeEfficiency, receiver, outputPrice, runcount] (double value) {
                double jobcost = value * runcount;

                receiver(totalMaterialsPrice, jobcost, outputPrice, getDuration(runcount, timeEfficiency));
            });
        }, sellPrices);

    }, materialEfficiciency, runcount, buyImmediately);
}

void Station::addToDB() {
    QSqlQuery query = EsiManager::requestERP(
        "INSERT INTO savedstations ( stationId, solarSystemId, customname, regionId ) VALUES (:stationId, :solarSystemId, :customname, :regionId)",
        {{"stationId", getStationId()},
         {"solarSystemId", getSystemId()},
         {"regionId", getRegionId()},
         {"customname", getName()}});

    query = EsiManager::requestERP("SELECT localId FROM savedstations WHERE stationId = :stationId", {{"stationId", getStationId()}});
    if (query.next()) {
        setLocalId(query.value("localId").toLongLong());
    }

    QString baseRequest = "INSERT INTO stationsindustrydatas (stationLocalId, activityId) "
                          "VALUES (:localId, :activityId)";

    EsiManager::ERP.transaction();

    for (int i = 0; i < 6; i++) {
        query = EsiManager::requestERP(baseRequest,
                                       {
                                           {"localId", getLocalId()}, {"activityId", Stations::ACTIVITIES[i].first}
                                       });
        Util::println("activityId while adding: ", Stations::ACTIVITIES[i].first);
    }

    EsiManager::ERP.commit();

    updateToDB();
}

void Station::updateToDB() {
    QSqlQuery query = EsiManager::requestERP(
        "UPDATE savedstations SET (stationId, solarSystemId, customname, regionId) = (:stationId, :solarSystemId, :customname, :regionId) WHERE localId = :localId",
        {{"stationId", getStationId()},
         {"solarSystemId", getSystemId()},
         {"regionId", getRegionId()},
         {"customname", getName()}, {"localId", getLocalId()}});

    QString baseRequest = "UPDATE stationsindustrydatas SET (facilityTax, materialModifier, durationModifier, costModifier) "
                          "= (:taxes, :matMod, :durMod, :costMod) WHERE stationLocalId = :localId AND activityId = :activityId";

    EsiManager::ERP.transaction();
    for (int i = 0; i < 6; i++) {
        query = EsiManager::requestERP(baseRequest,
                                       {
                                           {"activityId", Stations::ACTIVITIES[i].first},
                                           {"taxes", getFacilityTaxes().get(i)}, {"matMod", getMaterialBonuses().get(i)},
                                           {"durMod", getDurationBonuses().get(i)}, {"costMod", getJobCostBonuses().get(i)},
                                           {"localId", getLocalId()}
                                       });
        Util::println("activityId while updating: ", Stations::ACTIVITIES[i].first);
    }

    EsiManager::ERP.commit();
}

int VariableCostIndices::MANUFACTURING = 0;
int VariableCostIndices::TIME_RESEARCH = 1;
int VariableCostIndices::MATERIAL_RESEARCH = 2;
int VariableCostIndices::COPYING = 3;
int VariableCostIndices::INVENTION = 4;
int VariableCostIndices::REACTIONS = 5;

void Stations::readActivities(const QMap<int, int>& mappedIds) {
    QSqlQuery query = EsiManager::requestERP("SELECT * FROM industryactivities");

    while (query.next()) {
        int activityId = query.value("activityId").toInt();
        ACTIVITIES.insert(mappedIds.value(activityId), {activityId, query.value("activityName").toString()});
        REVERSED_ACTIVITIES.insert(query.value("activityName").toString(), mappedIds.value(activityId));
    }
}

QMap<ItemStackDatas, int> Items::parseItems(const QString& str) {
    QStringList work = str.split("\n");

    Item* item = nullptr;
    int amount = 0;
    int sep;
    QMap<ItemStackDatas, int> res = {};

    for (const QString& line: work) {
        if (line.trimmed().isEmpty()) continue;
        sep = line.indexOf(" x ");
        if (sep == -1) {
            item = Items::fromName(line);
            amount = 1;
        } else {
            amount = line.left(sep).toInt();
            item = Items::fromName(line.mid(sep + QString(" x ").size()));
        }
        if (item != nullptr) res.insert({item}, amount);
    }

    return res;
}

void Items::parseItemsAndLoad(const QString& str, Station* hub, std::function<void(const QMap<ItemStackDatas, int>& map)> then) {
    QMap<ItemStackDatas, int> work = parseItems(str);

    int* remainingItems = new int(work.size());

    for (const auto& [datas, qt]: work.asKeyValueRange()) {
        datas.getItem()->fetchDatas(hub, [remainingItems, work, then] (Item* item) {
            (*remainingItems) -= 1;
            if (*remainingItems == 0) {
                delete remainingItems;
                then(work);
            }
        }, [remainingItems, work, then, datas] (const QString& error) {
                             Util::println("Error while loading ", datas.getItem()->getName());
                             (*remainingItems) -= 1;
                             if (*remainingItems == 0) {
                                 delete remainingItems;
                                 then(work);
                             }
                         });
    }
}

void Blueprint::fetchJobCost(Station* facility, Character* character, double materialEfficiency, std::function<void(double)> receiver) {
    double adjustedPrice = 0.;
    for (const auto& [datas, qt]: getRecipeInput().asKeyValueRange()) {
        if (datas.getItem()->hasAdjustedPrice()) adjustedPrice += qt * datas.getItem()->getAdjustedPrice();
        Util::println("Adjusted price of ", datas.getItem()->getName(), ": ", datas.getItem()->getAdjustedPrice(), " (", qt, " = ", qt * datas.getItem()->getAdjustedPrice(), ")");
    }
    Util::println("Adjusted price: ", adjustedPrice);

    //double res =
    //    adjustedPrice * ((station->getSystemCostIndices().getManufacturing() * station->getJobCostBonuses().getManufacturing()) + station->getFacilityTaxes().getManufacturing() + station->getSccSurcharge().getManufacturing() + (character == nullptr ? 0 : character->isOmega() ? 0 : 0.0025));

    double res = 0.;

    Util::println("Job Cost Bonuses: ", facility->getJobCostBonuses().getManufacturing());
    res += adjustedPrice * facility->getSystemCostIndices().getManufacturing() * facility->getJobCostBonuses().getManufacturing();
    Util::println("Job Gross Cost: ", adjustedPrice * facility->getSystemCostIndices().getManufacturing() * facility->getJobCostBonuses().getManufacturing());

    res += adjustedPrice * facility->getFacilityTaxes().getManufacturing();
    Util::println("Facility Taxes: ", adjustedPrice * facility->getFacilityTaxes().getManufacturing());
    res += adjustedPrice * facility->getSccSurcharge().getManufacturing();
    Util::println("Scc Surcharges: ", adjustedPrice * facility->getSccSurcharge().getManufacturing());
    res += adjustedPrice * (character == nullptr ? 0 : character->isOmega() ? 0 : 0.0025);
    Util::println("Alpha Modifier: ", adjustedPrice * (character == nullptr ? 0 : character->isOmega() ? 0 : 0.0025));

    receiver(res);
}

void Character::fetchPortrait(std::function<void(Character*)> then) {
    if (this->portrait.isNull()) return forceFetchPortrait(then);
    else then(this);
}

void Character::forceFetchPortrait(std::function<void(Character*)> then) {
    EsiManager::fetchPlayerPortrait(getPlayerId(), 256, [this, then] (QPixmap map) {
        this->portrait = std::move(map);
        then(this);
    });
}

void Character::fetchSkills(EsiConnector* esiConnector, std::function<void(Character*)> then) {
    if (esiConnector == nullptr) {
        // MMMMMMMMH problème
        esiConnector->characterId();
    }


    QObject::connect(esiConnector, &EsiConnector::skillsReceived, [then, this, esiConnector] (const QJsonObject& o) {
        Util::println("Skills fetched");
        QJsonArray array = o["skills"].toArray();
        SkillMap map = SkillMap();
        QJsonObject obj;
        for (QJsonValueRef ref: array) {
            obj = ref.toObject();
            map.insert(obj["skill_id"].toInteger(), obj["active_skill_level"].toInt());
        }
        this->skills = std::move(map);

        then(this);
    });

    esiConnector->fetchSkills();
}

void Character::fetchDatas(EsiConnector* esiConnector, std::function<void(Character*)> then) {
    fetchPortrait([esiConnector, this, then] (Character* c) {
        this->fetchSkills(esiConnector, then);
    });
}

Station* Stations::registerStation(Station* hub, bool save) {
    for (Station* s: Stations::VALUES) if (s->getStationId() == hub->getStationId()) {
            delete hub;
            return s;
        }
    VALUES.append(hub);
    if (save) {
        hub->addToDB();
    }
    if (hub->getLocalId() == -1) {
        QSqlQuery query = EsiManager::requestERP("SELECT localId FROM savedStations WHERE stationId = :stationId", {{"stationId", hub->getStationId()}});
        if (query.next()) {
            hub->setLocalId(query.value("localId").toInt());
        }
    }
    return hub;
}

void Characters::logCharacter(std::function<void (Character *)> then) {
    EsiConnector* work = new EsiConnector();

    QObject::connect(work, &EsiConnector::loginSucceeded, [work, then] () {
        Character* character = new Character(work->characterId(), work->characterName());
        character->setConnector(work);

        character->fetchDatas(work, [then] (Character* c) { Characters::VALUES.append(c); then(c); });
    });

    QObject::connect(work, &EsiConnector::loginFailed, Util::error);

    work->login();
}

Character::~Character() {
    delete sso;
}

void Core::save() {
    QDir(SAVE_FOLDER).mkpath(".");
    saveStations();
    saveCharacters();
}

void Core::saveStations() {
    // Plus rien à faire ici désormais
}

void Core::saveCharacters() {
    QDir dir(SAVE_FOLDER + CHARACTERS_DIR);
    Util::clearDirectory(dir.path());
    dir.mkpath(".");
    Util::println("Characters: ", Characters::VALUES.length());
    for (Character* c: Characters::VALUES) {
        QFile file(SAVE_FOLDER + CHARACTERS_DIR + QString::number(c->getPlayerId()) + ".save");

        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::critical(MainWindow::INSTANCE, "Unable to save", "Unable to open character file " + c->getName());
            continue;
        }

        QString work = "";

        work += QString::number(c->getPlayerId()) + "\n";
        work += c->getName() + "\n";
        work += QString(c->isOmega() ? "1" : "0") + "\n";
        for (auto [skillId, skillLevel]: c->getSkills().asKeyValueRange()) {
            work += QString::number(skillId) + ":" + QString::number(skillLevel) + "\n";
        }

        file.write(work.toUtf8());
    }
}

void Core::loadCharacters(Then then) {
    bool success = false;
    QStringList work = std::move(Util::listFiles(SAVE_FOLDER + CHARACTERS_DIR, &success));

    if (work.length() == 0) {
        return then();
    }

    int* remainingChars = new int(work.length());

    for (const QString& fileName: work) {
        QFile file(SAVE_FOLDER + CHARACTERS_DIR + fileName);

        if (file.open(QIODevice::ReadOnly)) {
            QString work = file.readAll();

            QStringList w0 = work.split("\n");
            if (w0.length() < 3) continue;

            PLAYERID id = w0[0].toLongLong();
            QString name = w0[1];
            bool omega = (w0[2] == "1");
            Character* character = new Character(id, name);
            character->setOmega(omega);

            SkillMap map = {};
            for (int i = 3; i < w0.length(); i++) {
                if (w0[i].trimmed().isEmpty()) continue;
                QStringList w1 = w0[i].split(":");
                map.insert(w1[0].toLongLong(), w1[1].toInt());
            }
            character->setSkills(map);

            character->fetchPortrait([then, remainingChars] (Character* character) {
                *remainingChars -= 1;
                Characters::registerCharacter(character);
                if (*remainingChars <= 0) {
                    delete remainingChars;
                    then();
                }
            });
        }
    }
}

void Core::preload(Then then) {
    then();
}

void Core::load(Then then) {
    loadStations([then] () {
        loadCharacters(then);
    });
}

Item* Items::fromId(qint64 id, bool fetchDatasIfAdded) {
    bool ok = false;
    QSqlQuery w = EsiManager::requestSDE("SELECT typeName FROM invtypes WHERE typeId = :typeid", {{"typeid", id}}, &ok);

    if (!ok || !w.next()) return nullptr;
    return fromName(w.value("typeName").toString(), fetchDatasIfAdded);
}

void Core::loadStations(Then then) {
    QSet<int> systemIds = {};
    bool ok = false;
    QSqlQuery query = EsiManager::requestERP("SELECT * FROM savedstations", {}, &ok);
    if (!ok) Util::error("Unable to get savedstations datas");
    else {
        while (query.next()) {
            Station* work = new Station(
                query.value("customname").toString(),
                query.value("regionId").toInt(),
                query.value("solarSystemid").toInt(),
                query.value("stationId").toLongLong(),
                query.value("localId").toInt());

            QSqlQuery query2 = EsiManager::requestERP(
                "SELECT dt.*, ia.activityname FROM stationsindustrydatas dt INNER JOIN industryactivities ia ON ia.activityId = dt.activityId WHERE dt.stationLocalId = :id",
                {{"id", work->getLocalId()}});
            QList<VariableCostIndices> costIndices(4);

            while (query2.next()) {
                int actIndex = Stations::REVERSED_ACTIVITIES.value(query2.value("activityName").toString());
                costIndices[0].set(actIndex, query2.value("facilityTax").toDouble());
                costIndices[1].set(actIndex, query2.value("materialModifier").toDouble());
                costIndices[2].set(actIndex, query2.value("durationModifier").toDouble());
                costIndices[3].set(actIndex, query2.value("costModifier").toDouble());
                Util::println("Updated ", query2.value("activityName").toString(), " at index ", actIndex, " to ", query2.value("costModifier").toDouble());
            }

            work->setFacilityTaxes(costIndices[0]);
            work->setMaterialBonuses(costIndices[1]);
            work->setDurationBonuses(costIndices[2]);
            work->setJobCostBonuses(costIndices[3]);


            Util::println("Should add ", work->getName());
            systemIds.insert(Stations::registerStation(work, false)->getSystemId());
        }
    }

    EsiManager::fetchSystemCostIndices(systemIds, [then] (const QMap<int, VariableCostIndices>& res) {
        for (Station* s: Stations::VALUES) {
            s->setSystemCostIndices(res.value(s->getSystemId(), {}));
        }
        then();
    }, [] (const QString& error) { QMessageBox::critical(MainWindow::INSTANCE, "Unable to start the application", "Be sure you are connected to internet, and else contact the developer."); });
}

Station* Stations::registerStation(qint64 stationId, const QString &name, bool save) {
    for (Station* station: VALUES) if (station->getStationId() == stationId) return station;

    int regionID;
    int systemID;
    bool ok = false;
    QSqlQuery query = EsiManager::requestSDE(
        "SELECT regionID, solarSystemID FROM stastations WHERE stationID = :stationid",
        {{"stationid", stationId}}, &ok);
    if (!ok) {
        Util::error("Unable to fetch station " + name);
        return nullptr;
    } else {
        if (query.next()) {
            regionID = query.value("regionID").toInt();
            systemID = query.value("solarSystemID").toInt();

        } else {
            Util::error(name + " station not found.");
            return nullptr;
        }
    }

    Station* res = new Station(name, regionID, systemID, stationId, -1);

    return registerStation(res, save);
}

void AbsoluteItemPrice::upload(const QString& token, QJsonObject order, std::function<void(QJsonObject)> receiver) {

    QUrl url("https://market.redpandasgarden.com/makeabsoluteprice.php");

    QNetworkRequest request(url);

    request.setHeader(
        QNetworkRequest::ContentTypeHeader,
        "application/x-www-form-urlencoded"
        );

    QUrlQuery postData;

    postData.addQueryItem(
        "pwkey",
        token
        );

    postData.addQueryItem(
        "orderId",
        QString::number((order["orderId"].toInt()))
        );

    postData.addQueryItem(
        "price",
        QString::number(price, 'f', 2)
        );

    QNetworkReply* reply = RedPandasGarden::networkManager->post(
        request,
        postData.toString(QUrl::FullyEncoded).toUtf8()
        );

    QObject::connect(
        reply,
        &QNetworkReply::finished,
        [reply, order, receiver]()
        {
            const QByteArray response = reply->readAll();

            if (reply->error() != QNetworkReply::NoError)
            {
                qWarning() << "Erreur réseau :"
                           << reply->errorString();

                qWarning() << "Réponse serveur :"
                           << response;

                reply->deleteLater();
                return;
            }

            qDebug() << "Réponse serveur :"
                     << response;

            QJsonParseError parseError;

            const QJsonDocument json =
                QJsonDocument::fromJson(response, &parseError);

            if (parseError.error != QJsonParseError::NoError)
            {
                qWarning() << "JSON invalide :"
                           << parseError.errorString();

                reply->deleteLater();
                return;
            }

            const QJsonObject object = json.object();

            const bool success =
                object.value("success").toBool(false);

            if (success)
            {
                qDebug() << "Prix absolu enregistré :"
                         << object;
                receiver(order);
            }
            else
            {
                const QString error =
                    object.value("error")
                        .toString("Erreur inconnue");

                qWarning() << "Modification du prix refusée :"
                           << error;
            }

            reply->deleteLater();
        }
        );
}

