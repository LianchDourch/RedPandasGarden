#ifndef ADDSTATIONFORM_H
#define ADDSTATIONFORM_H

#include "core.h"
#include <QWidget>
#include <QCompleter>
#include <QListWidgetItem>

namespace Ui {
class AddStationForm;
}

struct ActivityData {
    double tax = 0.;
    double durationMod = 0.;
    double materialMod = 0.;
    double jobCostMod = 0.;
};

struct StationBuildData {
    int regionId = 0, systemId = 0;
    qint64 stationId = 0;
    QString stationName = QString();
    QString customName = QString();
    QString regionName = QString();
    QString systemName = QString();

    inline bool isNull() {
        return stationName.isNull();
    }
};

class AddStationForm : public QWidget
{
    Q_OBJECT

public:
    explicit AddStationForm(QWidget *parent = nullptr);
    ~AddStationForm();

    void refreshRegisteredStationList();
    void refreshRegionsCompleter();
    void refreshSystemsCompleter();
    void refreshStationsCompleter();
    void checkSystem();
    void checkStation(Then then);
    void refreshPOSEdits();

    void submit();
    void refreshBuildingStation();
    void refreshBuildingStation(const QString& regionName, const QString& systemName, const QString& stationName);
    void refreshCharacters();

    void setCurrentActivityData(const QString &str, ActivityData value);

    void saveValues();

    bool isPlayerOwned();

    Character* getCurrentCharacter();

    bool isActivityEnabled(const QString& activity);
    void setActivityDisplayEnabled(bool b);
private slots:
    void on_pushButton_seekRegion_clicked();

    void on_pushButton_seekSystem_clicked();

    void on_pushButton_searchStation_clicked();

    void on_pushButton_seekStation_clicked();

    void on_pushButton_addStation_clicked();

    void on_pushButton_fetchFreeports_clicked();

    void on_listWidget_activities_itemDoubleClicked(QListWidgetItem *item);

    void on_pushButton_saveValues_clicked();

    void on_pushButton_seekStationId_clicked();

    void on_checkBox_setPOS_checkStateChanged(const Qt::CheckState &arg1);

    void on_checkBox_setPOS_stateChanged(int arg1);

    void on_checkBox_setPOS_clicked();

    void on_pushButton_enable_clicked();

private:
    Ui::AddStationForm *ui;
    QCompleter* regionCompleter = nullptr;
    QCompleter* systemCompleter = nullptr;
    QCompleter* stationCompleter = nullptr;
    Station* buildingStation = nullptr;
    QMap<QString, ActivityData> activityDatas = {};
    QString currentDisplayedActivity = QString();
    StationBuildData stationBuildDatas = {};
};

#endif // ADDSTATIONFORM_H
