#include "addstationform.h"
#include "core.h"
#include "esimanager.h"
#include "ui_addstationform.h"
#include <QStringListModel>
#include "mainwindow.h"
#include <QMessageBox>

QCompleter* initCompleter(QCompleter *completer) {
    completer->setFilterMode(Qt::MatchStartsWith);

    completer->setCaseSensitivity(Qt::CaseInsensitive);

    completer->setCompletionMode(QCompleter::PopupCompletion);

    return completer;
}

AddStationForm::AddStationForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AddStationForm)
{
    ui->setupUi(this);

    regionCompleter = initCompleter(new QCompleter);
    ui->lineEdit_regionName->setCompleter(regionCompleter);

    systemCompleter = initCompleter(new QCompleter);
    ui->lineEdit_systemName->setCompleter(systemCompleter);

    stationCompleter = initCompleter(new QCompleter);
    ui->lineEdit_stationName->setCompleter(stationCompleter);

    refreshRegisteredStationList();
    refreshRegionsCompleter();
    refreshCharacters();
    refreshPOSEdits();

    QSqlQuery query = EsiManager::requestERP("SELECT activityId, activityName FROM industryactivities");

    while (query.next()) {
        ui->listWidget_activities->addItem(query.value("activityName").toString());
        activityDatas.insert(query.value("activityName").toString(), {0., 1, 0, 1});
        Util::println("Inserted ", query.value("activityName").toString(), ": ", activityDatas.value(query.value("activityName").toString()).durationMod);
    }

    query.finish();

    setCurrentActivityData(Stations::ACTIVITIES.value(0).second, activityDatas.value(Stations::ACTIVITIES.value(0).second));
}

void AddStationForm::checkSystem() {
    ui->pushButton_addStation->setEnabled(false);
    if (ui->lineEdit_systemName->text().trimmed().isEmpty()) return;

    bool ok = false;
    stationBuildDatas.systemName = ui->lineEdit_systemName->text();
    QSqlQuery query = EsiManager::requestSDE("SELECT regionid FROM mapsolarsystems WHERE solarSystemName = :systemname", {{"systemname", stationBuildDatas.systemName}});
    query.next();
    stationBuildDatas.regionId = query.value("regionid").toInt();
    query = EsiManager::requestSDE(
        "SELECT regionname FROM mapregions WHERE regionID = :regionId",
        {{"regionId", stationBuildDatas.regionId}}, &ok);
    if (!ok) return;
    else {
        if (query.next()) {
            stationBuildDatas.regionName = query.value("regionName").toString();
            ui->lineEdit_regionName->setText(stationBuildDatas.regionName);
            refreshRegionsCompleter();
        }
    }
}

bool AddStationForm::isPlayerOwned() {
    return ui->checkBox_setPOS->isChecked();
}

Character* AddStationForm::getCurrentCharacter() {
    return Characters::fromName(ui->comboBox_player->currentText());
}

void AddStationForm::refreshPOSEdits() {
    if (isPlayerOwned()) {
        ui->lineEdit_stationName->setEnabled(false);
        ui->widget_stationId->show();
        ui->lineEdit_stationId->clear();
        ui->lineEdit_stationName->clear();
    } else {
        ui->lineEdit_stationName->setEnabled(true);
        ui->widget_stationId->hide();
    }
}

