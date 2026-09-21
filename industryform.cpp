#include "industryform.h"
#include "ui_industryform.h"
#include <QSplitter>
#include "esimanager.h"

IndustryForm::IndustryForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::IndustryForm)
{
    ui->setupUi(this);

    QWidget* a = ui->widget_pasteMaterials;
    QWidget* b = ui->widget_viewMaterials;

    a->setParent(nullptr);
    b->setParent(nullptr);

    ui->label_fetchingBlueprint->hide();

    QSplitter* work = new QSplitter(ui->widget_materials);
    ui->widget_materials->layout()->addWidget(work);
    work->addWidget(a);
    work->addWidget(b);
    a->hide();
    work->setSizes(QList<int>({2500, 7500}));

    refreshStationsList();

    std::function<QPixmap(Item*)> blueprintGetter = [this] (Item* item) {
        if (ui->radioButton_bpo->isChecked()) return item->getIcon();
        else return dynamic_cast<Blueprint*>(item)->getBpcIcon();
    };

    ui->widget_bpItemView->layout()->setContentsMargins(0, 0, 0, 0);
    blueprintItemWidget = new IconOnlyItemWidget(ui->widget_blueprint);
    blueprintItemWidget->setBackground(QColor("#777777"));
    ui->widget_bpItemView->layout()->addWidget(blueprintItemWidget);
    blueprintItemWidget->setIconGetter(blueprintGetter);

    outputItemWidget = new LinedItemWidget(ui->widget_outputItemWidgetContainer);
    ui->widget_outputItemWidgetContainer->layout()->addWidget(outputItemWidget);
    outputItemWidget->setBackground(QColor("#777777"));

    ui->tableWidget_viewMaterials->setColumnCount(8);
    ui->tableWidget_viewMaterials->setHorizontalHeaderLabels({"Icon", "Name", "Quantity", "Sell price", "Buy price", "Total Quantity", "Total Sell Price", "Total Buy Price"});
    ui->tableWidget_viewMaterials->setShowGrid(false);
    ui->tableWidget_viewMaterials->verticalHeader()->hide();
    ui->tableWidget_viewMaterials->verticalHeader()->setDefaultSectionSize(32);
    ui->tableWidget_viewMaterials->setColumnWidth(0, 32);

    setBlueprint(nullptr);
}

Station* IndustryForm::getBuyingHub() {
    return Stations::fromName(ui->comboBox_buyingstation->currentText());
}

Station* IndustryForm::getSellingHub() {
    return Stations::fromName(ui->comboBox_sellingstation->currentText());
}

Station* IndustryForm::getFacility() const {
    return Stations::fromName(ui->comboBox_facility->currentText());
}

void IndustryForm::setItem(const ItemStack &itemStack, int row, std::function<void(Item *)> then) {
    if (ui->tableWidget_viewMaterials->rowCount() <= row) {
        ui->tableWidget_viewMaterials->setRowCount(row+1);
    }
    if (itemStack.getItem() == nullptr) return;

    for (int i = 0; i < ui->tableWidget_viewMaterials->columnCount(); i++) ui->tableWidget_viewMaterials->setItem(row, i, new QTableWidgetItem("..."));

    ui->tableWidget_viewMaterials->setItem(row, 1, new QTableWidgetItem(itemStack.getItem()->getName()));

    Station *buyingHub = getBuyingHub();
    Station *sellingHub = getSellingHub();
    int runcount = ui->spinBox_runCount->value();
    itemStack.getItem()->fetchDatas(
        getBuyingHub(), [this, buyingHub, sellingHub, row, itemStack, runcount, then](Item *item) {
            QTableWidgetItem *itemWidget = nullptr;

            QLabel *iconLabel = new QLabel;
            iconLabel->setPixmap(item->getIcon());
            iconLabel->setAlignment(Qt::AlignCenter);
            iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
            iconLabel->setScaledContents(true);
            ui->tableWidget_viewMaterials->setCellWidget(row, 0, iconLabel);

            itemWidget = new QTableWidgetItem(QString::number(itemStack.getQuantity()));
            itemWidget->setTextAlignment(Qt::AlignCenter);
            ui->tableWidget_viewMaterials->setItem(row, 2, itemWidget);

            itemWidget = new QTableWidgetItem(QString::number(itemStack.getQuantity() * runcount));
            itemWidget->setTextAlignment(Qt::AlignCenter);
            ui->tableWidget_viewMaterials->setItem(row, 5, itemWidget);

            itemWidget = new QTableWidgetItem(
                item->hasOrder(buyingHub, true, 0)
                    ? moneyToString(item->getPrice(buyingHub, true, 0), true)
                    : "Unknown");
            itemWidget->setTextAlignment(Qt::AlignCenter);
            ui->tableWidget_viewMaterials->setItem(row, 3, itemWidget);

            itemWidget = new QTableWidgetItem(
                item->hasOrder(buyingHub, false, 0)
                    ? moneyToString(item->getPrice(buyingHub, false, 0), true)
                    : "Unknown");
            itemWidget->setTextAlignment(Qt::AlignCenter);
            ui->tableWidget_viewMaterials->setItem(row, 4, itemWidget);

            item->priceFor(buyingHub, itemStack.getQuantity() * runcount, [this, row, buyingHub, then, itemStack, runcount, item] (double value) {
                QTableWidgetItem* itemWidget = new QTableWidgetItem(moneyToString(value));
                itemWidget->setTextAlignment(Qt::AlignCenter);
                ui->tableWidget_viewMaterials->setItem(row, 6, itemWidget);

                item->priceFor(buyingHub, itemStack.getQuantity() * runcount, [this, row, then, item] (double value) {
                    QTableWidgetItem* itemWidget = new QTableWidgetItem(moneyToString(value));
                    itemWidget->setTextAlignment(Qt::AlignCenter);
                    ui->tableWidget_viewMaterials->setItem(row, 7, itemWidget);

                    then(item);
                }, false);
            }, true);
        });
}

