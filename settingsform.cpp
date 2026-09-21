#include "settingsform.h"
#include "esimanager.h"
#include "redpandasgarden.h"
#include "ui_settingsform.h"
#include <QInputDialog>
#include <QMessageBox>
#include "mainwindow.h"

SettingsForm::SettingsForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SettingsForm)
{
    ui->setupUi(this);
}

SettingsForm::~SettingsForm()
{
    delete ui;
}

void SettingsForm::on_pushButton_loadAlliances_clicked()
{
    EsiManager::loadAlliances([] () {});
}

void SettingsForm::refresh() {

}

void SettingsForm::on_pushButton_loadWars_clicked()
{
    EsiManager::loadWars([] () {});
}


void SettingsForm::on_pushButton_loadRepam_clicked()
{
    QMap<QString, Character*> map;
    for (Character *c: Characters::VALUES) map.insert(c->getName(), c);
    Character* c = map.value(QInputDialog::getItem(MainWindow::INSTANCE, "Load REPAM", "Choose the character to load", map.keys()), nullptr);
    if (c == nullptr) QMessageBox::critical(MainWindow::INSTANCE, "REPAM Load Fail", "This character isn't registered");
    else {
        if (c->hasConnector() && c->getConnector()->isLoggedIn()) {
            RedPandasGarden::fetchRepamContainers(c->getConnector(),
                [c] (const QList<Container>&) { QMetaObject::invokeMethod(
                                         qApp,
                                                    [c] () { QMessageBox::information(MainWindow::INSTANCE, "REPAM load successful", "Successfully loaded REPAM for " + c->getName()); },
                                         Qt::QueuedConnection
                                         ); },
                [c] (const QString& err) { QMetaObject::invokeMethod(
                                         qApp,
                                              [c, err] () { QMessageBox::critical(MainWindow::INSTANCE, "REPAM Load Fail", "Unable to load REPAM:\n\t" + err); },
                                         Qt::QueuedConnection
                                         ); });
        } else {
            QMessageBox::critical(MainWindow::INSTANCE, "REPAM Load Fail", "This character isn't logged in.");
        }
    }

}


void SettingsForm::on_pushButton_clearCache_clicked()
{
    EsiManager::clearCache();
}