void AddStationForm::checkStation(Then then) {
    ui->pushButton_addStation->setEnabled(false);
    if (isPlayerOwned()) {
        if (ui->lineEdit_stationId->text().trimmed().isEmpty()) return;
        stationBuildDatas.stationId = ui->lineEdit_stationId->text().toLongLong();

        Character* character = getCurrentCharacter();
        if (character == nullptr || !character->hasConnector()) {
            QMessageBox::critical(this, "Unable to fetch station", "Please check that you selected the right player, and that you are logged in.");
        } else {
            character->getConnector()->fetchPlayerStation(stationBuildDatas.stationId, [this, then] (const PlayerStation& station) {
                Util::println("Setting stationName to ", station.name);
                ui->lineEdit_stationName->setText(station.name);
                Util::println("should be good");
                ui->lineEdit_customName->setText(station.name);
                stationBuildDatas.stationId = station.structureId;
                stationBuildDatas.stationName = station.name;
                stationBuildDatas.systemId = station.solarSystemId;
                Util::println("Type ID: ", station.typeId);

                bool ok = false;
                QSqlQuery query = EsiManager::requestSDE(
                    "SELECT solarSystemName FROM mapsolarsystems WHERE solarSystemID = :solarSystemId",
                    {{"solarSystemId", stationBuildDatas.systemId}}, &ok);

                if (!ok) return then();
                else {
                    if (query.next()) {
                        stationBuildDatas.systemName = query.value("solarSystemName").toString();
                        ui->lineEdit_systemName->setText(stationBuildDatas.systemName);
                        checkSystem();
                    }
                }
                then();
            }, Util::error);
        }
    } else {
        if (ui->lineEdit_stationName->text().trimmed().isEmpty()) return;

        bool ok = false;
        stationBuildDatas.stationName = ui->lineEdit_stationName->text();
        QSqlQuery query = EsiManager::requestSDE("SELECT solarsystemId FROM stastations WHERE stationName = :stationName", {{"stationName", stationBuildDatas.stationName}});
        stationBuildDatas.systemId = query.value("solarsystemId").toInt();
        query = EsiManager::requestSDE(
            "SELECT solarSystemName FROM mapsolarsystems WHERE solarSystemID = :solarSystemId",
            {{"solarSystemId", stationBuildDatas.systemId}}, &ok);

        if (!ok) return;
        else {
            if (query.next()) {
                stationBuildDatas.systemName = query.value("solarSystemName").toString();
                ui->lineEdit_systemName->setText(stationBuildDatas.systemName);
                checkSystem();
            }
        }

        stationBuildDatas.stationName = ui->lineEdit_stationName->text();
        query = EsiManager::requestSDE(
            "SELECT stationId FROM stastations WHERE stationName = :stationName",
            {{"stationName", stationBuildDatas.stationName}}, &ok);
        stationBuildDatas.stationId = query.value("stationId").toLongLong();
        ui->lineEdit_stationId->setText(QString::number(stationBuildDatas.stationId));

        then();
    }


}

void AddStationForm::refreshSystemsCompleter() {
    ui->lineEdit_systemName->clear();

    bool ok = false;
    QSqlQuery query = EsiManager::requestSDE(
        "SELECT s.solarSystemName "
        "FROM mapSolarSystems s "
        "JOIN mapRegions r ON s.regionID = r.regionID "
        "WHERE r.regionName = :regionName "
        "ORDER BY s.solarSystemName ASC",
        {{"regionName", ui->lineEdit_regionName->text()}}, &ok);

    if (!ok) ui->lineEdit_regionName->clear();
    else {
        QStringList res = {};
        res.reserve(200); // Juste pour opti, et je vois mal une région avoir plus de 150 systèmes

        while (query.next()) {
            res.append(query.value("solarSystemName").toString());
        }

        QStringListModel* model = new QStringListModel(res, systemCompleter);

        systemCompleter->setModel(model);
    }
}

void AddStationForm::refreshCharacters() {
    QString previous = ui->comboBox_player->currentText();
    ui->comboBox_player->clear();
    for (Character* c: Characters::VALUES) ui->comboBox_player->addItem(c->getName());
    if (!previous.isEmpty()) ui->comboBox_player->setCurrentText(previous);
}

void AddStationForm::refreshRegionsCompleter() {
    bool ok = false;
    QSqlQuery query = EsiManager::requestSDE("SELECT regionName FROM mapregions", {}, &ok);
    if (ok) {
        QStringList regions = {};
        regions.reserve(120); // 113 regions au moment où j'écris ces lignes

        while (query.next()) {
            regions.append(query.value("regionName").toString());
        }

        QStringListModel* model = new QStringListModel(regions, regionCompleter);

        regionCompleter->setModel(model);
    } else {
        Util::error("Unable to fetch regions !!");
    }
}

AddStationForm::~AddStationForm()
{
    delete ui;
}

