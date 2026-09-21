#ifndef CORE_H
#define CORE_H

#include <QString>
#include "flagmanager.h"
#include "rserializable.h"
#include "util.h"
#include <QVector>
#include <qjsonobject.h>
#include <qtimer.h>
#include <QSqlQuery>
#include <QPixmap>
#include "settings.h"

#define BLUEPRINT_TIMEEFFICIENCY_ISKEY "bptimeefficiency"
#define BLUEPRINT_MATEFFICIENCY_ISKEY "bpmatefficiency"
#define BLUEPRINT_ISBPC_ISKEY "isbpc"
#define BLUEPRINT_RUNCOUNT_ISKEY "runcount"

class EsiConnector;
class Character;
class ProductionChain;
class ItemStack;
using ItemView = ItemStack;
class Blueprint;
class ItemStackDatas;

typedef qint64 PLAYERID;

QString moneyToString(double money, bool fullNumber = false);

struct Alliance {
    qint64 id = 0;
    QString name = QString();
    QString ticker = QString();

    inline bool isNull() { return id == 0; }
};

struct AllianceStandings {
    qint64 id = 0;
    double standing = 0.;
};

struct KillmailReference
{
    qint64 id = 0;
    QString hash = QString();

    inline bool isNull() const { return id == 0; }
};

class Duration {
public:
    explicit Duration(qint64 seconds = 0) : m_duration(seconds) {}
    explicit Duration(std::chrono::seconds duration) : m_duration(duration) {}

    void operator+=(std::chrono::seconds bonus) { m_duration += bonus; }

    auto operator<=>(const Duration&) const = default;

    Duration operator*(double value) {
        return Duration(std::ceil(static_cast<long double>(totalSeconds()) * value));
    }

    QString toQString() const {
        using namespace std::chrono;
        auto secs = m_duration;

        auto d = duration_cast<days>(secs);    secs -= d;
        auto h = duration_cast<hours>(secs);   secs -= h;
        auto m = duration_cast<minutes>(secs); secs -= m;

        QStringList parts;
        if (d.count() > 0)    parts << QString("%1d").arg(d.count());
        if (h.count() > 0)    parts << QString("%1h").arg(h.count());
        if (m.count() > 0)    parts << QString("%1min").arg(m.count());
        if (secs.count() > 0 || parts.isEmpty()) parts << QString("%1s").arg(secs.count());

        return parts.join(' ');
    }

    int64_t totalSeconds() const { return m_duration.count(); }

private:
    std::chrono::seconds m_duration;
};

struct MarketOrder {
    long long orderId = 0;
    double price = 0.;
    int volumeRemain = 0;
    bool isBuy = false;

    inline bool isNull() const { return orderId == 0; }
};

struct VariableCostIndices {

    QList<double> values = {0., 0., 0., 0., 0., 0.};

    VariableCostIndices() {

    }

    VariableCostIndices(std::initializer_list<double> values) : VariableCostIndices(QList<double>(values)) {
    }

    VariableCostIndices(QList<double> values) {
        this->values = values;
        while (values.length() < 6) values.append(0.);
    }

    static int MANUFACTURING;
    static int TIME_RESEARCH;
    static int MATERIAL_RESEARCH;
    static int COPYING;
    static int INVENTION;
    static int REACTIONS;

    inline double getManufacturing() const { return get(MANUFACTURING); }
    inline double getTimeResearch() const { return get(TIME_RESEARCH); }
    inline double getMaterialResearch() const { return get(MATERIAL_RESEARCH); }
    inline double getCopying() const { return get(COPYING); }
    inline double getInvention() const { return get(INVENTION); }
    inline double getReactions() const { return get(REACTIONS); }

    inline double get(int index) const { return values[index]; }
    inline void set(int index, double value) { values[index] = value; }

