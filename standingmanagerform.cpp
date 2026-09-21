#include "standingmanagerform.h"
#include "core.h"
#include "esimanager.h"
#include "ui_standingmanagerform.h"

StandingManagerForm::StandingManagerForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::StandingManagerForm)
{
    ui->setupUi(this);

    reset();
}

StandingManagerForm::~StandingManagerForm()
{
    delete ui;
}

void StandingManagerForm::reset() {
    refreshPlayerList();

    ui->lineEdit_allianceSearch->clear();
    ui->label_allianceName->clear();
    ui->listWidget_alliances->clear();
    ui->listWidget_toRemove->clear();
    ui->listWidget_toAdd->clear();

    ui->radioButton_orange->setChecked(true);

    refresh();
}

void StandingManagerForm::refreshPlayerList() {
    ui->comboBox_players->clear();
    for (Character* c: Characters::VALUES) ui->comboBox_players->addItem(c->getName());
}

void StandingManagerForm::refreshHead() {
    if (ui->comboBox_players->count() == 0) ui->label_portrait->clear();
    else {
        Character *c = Characters::fromName(ui->comboBox_players->currentText());
        if (c == nullptr) ui->label_portrait->clear();
        else {
            ui->label_portrait->setPixmap(c->getPortrait());
        }
    }
}

void StandingManagerForm::refreshAlliances() {
    ui->listWidget_alliances->clear();
    if (ui->lineEdit_allianceSearch->text().length() > 2) {
        QList<Alliance> all = EsiManager::searchAlliances(ui->lineEdit_allianceSearch->text(), [] (const QString& err) { Util::error(err); });
        for (Alliance a: all) ui->listWidget_alliances->addItem(a.name);
    }
}

void StandingManagerForm::refreshOutput() {
    ui->listWidget_toRemove->clear();
    ui->listWidget_toAdd->clear();

    m_toRemove.clear();
    m_toAdd.clear();

    Character* c = Characters::fromName(ui->comboBox_players->currentText());
    if (c->hasConnector() && c->getConnector()->isLoggedIn()) {
        c->getConnector()->fetchAllianceStandings([this] (const QList<AllianceStandings>& standings) {
            Util::println("Filling the lists");
            QMap<qint64, double> work = {};
            for (const AllianceStandings& s: standings) work.insert(s.id, s.standing);
            bool ok = false;
            QSqlQuery query = EsiManager::requestERP("SELECT * FROM wars WHERE agressorId = :id OR victimId = :id", {{"id", getCurrentAlliance().id}}, &ok);
            double wantedStandings = getWantedStandings();
            if (!ok) {
                Util::error("Unable to read wars from ERP.");
            } else {
                while (query.next()) {
                    qint64 otherId = query.value("victimId").toLongLong() == getCurrentAlliance().id ? query.value("agressorId").toLongLong() : query.value("victimId").toLongLong();
                    bool add = true;

                    if (work.contains(otherId)) {
                        if (work.value(otherId) == wantedStandings) add = false;
                        work.remove(otherId);
                    }
                    if (add) {
                        Alliance w = EsiManager::getAllianceFromId(otherId);
                        if (w.isNull()) {
                            Util::error("Alliances list not up to date. Id not found " + QString::number(otherId));
                            continue;
                        } else {
                            ui->listWidget_toAdd->addItem(w.name);
                            m_toAdd[w.id] = wantedStandings;
                        }
                    }
                }
                for (const auto& [k, v]: work.asKeyValueRange()) {
                    Alliance w = EsiManager::getAllianceFromId(k);
                    if (w.isNull()) {
                        Util::error("Alliances list not up to date. Id not found " + QString::number(k));
                        continue;
                    } else {
                        ui->listWidget_toRemove->addItem(w.name);
                        m_toRemove.append(w.id);
                    }
                }
            }
        });
    } else {
        Util::error("Character not logged");
    }
}

double StandingManagerForm::getWantedStandings() {
    if (ui->radioButton_orange->isChecked()) return -5;
    if (ui->radioButton_red->isChecked()) return -10;
    if (ui->radioButton_neutral->isChecked()) return 0;
    if (ui->radioButton_blue5->isChecked()) return 5;
    if (ui->radioButton_blue10->isChecked()) return 10;
    else return 0;
}

void StandingManagerForm::on_pushButton_searchAlliance_clicked()
{
    refreshAlliances();
}


void StandingManagerForm::on_comboBox_players_activated(int index)
{
    refreshHead();
}


void StandingManagerForm::on_pushButton_reset_clicked()
{
    reset();
}


void StandingManagerForm::on_listWidget_alliances_itemClicked(QListWidgetItem *item)
{
    setCurrentAlliance(EsiManager::getAllianceFromExactName(item->text()));
}

void StandingManagerForm::setCurrentAlliance(Alliance a) {
    this->alliance = a;
    if (getCurrentAlliance().isNull()) ui->label_allianceName->clear();
    else ui->label_allianceName->setText(getCurrentAlliance().name);
}

void StandingManagerForm::on_pushButton_submit_clicked()
{
    refreshOutput();
}


void StandingManagerForm::on_pushButton_write_clicked()
{
    Character *c = Characters::fromName(ui->comboBox_players->currentText());
    if (c->hasConnector() && c->getConnector()->isLoggedIn()) {
        c->getConnector()->removeAllianceContacts(m_toRemove, [] () { Util::println("Successfully cleared contacts"); }, Util::error);
        c->getConnector()->writeAllianceStandings(m_toAdd, Util::error);
    } else {
        Util::error("Not logged in :(");
    }
}