void AddStationForm::refreshStationsCompleter() {
    ui->lineEdit_stationName->clear();

    QStringList res = {};
    res.reserve(10);

    qint32 solarSystemId;

    {
        bool ok = false;
        QSqlQuery query = EsiManager::requestSDE(
            "SELECT solarSystemID FROM mapsolarsystems WHERE solarsystemname = :solarsystemname",
            {{"solarsystemname", ui->lineEdit_systemName->text()}}, &ok);

        if (!ok || !query.next()) {
            Util::error("Unable to read systemId while seeking station: " + ui->lineEdit_systemName->text());
            return;
        } else {
            solarSystemId = query.value("solarSystemID").toInt();
        }
    }

    //! Stations NPC
    {
        bool ok = false;
        QSqlQuery query = EsiManager::requestSDE(
            "SELECT stationName FROM stastations WHERE solarSystemID = :systemId",
            {{"systemId", solarSystemId}}, &ok);

        if (!ok) {
            Util::error("Unable to read stations.");
            return;
        } else {
            while (query.next()) {
                res.append(query.value("stationname").toString());
            }
        }
    }

    //! Structures

    stationCompleter->setModel(new QStringListModel(res, stationCompleter));
}

void AddStationForm::refreshRegisteredStationList() {

}

void AddStationForm::on_pushButton_seekRegion_clicked()
{
    refreshSystemsCompleter();
    refreshStationsCompleter();
}


void AddStationForm::on_pushButton_seekSystem_clicked()
{
    checkSystem();
    refreshStationsCompleter();
}


void AddStationForm::on_pushButton_searchStation_clicked()
{
    submit();
}

void AddStationForm::on_pushButton_seekStation_clicked()
{
    checkStation([this] () { ui->lineEdit_customName->setText(ui->lineEdit_stationName->text()); });
}

void AddStationForm::submit() {
    checkStation([this] () {
        ui->label_stationName->clear();
        ui->label_regionName->clear();
        ui->label_systemName->clear();
        ui->label_stationCustomName->clear();
        ui->label_industryindex->clear();

        QList<VariableCostIndices> work = {{}, {}, {}, {}};
        for (auto [key, data]: activityDatas.asKeyValueRange()) {
            int workSubIndex = Stations::REVERSED_ACTIVITIES.value(key);
            work[0].set(workSubIndex, data.tax);
            work[1].set(workSubIndex, data.durationMod);
            work[2].set(workSubIndex, data.materialMod);
            work[3].set(workSubIndex, data.jobCostMod);
        }

        Util::println("Taxes: ", work[0].toQString());
        Util::println("Duration Modifier: ", work[1].toQString());
        Util::println("Materials Modifier: ", work[2].toQString());
        Util::println("Job Cost Modifier: ", work[3].toQString());

        if (isPlayerOwned()) {
            bool ok = false;
        } else {
            bool ok = false;
            QSqlQuery query = EsiManager::requestSDE(
                "SELECT "
                "s.solarSystemID,  "
                "s.solarSystemName, "
                "st.stationID,  "
                "r.regionName, "
                "r.regionID  "
                "    FROM staStations st "
                "        JOIN mapSolarSystems s ON st.solarSystemID = s.solarSystemID "
                "      JOIN mapRegions r ON st.regionID = r.regionID "
                "      WHERE st.stationName = :stationName ",
                {{"stationName", stationBuildDatas.stationName}}, &ok);
            if (!ok) {
                Util::error("Unable to fetch " + ui->lineEdit_stationName->text());
            } else {
                if (query.next()) {
                    stationBuildDatas.regionId = query.value("regionID").toInt();
                    stationBuildDatas.systemId = query.value("solarSystemID").toInt();
                    stationBuildDatas.stationId = query.value("stationID").toInt();
                    stationBuildDatas.regionName = query.value("regionName").toString();
                    stationBuildDatas.systemName = query.value("solarSystemName").toString();
                } else {
                    Util::error(ui->lineEdit_stationName->text() + " not found.");
                }
            }
        }

        if (buildingStation != nullptr) {
            delete buildingStation;
            buildingStation = nullptr;
        }

        stationBuildDatas.customName = ui->lineEdit_customName->text();

        buildingStation = new Station(
            stationBuildDatas.customName,
            stationBuildDatas.regionId,
            stationBuildDatas.systemId,
            stationBuildDatas.stationId,
            -1);
        buildingStation->setFacilityTaxes(work[0]);
        buildingStation->setDurationBonuses(work[1]);
        buildingStation->setMaterialBonuses(work[2]);
        buildingStation->setJobCostBonuses(work[3]);
        buildingStation->fetchDatas([this] (Station* station) {
            ui->pushButton_addStation->setEnabled(true);
        }, [this] (const QString& error) {
                                        delete buildingStation;
                                        buildingStation = nullptr;
                                        QMessageBox::critical(this, "Unable to get station", "It seems impossible to get the station from ESI...");
                                    });
        refreshBuildingStation(stationBuildDatas.regionName, stationBuildDatas.systemName, stationBuildDatas.stationName);
    });
}

