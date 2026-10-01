#include "itemstreamsmanagerform.h"
#include "ui_itemstreamsmanagerform.h"

ItemStreamsManagerForm::ItemStreamsManagerForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ItemStreamsManagerForm)
{
    ui->setupUi(this);
    setWindowTitle("Item Flux Manager");

    connect(ui->listWidget_hierarchy->model(), &QAbstractListModel::rowsMoved, this, &ItemStreamsManagerForm::on_hierarchy_rowsMoved);
}

ItemStreamsManagerForm::~ItemStreamsManagerForm()
{
    delete ui;
}

void ItemStreamsManagerForm::reset() {
    ui->label_itemName->clear();
    ui->listWidget_itemList->clear();
    displayed.clear();
    editedHierarchies.clear();
    ui->listWidget_properties->clear();
    currentItem = ItemStackDatas();

    if (hasNode()) {
        ProductionNode* node = getNode();
        ui->label_nodeLabel->setText(node->getName());
        for (const ItemStack& item: node->getOutputs()) {
            ui->listWidget_itemList->addItem(item.getItem()->getName());
            if (!currentItem.isValid()) {
                currentItem = item.getDatas();
            }
            displayed.append(item.getDatas());
        }
        for (const ItemStackDatas& itemStack: node->getItemStreamPtr()->getTransittingItems()) {
            editedHierarchies[itemStack] = node->getItemStreamPtr()->getHierarchy(itemStack);
        }


        setCurrentItemStack(currentItem, node);
    } else {
        ui->label_nodeLabel->clear();
    }
}

void ItemStreamsManagerForm::setCurrentItemStack(const ItemStackDatas& item, ProductionNode* node) {
    this->currentItem = item;

    int i = 0;
    for (const ItemStackDatas& d: displayed) {
        if (d == item) {
            ui->listWidget_itemList->setCurrentItem(ui->listWidget_itemList->item(i));
            break;
        }
        i++;
    }

    if (node == nullptr) node = getNode();
    QList<ItemStreamSlot> hierarchy = editedHierarchies.value(item, {});
    ui->listWidget_connectedNodes->clear();
    ui->listWidget_hierarchy->clear();
    int index;
    for (const NodeConnection& conn: node->getChildren()) {
        ui->listWidget_connectedNodes->addItem(conn.getChild()->getUniqueName());
    }
    Util::println("Displaying a hierarchy of ", hierarchy.size(), " elements.");
    for (const ItemStreamSlot& slot: hierarchy) {
        ui->listWidget_hierarchy->addItem(slot.getName());
    }
}

void ItemStreamsManagerForm::on_listWidget_connectedNodes_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{

}

void ItemStreamsManagerForm::on_hierarchy_rowsMoved() {
    QList<ItemStreamSlot>& hierarchy = editedHierarchies[currentItem];

    for (int i = 0; i < ui->listWidget_hierarchy->count(); i++) {
        ItemStreamSlot v = ItemStreamSlot::fromName(ui->listWidget_hierarchy->item(i)->text());

        hierarchy[i] = v;
    }
}

void ItemStreamsManagerForm::on_listWidget_connectedNodes_itemDoubleClicked(QListWidgetItem *item)
{
    if (!hasNode()) return;
    int nodeId = ProductionNode::getLocalIdFromUniqueName(item->text());

    ProductionNode* child = getChain()->get(nodeId);
    if (child == nullptr) Util::error("Error ! Child is nullptr in itemstreamsmanagerform add");
    else {
        ItemStreamSlot slot = {nodeId, child->getAnyProperty()};
        ui->listWidget_hierarchy->addItem(slot.getName(child->getName()));
        editedHierarchies[currentItem].append(slot);
        showSlotSelection(nodeId, ui->listWidget_hierarchy->count() - 1);
    }
}


void ItemStreamsManagerForm::on_listWidget_hierarchy_itemDoubleClicked(QListWidgetItem *item)
{
    if (!hasNode()) return;
    ItemStreamSlot slot = ItemStreamSlot::fromName(item->text());
    ProductionNode* child = getChain()->get(slot.nodeLocalId);
    if (child == nullptr) Util::error("Error ! Child is nullptr in itemstreamsmanagerform delete");
    else {
        editedHierarchies[currentItem].removeAll(slot);
        hideSlotSelection();
        delete ui->listWidget_hierarchy->takeItem(ui->listWidget_hierarchy->row(item));
    }
}


void ItemStreamsManagerForm::on_pushButton_reset_clicked()
{
    reset();
}


void ItemStreamsManagerForm::on_pushButton_save_clicked()
{
    submit();
    hide();
}


void ItemStreamsManagerForm::on_listWidget_itemList_itemClicked(QListWidgetItem *item)
{
    setCurrentItemStack(displayed[ui->listWidget_itemList->row(item)]);
}


void ItemStreamsManagerForm::hideSlotSelection() {
    ui->listWidget_properties->clear();
    editedSlotIndex = -1;
}


void ItemStreamsManagerForm::showSlotSelection(int nodeId, int slotIndex) {
    if (!hasNode()) return;
    ProductionNode* node = getChain()->get(nodeId);
    editedSlotIndex = slotIndex;
    ui->listWidget_properties->clear();
    for (ProductionNodeProperty* prop: node->getProperties()) ui->listWidget_properties->addItem(prop->getPropertyName());
}


void ItemStreamsManagerForm::submit() {
    if (!hasNode()) {
        Util::error("No Node Defined for Item Stream Manager");
        return;
    }
    ProductionNode* node = getNode();
    ItemStream* work = node->getItemStreamPtr();
    QList<ItemStreamSlot> res = {};
    Util::println("Uploading hierarchies:");
    for (const auto& [k, v]: editedHierarchies.asKeyValueRange()) {
        if (!k.isValid()) {
            Util::println(" -> Invalid itemstack with ", v.size(), " piece of data inside.");
            continue;
        }
        Util::println(" -> ", k.getItem()->getName());
        QQueue<ItemStreamSlot> w = {};
        for (const ItemStreamSlot& s: v) {
            Util::println("\t |--- ", s.getName(node->getName()));
            w.enqueue(s);
        }
        work->setHierarchy(k, w);
        Util::println("\t \\ ", work->getHierarchy(k).size(), " elements.");
    }
    Util::println("End of work");
    emit streamSaved();
    getChain()->notifyIOUpdate();
    Util::println("Returning");
}

void ItemStreamsManagerForm::on_listWidget_hierarchy_itemClicked(QListWidgetItem *item)
{
    showSlotSelection(ItemStreamSlot::fromName(item->text()).nodeLocalId, ui->listWidget_hierarchy->row(item));
}


void ItemStreamsManagerForm::on_listWidget_properties_itemClicked(QListWidgetItem *item)
{
    editedHierarchies[currentItem][editedSlotIndex].port = ProductionNodeProperties::fromName(item->text());
    ItemStreamSlot slot = editedHierarchies[currentItem][editedSlotIndex];
    ui->listWidget_hierarchy->item(editedSlotIndex)->setText(slot.getName(getChain()->get(slot.nodeLocalId)->getName()));
}