    inline QString toQString() const {
        QString res = "VariableCostIndices(";
        bool start = true;
        for (double d: values) {
            if (start) start = false;
            else res += ", ";
            res += QString::number(d * 100.);
        }
        return res + ")";
    }
};

struct StructureData {
    QString ingameName = QString();
    qint32 typeId = 0;

    inline bool isNull() { return ingameName.isNull(); }
};

class Location {
    // TODO séparer Station et location (avec Station : public Location)
};

class Station {
private:
    CONST_CONSTRUCTABLE_PROPERTY_BIGPOD(QString, customName, getName)
    CONST_CONSTRUCTABLE_PROPERTY_POD(qint32, regionId, getRegionId)
    CONST_CONSTRUCTABLE_PROPERTY_POD(qint32, systemId, getSystemId)
    CONST_CONSTRUCTABLE_PROPERTY_POD(qint64, stationId, getStationId)
    GENERAL_PROPERTY_BIGPOD(StructureData, structureDatas, {}, getStructureDatas, setStructureDatas)
    CONST_CONSTRUCTABLE_PROPERTY_POD(qint32, localId, getLocalId)

    GENERAL_PROPERTY_BIGPOD(VariableCostIndices, systemCostIndices, {}, getSystemCostIndices, setSystemCostIndices);
    GENERAL_PROPERTY_BIGPOD(VariableCostIndices, facilityTaxes, {}, getFacilityTaxes, setFacilityTaxes);
    GENERAL_PROPERTY_BIGPOD(VariableCostIndices, materialBonuses, {}, getMaterialBonuses, setMaterialBonuses);
    GENERAL_PROPERTY_BIGPOD(VariableCostIndices, durationBonuses, {}, getDurationBonuses, setDurationBonuses);
    GENERAL_PROPERTY_BIGPOD(VariableCostIndices, jobCostBonuses, {}, getJobCostBonuses, setJobCostBonuses);
public:
    CONSTRUCTOR(Station, customName, regionId, systemId, stationId, localId) {
    }

    void fetchDatas(std::function<void(Station*)> then, ERROR_LISTENER);
    void fetchSystemCostIndex(std::function<void(Station*)> then, ERROR_LISTENER);

    VariableCostIndices getSccSurcharge() {
        return VariableCostIndices({0.04, 0.02, 0.02, 0.02, 0.04, 0.04});
    }

    void setLocalId(qint32 localId) {
        this->localId = localId;
    }

    inline bool isStructure() const {
        return stationId >= 1000000000000l;
    }

    inline bool isPOS() const { return isStructure(); }

    void addToDB();
    void updateToDB();
};

struct Stations {

    static void readActivities(const QMap<int, int>& mappedIds);

    inline static QMap<QString, int> REVERSED_ACTIVITIES = {};
    inline static QMap<int, QPair<int, QString>> ACTIVITIES = {};

    inline static QList<Station*> VALUES = {};

    static Station* registerStation(Station* hub, bool save = false);

    /**
     * @brief registerStation will NOT fetch datas for you
     * @param stationId
     * @param name
     * @return
     */
    static Station* registerStation(qint64 stationId, const QString& name, bool save = false);

    static Station* fromName(const QString &name) {
        for (Station* hub: VALUES) if (hub->getName() == name) return hub;
        return nullptr;
    }
};

class ItemType {
private:
    QString name;

public:
    ItemType(const QString& name) : name(name) {}
};

struct MarketPrices {
    QVector<MarketOrder> sell = {};
    QVector<MarketOrder> buy = {};

    inline bool isNull() const { return sell.isEmpty() && buy.isEmpty(); }
};

class Item {
private:
    inline static const unsigned long long MAX_ORDER_HISTORY = 8;

    using PricesMap = QHash<Station*, MarketPrices>;

