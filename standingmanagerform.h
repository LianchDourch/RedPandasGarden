#ifndef STANDINGMANAGERFORM_H
#define STANDINGMANAGERFORM_H

#include "core.h"
#include <QWidget>
#include <qlistwidget.h>

namespace Ui {
class StandingManagerForm;
}

class StandingManagerForm : public QWidget
{
    Q_OBJECT

public:
    explicit StandingManagerForm(QWidget *parent = nullptr);
    ~StandingManagerForm();

    void reset();
    void refresh() {
        refreshHead();
        refreshAlliances();
    }
    void refreshHead();
    void refreshAlliances();
    void refreshOutput();

    inline void refreshMutableLists() { refreshPlayerList(); }
    void refreshPlayerList();

    double getWantedStandings();

    inline Alliance getCurrentAlliance() { return alliance; }
    void setCurrentAlliance(Alliance alliance);
private slots:
    void on_pushButton_searchAlliance_clicked();

    void on_comboBox_players_activated(int index);

    void on_pushButton_reset_clicked();

    void on_listWidget_alliances_itemClicked(QListWidgetItem *item);

    void on_pushButton_submit_clicked();

    void on_pushButton_write_clicked();

private:
    Ui::StandingManagerForm *ui;
    Alliance alliance;
    QMap<qint64, double> m_toAdd = {};
    QList<qint64> m_toRemove = {};
};

#endif // STANDINGMANAGERFORM_H
