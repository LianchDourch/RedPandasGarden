#include "redpandasmarketform.h"
#include "core.h"
#include "redpandasgarden.h"
#include "ui_redpandasmarketform.h"
#include "esimanager.h"
#include <qheaderview.h>
#include <qinputdialog.h>
#include <qmessagebox.h>

RedPandasMarketForm::RedPandasMarketForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::RedPandasMarketForm)
{
    ui->setupUi(this);

    connect(ui->comboBox_characters, &QComboBox::activated, this, [this] () {
        setCharacter(Characters::fromName(ui->comboBox_characters->currentText()));
        resetTree();
        resetSellableTable();
    });

    new QVBoxLayout(ui->widget_content);
    ui->widget_content->layout()->setContentsMargins(0, 0, 0, 0);
    splitter = new QSplitter;
    ui->widget_content->layout()->addWidget(splitter);

    canLocations = new QTreeWidget;
    splitter->addWidget(canLocations);
    canLocations->setColumnCount(1);
    canLocations->header()->hide();
    connect(canLocations, &QTreeWidget::itemDoubleClicked, this, [this] (QTreeWidgetItem* item) {
        Util::println("Clicked !");
        QString name = item->text(0);
        if (hasCharacter()) {
            Util::println("Working =)");
            currentContainer = EsiManager::loadContainer(name, getCharacter()->getPlayerId());
            Util::println("Name was ", name, " and container is null ? ", currentContainer.isNull());
            resetSellableTable();
        }
    });

    sellableItems = new RepamListWidget();
    splitter->addWidget(sellableItems);

    resetTree();
    resetSellableTable();
}

RedPandasMarketForm::~RedPandasMarketForm()
{
    delete ui;
}

void RedPandasMarketForm::refreshMutableLists() {
    ui->comboBox_characters->clear();
    for (Character* c: Characters::VALUES) ui->comboBox_characters->addItem(c->getName());

    resetTree();
    resetSellableTable();
}

void RedPandasMarketForm::resetTree() {
    canLocations->clear();
    if (!hasCharacter()) Util::error("No character");
    else {
        bool ok = false;
        QSqlQuery query = EsiManager::requestERP("SELECT * FROM erpmarketcontainers WHERE characterId = :charId", {{"charId", character->getPlayerId()}}, &ok);
        if (!ok) Util::error("Unable to get containers.");
        else {
            QQueue<QString> containers = {};
            while (query.next()) {
                containers.enqueue(query.value("containerName").toString());
            }

            for (const QString& name: containers) {
                (new QTreeWidgetItem(canLocations))->setText(0, name);
            }
        }
    }
}

void RedPandasMarketForm::resetSellableTable() {
    sellableItems->clear();
    if (currentContainer.isNull()) return;

    for (const CharacterAsset &asset: currentContainer.contents) {
        sellableItems->addItem(asset);
    }
}

void RedPandasMarketForm::on_pushButton_viewOrders_clicked()
{
    if (!hasCharacter()) Util::error("Character not found");
    else {
        if (getCharacter()->hasConnector() && getCharacter()->getConnector()->isLoggedIn()) {
            Util::println("Going for the search");
            RedPandasGarden::fetchRepamContainers(getCharacter()->getConnector(), [] (const QList<Container>& list) {
                Util::println("Received: ", list.size());
                for (const Container &c: list) {
                    Util::println("\t", c.name, " ", c.typeId, " with ", c.contents.size(), " item(s) inside.");
                }
            });
        } else Util::error("char not logged");
    }
}


void RedPandasMarketForm::on_pushButton_addOrder_clicked()
{

}


void RedPandasMarketForm::on_pushButton_reset_clicked()
{
    refreshMutableLists();
}



void RedPandasMarketForm::on_pushButton_tickall_clicked()
{
    sellableItems->setWholeCheckState(true);
}


void RedPandasMarketForm::on_pushButton_unselectAll_clicked()
{
    sellableItems->setWholeCheckState(false);
}


void RedPandasMarketForm::on_pushButton_upload_clicked()
{
    if (!hasCharacter() || !getCharacter()->hasConnector() || !getCharacter()->getConnector()->isLoggedIn()) {
        QMessageBox::critical(this, "REPAM Upload Failed", "You must be on a logged character to create a sell order.");
        return;
    }
    QString token = getCharacter()->getConnector()->getRepamToken();
    for (RepamItemWidget* w: sellableItems->getItemsView()) {
        if (w->getCheckBox()->isChecked()) {
            if (w->getItemWidget()->getItem()->isBlueprint()) {
                BlueprintAsset asset = EsiManager::loadBlueprint(w->getAsset().itemId, getCharacter()->getPlayerId());
                RedPandasGarden::createBlueprintSellOrder(
                    token,
                    w->getAsset().typeId,
                    w->getAsset().itemId, 0,
                    w->getActionWidget()->getPriceSpecify()->isChecked() ? std::make_shared<AbsoluteItemPrice>(w->getActionWidget()->getPriceEdit()->text().toDouble()) : std::make_shared<ItemPrice>(),
                    w->getAsset().quantity, w->getActionWidget()->getDescription(),
                    asset.materialEfficiency,
                    asset.timeEfficiency,
                    asset.bpc,
                    asset.runsRemaining
                    );
            } else {
                Util::println("Item ", w->getItemWidget()->getItem()->getName(), " wasn't a bp");
                RedPandasGarden::createSellOrder(
                token,
                w->getAsset().typeId,
                w->getAsset().itemId, 0,
                w->getActionWidget()->getPriceSpecify()->isChecked() ? std::make_shared<AbsoluteItemPrice>(w->getActionWidget()->getPriceEdit()->text().toDouble()) : std::make_shared<ItemPrice>(),
                w->getAsset().quantity, w->getActionWidget()->getDescription());
            }
        }
    }
}