    CONST_CONSTRUCTABLE_PROPERTY_BIGPOD(QString, name, getName);
    CONST_CONSTRUCTABLE_PROPERTY_POD(int, id, getId);
    CONST_CONSTRUCTABLE_PROPERTY_BIGPOD(QPixmap, icon, getIcon);
    CONST_CONSTRUCTABLE_PROPERTY_BIGPOD(PricesMap, prices, getPrices);
    CONST_PROPERTY_POD(bool, loaded, isLoaded)

public:
    CONSTRUCTOR(Item, name), id(0), loaded(false), icon() {

    }

    constexpr bool isNull() const { return name.isNull(); }

    inline bool hasOrder(Station* hub, bool sell = true, int index = 0) {
        return prices.contains(hub) && (sell ? prices[hub].sell : prices[hub].buy).size() > index;
    }

    inline MarketOrder getOrder(Station* hub, bool sell, int index = 0) {
        return (sell ? prices[hub].sell : prices[hub].buy)[index];
    }

    inline double getPrice(Station* hub, bool sell = true, int index = 0) {
        return getOrder(hub, sell, index).price;
    }

    virtual void fetchDatas(Station* hub, std::function<void(Item*)> then, ERROR_LISTENER);
    void fetchPrices(Station* hub, std::function<void(Item*)> then, ERROR_LISTENER);

    bool checkValidity();

    virtual bool isBlueprint() const;
    int getTypeId() const { return getId(); }

    void priceFor(Station* hub, int quantity, std::function<void(double)> receiver, bool wtb = true);
    double priceFor(Station* hub, int quantity, bool sellPrices = true) {
        if (!getPrices().contains(hub)) return -1;
        MarketPrices work = getPrices()[hub];
        double res = 0.;
        for (int i = 0; i < (sellPrices ? work.sell : work.buy).size(); i++) {
            MarketOrder w0 = getOrder(hub, sellPrices, i);
            if (w0.isNull()) continue;

            if (w0.volumeRemain < quantity) {
                res += w0.price * w0.volumeRemain;
                quantity -= w0.volumeRemain;
            } else {
                res += w0.price * quantity;
                return res;
            }
        }
        return -1;
    }

    double hasAdjustedPrice() {
        return getAdjustedPrice() >= 0.;
    }

    double getAdjustedPrice(bool refreshIfPossible = false);
    double getEstimatedPrice(bool refreshIfPossible = false);
private:
    void fetchId(std::function<void(Item*)> then, ERROR_LISTENER);
    void fetchIcon(std::function<void(Item*)> then, int size = 64, ERROR_LISTENER);

};

class Items {
public:
    inline static QMap<QString, Item*> VALUES = {};

    static Item* fromName(const QString& name, bool fetchDatasIfAdded = false) {
        if (!VALUES.contains(name)) registerItem(name, fetchDatasIfAdded);
        return VALUES.value(name, nullptr);
    }

    inline static Item* getOrCreate(const QString& name, int id) {
        return fromName(name);
    }

    static Item* fromId(qint64 id, bool fetchDatasIfAdded = false);

    static void registerItem(const QString& name, bool fetchDatasIfAdded = false);

    static void registerItem(Item* item, bool fetchDatasIfAdded = false);

    static bool areItemsLoaded(const QHash<Item*, int>& items) {
        for (auto [k, v]: items.asKeyValueRange()) if (!k->isLoaded()) return false;
        return true;
    }

    static QMap<ItemStackDatas, int> parseItems(const QString& str);

    static void parseItemsAndLoad(const QString& str, Station* hub, std::function<void(const QMap<ItemStackDatas, int>& map)> then);
};

typedef QMap<QString, QVariant> ItemMeta;
bool itemMetaLess(const ItemMeta& a, const ItemMeta& b);

class ItemStack {
private:
    FULL_PROPERTY_PTR(Item*, item, nullptr, getItem, setItem, isValid);
    GENERAL_PROPERTY_POD(int, quantity, 1, getQuantity, setQuantity);
    GENERAL_PROPERTY_BIGPOD(ItemMeta, itemMeta, {}, getItemMeta, setItemMeta);

public:
    ItemStack(Item* item = nullptr, int quantity = 1) : item(item), quantity(quantity) {}
    ItemStack(const ItemStackDatas& datas, int quantity);