void IndustryForm::setValue(QLabel* label, double money) {
    label->setStyleSheet("color: " + QColor(money == 0 ? Qt::gray : (money > 0 ? Qt::green : Qt::red)).name() + "; font: 700 11pt \"Segoe UI\";");
    label->setText(moneyToString(money, true));
}

void IndustryForm::setBlueprint(Blueprint* blueprint) {
    _setBlueprint(blueprint);
    blueprintItemWidget->setItem(blueprint);
    ui->lineEdit_bpName->clear();

    if (blueprint == nullptr) {
        setMaterials({});
        outputItemWidget->setItem(nullptr);
    } else {
        if (!setMaterials(blueprint->getRecipeInput())) {
            Util::error("WARNING, couldn't set materials as it is still fetching");
        }

        blueprint->getOutput().getItem()->fetchDatas(getBuyingHub(), [this, blueprint] (Item* item) {
            outputItemWidget->setItem(blueprint->getOutput());
        });

        ui->lineEdit_bpName->setText(blueprint->getName());
    }
    ui->label_fetchingBlueprint->hide();
}

bool IndustryForm::setMaterials(const QMap<ItemStackDatas, int>& map) {
    if (materialFetchCounter != nullptr) return false;
    _setMaterials(map);
    return true;
}

void IndustryForm::refreshJob() {
    if (!hasBlueprint() || !getBlueprint()->isLoaded()) {
        ui->label_jobtime->setText("");
        ui->label_jobcost->setText("");
        ui->label_materialscost->setText("");
        ui->label_outputprice->setText("");
        ui->label_jobbenefit->setText("");
        return;
    }

    bool sellViaOrders = !ui->checkBox_sellImmediately->isChecked();
    bool buyImmediately = !ui->checkBox_buyWithOrders->isChecked();
    Station* buyingHub = getBuyingHub();
    Station* sellingHub = getSellingHub();
    double materialEfficiency = 1. - static_cast<double>(ui->doubleSpinBox_materialEfficiency->value()) * 0.01;
    int runcount = ui->spinBox_runCount->value();
    getBlueprint()->fetchManufacturingInformations(
        buyingHub, sellingHub, getFacility(), getCharacter(), runcount, materialEfficiency * (getFacility() == nullptr ? 1 : getFacility()->getMaterialBonuses().getManufacturing()),
        1. - static_cast<double>(ui->doubleSpinBox_timeEfficiency->value()) * 0.01, sellViaOrders, buyImmediately, [this, materialEfficiency, buyingHub, sellingHub] (double totalMaterialsPrice, double totalJobCost, double totalOutputPrice, Duration duration) {
            setValue(ui->label_totalMaterialsPrice, - totalMaterialsPrice);
            setValue(ui->label_totalJobPrice, - totalJobCost);
            setValue(ui->label_chiffredAffaire, totalOutputPrice);
            setValue(ui->label_totalInvestedMoney, - totalJobCost - totalMaterialsPrice);
            ui->label_totalTimeRequired->setText((duration * (hasFacility() ? getFacility()->getDurationBonuses().getManufacturing() : 1)).toQString());
            setValue(ui->label_netBenefits, totalOutputPrice - totalJobCost - totalMaterialsPrice);
            double singleMatPrice = getBlueprint()->getTotalMaterialsPrice(buyingHub, materialEfficiency);
            setValue(ui->label_materialscost, -singleMatPrice);
            double singleOutputPrice = getBlueprint()->getOutput().getItem()->priceFor(sellingHub, getBlueprint()->getOutput().getQuantity(), true);
            setValue(ui->label_outputprice, singleOutputPrice);
            setValue(ui->label_jobbenefit, singleOutputPrice - singleMatPrice);
        });

}