void AddStationForm::refreshBuildingStation() {

}

void AddStationForm::refreshBuildingStation(const QString& regionName, const QString& systemName, const QString& stationName) {
    ui->label_stationCustomName->setText(buildingStation == nullptr ? "" : buildingStation->getName());
    ui->label_regionName->setText(regionName + " (" + (buildingStation == nullptr ? QString("0") : QString::number(buildingStation->getRegionId())) + ")");
    ui->label_systemName->setText(systemName + " (" + (buildingStation == nullptr ? QString("0") : QString::number(buildingStation->getSystemId())) + ")");
    ui->label_stationName->setText(stationName + " (" + (buildingStation == nullptr ? QString("0") : QString::number(buildingStation->getStationId())) + ")");
    ui->label_industryindex->setText(QString::number((buildingStation == nullptr ? 0 : buildingStation->getSystemCostIndices().getManufacturing()) * 100.) + "%");
    ui->checkBox_isPOS->setChecked(false); // FIXME
}


void AddStationForm::on_pushButton_addStation_clicked()
{
    if (buildingStation != nullptr) {
        Stations::registerStation(buildingStation, true);
        MainWindow::INSTANCE->onStationsListChange();
        buildingStation = nullptr;
    }
}


void AddStationForm::on_pushButton_fetchFreeports_clicked()
{
    EsiManager::fetchFreeports([this] () { refreshStationsCompleter(); });
}

void AddStationForm::setCurrentActivityData(const QString &str, ActivityData datas) {
    currentDisplayedActivity = str;

    ui->label_titleActivity->setText("Values for " + str + " (%)");

    ui->doubleSpinBox_tax->setValue(datas.tax * 100.);
    ui->doubleSpinBox_durModifier->setValue((datas.durationMod - 1.) * 100.);
    ui->doubleSpinBox_matModif->setValue((datas.materialMod - 1.) * 100.);
    ui->doubleSpinBox_jobcostmodif->setValue((datas.jobCostMod -1.) * 100.);

    activityDatas[str] = datas;

    setActivityDisplayEnabled(isActivityEnabled(str));
}

void AddStationForm::saveValues() {
    if (currentDisplayedActivity.isNull()) return;

    activityDatas[currentDisplayedActivity] = {
        ui->doubleSpinBox_tax->value() / 100.,
        1. + (ui->doubleSpinBox_durModifier->value() / 100.),
        1. + (ui->doubleSpinBox_matModif->value() / 100.),
        1. + (ui->doubleSpinBox_jobcostmodif->value() / 100.)
    };
}

void AddStationForm::on_listWidget_activities_itemDoubleClicked(QListWidgetItem *item)
{
    saveValues();
    Util::println("Value: ", activityDatas.value(item->text()).durationMod);
    setCurrentActivityData(item->text(), activityDatas.value(item->text()));
}

void AddStationForm::on_pushButton_saveValues_clicked()
{
    saveValues();
}


void AddStationForm::on_pushButton_seekStationId_clicked()
{
    checkStation([] () {});
}


void AddStationForm::on_checkBox_setPOS_checkStateChanged(const Qt::CheckState &arg1)
{
}


void AddStationForm::on_checkBox_setPOS_stateChanged(int arg1)
{
}


void AddStationForm::on_checkBox_setPOS_clicked()
{
    refreshPOSEdits();
}


void AddStationForm::on_pushButton_enable_clicked()
{
    setCurrentActivityData(currentDisplayedActivity, isActivityEnabled(currentDisplayedActivity) ? ActivityData{0, 1, 0, 1} : ActivityData{0, 1, 1, 1});
}

bool AddStationForm::isActivityEnabled(const QString& activity) {
    return ui->doubleSpinBox_durModifier->value() > -100.;
}

void AddStationForm::setActivityDisplayEnabled(bool b) {
    ui->pushButton_enable->setText(b ? "Disable" : "Enable");
}