    inline bool isNull() const { return !isValid(); }

    inline void setAmount(int amount) { return setQuantity(amount); }
    inline int getAmount() const { return getQuantity(); }

    inline bool isBlueprint() const { return isValid() && getItem()->isBlueprint(); }
    inline bool isBPC() const { return isBlueprint() && getItemMeta().value(BLUEPRINT_ISBPC_ISKEY, QVariant(false)).toBool(); }
    inline bool isBPO() const { return isBlueprint() && !getItemMeta().value(BLUEPRINT_ISBPC_ISKEY, QVariant(true)).toBool(); }
    inline void setBpc(bool isBpc) { itemMeta.insert(BLUEPRINT_ISBPC_ISKEY, isBpc); }
    inline double getBlueprintTimeModifier() const { return isBlueprint() ? getItemMeta().value(BLUEPRINT_TIMEEFFICIENCY_ISKEY, QVariant(1.)).toDouble() : 0.; }
    inline double getBlueprintMaterialsModifier() const { return isBlueprint() ? getItemMeta().value(BLUEPRINT_MATEFFICIENCY_ISKEY, QVariant(1.)).toDouble() : 0.; }
    inline void setBlueprintTimeModifier(double modifier) { itemMeta.insert(BLUEPRINT_TIMEEFFICIENCY_ISKEY, modifier); }
    inline void setBlueprintMaterialsModifier(double modifier) { itemMeta.insert(BLUEPRINT_MATEFFICIENCY_ISKEY, modifier); }
    inline int getBlueprintRunCount() const { return isBlueprint() ? getItemMeta().value(BLUEPRINT_RUNCOUNT_ISKEY, QVariant(0)).toInt() : 0; }
    inline void setBlueprintRunCount(int runcount) { setMetadata(BLUEPRINT_RUNCOUNT_ISKEY, runcount); }
    Blueprint* getBlueprint() const;

    inline void setMetadata(const QString& key, const QVariant& value) {
        itemMeta.insert(key, value);
    }

    /**
     * @brief isSimilar just doesn't check the quantity.
     * @param o
     * @return
     */
    inline bool isSimilar(const ItemStack& o) const {
        return item == o.item && itemMeta == o.itemMeta;
    }

    bool isSimilar(const ItemStackDatas& o) const;

    inline bool operator==(const ItemStack& o) const {
        return isSimilar(o) && quantity == o.quantity;
    }

    inline bool operator<(const ItemStack& o) const {
        return this->item < o.item || (item == o.item && quantity < o.quantity || (quantity == o.quantity && itemMetaLess(itemMeta, o.itemMeta)));
    }

    inline bool accept(const ItemStack& item) const {
        return (*this) == item;
    }

    QPixmap getIcon() const;

    ItemStack clone() const {
        ItemStack res = ItemStack(item, quantity);
        res.setItemMeta(this->itemMeta);
        return res;
    }

    ItemStackDatas getDatas() const;

    inline ItemStack& addQuantity(int qt) {
        this->quantity += qt;
        return *this;
    }

    inline ItemStack& mulQuantity(int coef) {
        this->quantity *= coef;
        return *this;
    }

    inline ItemStack addedQuantity(int qt) const { return clone().addedQuantity(qt); }
    inline ItemStack multipliedQuantity(int qt) const { return clone().mulQuantity(qt); }
};

size_t qHash(const ItemStack& itemStack, size_t seed = 0);

// ItemStack without amount
struct ItemStackDatas {
    ItemStack itemStack;

    inline bool operator<(const ItemStackDatas& o) const {
        return itemStack.getItem() < o.itemStack.getItem() || (itemStack.getItem() == o.itemStack.getItem() && itemMetaLess(itemStack.getItemMeta(), o.itemStack.getItemMeta()));
    }

