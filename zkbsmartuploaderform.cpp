#include "zkbsmartuploaderform.h"
#include "core.h"
#include "ui_zkbsmartuploaderform.h"
#include "esimanager.h"

ZkbSmartUploaderForm::ZkbSmartUploaderForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ZkbSmartUploaderForm)
{
    ui->setupUi(this);

    reset();
}

ZkbSmartUploaderForm::~ZkbSmartUploaderForm()
{
    delete ui;
}

void ZkbSmartUploaderForm::reset() {
    resetCharacters();

    ui->checkBox_deepFetch->setChecked(false);
    ui->checkBox_includeDeaths->setChecked(false);
    ui->checkBox_includeKills->setChecked(true);
    ui->checkBox_includeSameAlliance->setChecked(false);

    ui->listWidget_filters->clear();
    ui->listWidget_killmails->clear();
}

void ZkbSmartUploaderForm::resetCharacters() {
    ui->comboBox_player->clear();
    for (Character* c: Characters::VALUES) ui->comboBox_player->addItem(c->getName());
}

void ZkbSmartUploaderForm::refreshOutput() {
    Character* c = Characters::fromName(ui->comboBox_player->currentText());

    if (c != nullptr && c->hasConnector() && c->getConnector()->isLoggedIn()) {
        c->getConnector()->fetchRecentKillmails([this] (const QList<KillmailReference>& refs) {
            for (const KillmailReference &ref: refs) {
                    EsiManager::isKillmailOnZkillboard(ref.id, [this, ref] (bool b) {
                    if (!b) EsiManager::fetchKillmail(ref.id, ref.hash, [this] (const Killmail& k) {
                            addKillmailView(k);
                        });
                    });
            }
        });
    }
}

void ZkbSmartUploaderForm::on_pushButton_fetchKills_clicked()
{
    refreshOutput();
}