void IndustryForm::onMaterialsFetchEnd() {
    refreshJob();
}

void IndustryForm::_setMaterials(const QMap<ItemStackDatas, int>& items) {
    ui->tableWidget_viewMaterials->clearContents();
    ui->tableWidget_viewMaterials->setRowCount(0);
    QString work = "";

    if (items.size() > 0) materialFetchCounter = new int(items.size());

    for (const auto& [datas, qt]: items.asKeyValueRange()) {
        addItem({datas, qt}, [this] (Item* item) {
            *materialFetchCounter -= 1;
            if ((*materialFetchCounter) <= 0) {
                delete materialFetchCounter;
                materialFetchCounter = nullptr;
                ui->tableWidget_viewMaterials->resizeColumnsToContents();
                ui->tableWidget_viewMaterials->setColumnWidth(0, 32);
                onMaterialsFetchEnd();
            }
        });
        work += QString::number(qt) + " x " + datas.getItem()->getName() + "\n";
    }
    ui->plainTextEdit_materials->setPlainText(work);
}

void IndustryForm::refreshStationsList() {
    QString previousBuyingHub = ui->comboBox_buyingstation->currentText();
    QString previousFacility = ui->comboBox_facility->currentText();
    QString previousSellingHub = ui->comboBox_facility->currentText();
    ui->comboBox_buyingstation->clear();
    ui->comboBox_facility->clear();
    ui->comboBox_sellingstation->clear();
    for (Station* station: Stations::VALUES) {
        ui->comboBox_buyingstation->addItem(station->getName());
        ui->comboBox_facility->addItem(station->getName());
        ui->comboBox_sellingstation->addItem(station->getName());
    }

    ui->comboBox_buyingstation->setCurrentText(previousBuyingHub);
    ui->comboBox_sellingstation->setCurrentText(previousSellingHub);
    ui->comboBox_facility->setCurrentText(previousFacility);
}

void IndustryForm::refreshCharactersList() {
    QString previous = ui->comboBox_character->currentText();
    ui->comboBox_character->clear();
    for (Character* c: Characters::VALUES) ui->comboBox_character->addItem(c->getName());
    ui->comboBox_character->setCurrentText(previous);
}

Character* IndustryForm::getCharacter() {
    return Characters::fromName(ui->comboBox_character->currentText());
}

void IndustryForm::setBlueprintWaitingScreen() {
    ui->label_fetchingBlueprint->show();
}

bool IndustryForm::setBlueprint(const QString& itemname) {
    if (EsiManager::isBlueprint(itemname)) {
        Blueprint* blueprint = dynamic_cast<Blueprint*>(Items::fromName(itemname));
        setBlueprintWaitingScreen();
        blueprint->fetchDatas(getBuyingHub(), [this, blueprint] (Item* item) { setBlueprint(blueprint); });
        return true;
    } else {
        setBlueprint(nullptr);
        return false;
    }
}

void IndustryForm::addItem(const ItemStack& item, std::function<void(Item *)> then) {
    setItem(item, ui->tableWidget_viewMaterials->rowCount(), then);
}

IndustryForm::~IndustryForm()
{
    delete ui;
}

void IndustryForm::refreshMaterials() {
    QString str = ui->plainTextEdit_materials->toPlainText();

    Items::parseItemsAndLoad(str, getBuyingHub(), [this] (const QMap<ItemStackDatas, int>& map) { setMaterials(map); });
}

void IndustryForm::on_pushButton_submitMaterials_clicked()
{
    refreshMaterials();
}


void IndustryForm::on_pushButton_refreshMaterials_clicked()
{

}


void IndustryForm::on_pushButton_submitBp_clicked()
{
    QString work = ui->lineEdit_bpName->text();
    if (!setBlueprint(work) && !work.contains("Blueprint")) {
        setBlueprint(work.trimmed() + " Blueprint");
    }
}