    inline bool isValid() const { return getItem() != nullptr; }

    inline ItemStack& getItemStack() { return itemStack; }
    inline Item* getItem() const { return itemStack.getItem(); }
    inline const ItemMeta& getItemMeta() const { return itemStack.getItemMeta(); }
};


/**
 * @brief The Blueprint class
 * Station is both BUYING AND SELLING STATION (for fetchDatas)
 */
class Blueprint : public Item {
public:
    using super = Item;
    using RecipeMap = QMap<ItemStackDatas, int>;
    using Output = QPair<Item*, int>;
    inline static Output DEFAULT = {nullptr, 0};
private:
    CONST_GENERAL_PROPERTY_BIGPOD(QPixmap, bpcTexture, QPixmap(), getBpcIcon);
    CONST_GENERAL_PROPERTY_BIGPOD(RecipeMap, input, {}, getRecipeInput);
    CONST_GENERAL_PROPERTY_BIGPOD(ItemStack, output, {}, getOutput);
    CONST_GENERAL_PROPERTY_POD(int, duration, 0, _getDuration);

public:
    Blueprint(QString name) : Item(name) {
        if (!checkValidity()) throw new std::exception;
        if (!Item::isBlueprint()) throw new std::exception;
    }

    void fetchDatas(Station* station, std::function<void(Item*)> then, ERROR_LISTENER) override {
        super::fetchDatas(nullptr, [this, then, errorListener] (Item* item) {
            readRecipeItems();
            readRecipeOutput();
            readCraftTime();
            fetchBpcIcon(then, errorListener);
        });
    }

    inline int getTypeId() const { return getId(); }

    bool isBlueprint() const override {
        return true;
    }

    Duration getDuration(int runcount = 1, double timeEfficiency = 1.) {
        int time = (static_cast<double>(_getDuration()) * timeEfficiency) * runcount;
        return Duration(time);
    }

    /**
     * @brief getTotalMaterialsPrice SUPPOSES THAT THE ITEMS ARE FETCHED !!!:
     * @return
     */
    double getTotalMaterialsPrice(Station* hub, double modifier = 1, int runcount = 1, bool sellPrices = true);

    /**
     * @brief getTotalMaterialsPrice will fetch the prices if needed, so should return -1 only if there isn't enough of the item on the market
     * @param hub
     * @param receiver
     * @param modifier
     * @param runcount
     */
    void getTotalMaterialsPrice(Station* hub, std::function<void(double)> receiver, double modifier = 1, int runcount = 1, bool wtb = true);

    void fetchJobCost(Station* hub, Character* character, double materialEfficiciency, std::function<void(double)> receiver);

    double getJobCost(Station* station, Character* character, double materialEfficiency) {
        double work;
        fetchJobCost(station, character, materialEfficiency, [&work] (double v) { work = v; });
        return work;
    }

    void fetchManufacturingInformations(Station* buy_hub, Station *sell_hub, Station* facility, Character *character, int runcount, double materialEfficicency, double timeEfficiency, bool sellPrices, bool buyViaBuyOrders,
                                        std::function<void (double, double, double, Duration)> receiver);

private:
    void readRecipeItems();
    void readRecipeOutput();
    void readCraftTime();
    void fetchBpcIcon(std::function<void(Item*)> then, ERROR_LISTENER);
};


struct SkillMap : public QMap<qint64, int> {
    using QMap<qint64, int>::QMap;
};

struct CharacterView {
    PLAYERID id = 0;
    QString name = QString();
};

struct Killmail {
    KillmailReference ref = {};
    CharacterView victim = {};
    qint64 deadTypeId = 0;
    QList<CharacterView> agressors = {};
    int finalBlowId = -1; // l'index dans agressors
    QDate date = QDate();
    qint64 systemId = 0;

    inline bool isNull() const { return ref.isNull(); }
};

class Character {
private:
    CONST_CONSTRUCTABLE_PROPERTY_POD(PLAYERID, id, getPlayerId);
    CONST_CONSTRUCTABLE_PROPERTY_BIGPOD(QString, name, getName);
    GENERAL_PROPERTY_BIGPOD(SkillMap, skills, {}, getSkills, setSkills);
    GENERAL_PROPERTY_POD(bool, omega, false, isOmega, setOmega);
    CONST_PROPERTY_BIGPOD(QPixmap, portrait, getPortrait)
    FULL_PROPERTY_PTR(EsiConnector*, sso, nullptr, getConnector, setConnector, hasConnector)

public:
    CONSTRUCTOR(Character, id, name) {}

    ~Character();

    void fetchDatas(EsiConnector* esiConnector = nullptr, std::function<void(Character*)> then = [] (Character*) {});

    void fetchPortrait(std::function<void(Character*)> then = [] (Character*) {});
    void forceFetchPortrait(std::function<void(Character*)> then = [] (Character*) {});
    void fetchSkills(EsiConnector* esiConnector = nullptr, std::function<void(Character*)> then = [] (Character*) {});

    inline const QPixmap& getHead() const { return getPortrait(); }
};

// A UTILISER DANS DES SHARED PTR
class ItemPrice {
public:
    virtual int getPriceType() { return 0; }

    virtual void upload(const QString& token, QJsonObject order, std::function<void(QJsonObject)> receiver = [] (QJsonObject) {}) { receiver(order); }
};

class AbsoluteItemPrice : public ItemPrice {
private:
    double price;

public:
    AbsoluteItemPrice(double price) : price(price) {}

    int getPriceType() override { return 1; }

    void upload(const QString& token, QJsonObject order, std::function<void(QJsonObject)> receiver = [] (QJsonObject) {}) override;
};

struct CharacterAsset
{
    qint64 itemId = 0;
    qint64 typeId = 0;
    int quantity = 0;

    qint64 locationId = 0;
    QString locationType;
    QString locationFlag;

    bool isSingleton = false;

    static CharacterAsset parse(const QSqlQuery *q) {
        return CharacterAsset{
            q->value("itemId").toLongLong(),
            q->value("typeId").toLongLong(),
            q->value("quantity").toInt(),
            q->value("locationId").toLongLong(),
            q->value("locationType").toString(),
            q->value("locationFlag").toString(),
            q->value("isSingleton").toBool(),
        };
    }

    inline bool isNull() const { return typeId == 0; }
};

struct Container : public CharacterAsset {
    QString name;
    QList<CharacterAsset> contents;
};

struct BlueprintAsset : public CharacterAsset {
    int materialEfficiency;
    int timeEfficiency;
    bool bpc;
    int runsRemaining;
};

class Characters {
public:
    inline static QList<Character*> VALUES = {};


    static void logCharacter(std::function<void(Character*)> then);

    inline static Character* fromName(const QString &name) {
        for (Character* c: VALUES) if (c->getName() == name) return c;
        return nullptr;
    }

    inline static Character* registerCharacter(Character* c) {
        VALUES.append(c);
        return c;
    }
};

class Core
{
public:
    inline static QString SAVE_FOLDER = "Saves/";
    inline static QString STATIONS_FILENAME = "stations.save";
    inline static QString CHARACTERS_DIR = "Characters/";

    /**
     * @brief preload Beofre ESIManager load
     * @param then
     */
    static void preload(Then then);
    /**
     * @brief load After ESIManager load
     * @param then
     */
    static void load(Then then);

    static void loadStations(Then then);
    static void loadCharacters(Then then);


    static void save();

    static void saveStations();
    static void saveCharacters();
};

Q_DECLARE_METATYPE(ItemStack)
Q_DECLARE_METATYPE(QList<ItemStack>)

#endif // CORE_H
