#ifndef ITEMSTREAMSMANAGERFORM_H
#define ITEMSTREAMSMANAGERFORM_H

#include "productionnodes.h"
#include <QWidget>
#include <QListWidget>
#include <QListWidgetItem>

namespace Ui {
class ItemStreamsManagerForm;
}

struct TempItemDatas {
    ItemStack item;
    QList<int> hierarchy;
};

class ItemStreamsManagerForm : public QWidget
{
    Q_OBJECT

    FULL_PROPERTY_PTR(ProductionChain*, chain, nullptr, getChain, setChain, hasChain);
    GENERAL_PROPERTY_POD(int, nodeLocalId, 0, getNodeLocalId, setNodeLocalId);
public:
    explicit ItemStreamsManagerForm(QWidget *parent = nullptr);
    ~ItemStreamsManagerForm();

    inline void setNode(ProductionNode* node) {
        if (node == nullptr) {
            this->chain = nullptr;
            this->nodeLocalId = -1;
        } else {
            if (!node->hasProductionChain()) {
                Util::error("No chain in the node for ItemStreamsManagerForm =(");
                return;
            }
            this->chain = node->getProductionChain();
            this->nodeLocalId = node->getLocalId();
        }
        reset();
    }
    void reset();
    void setCurrentItemStack(const ItemStackDatas& item, ProductionNode* node = nullptr);
    void showSlotSelection(int nodeId, int slotIndex);
    void hideSlotSelection();

    void submit();

    inline bool hasNode() const { return hasChain(); }
    inline ProductionNode* getNode() const { return getChain()->get(getNodeLocalId()); }
private:
    Ui::ItemStreamsManagerForm *ui;
    QList<ItemStackDatas> displayed;
    QMap<ItemStackDatas, QList<ItemStreamSlot>> editedHierarchies;
    ItemStackDatas currentItem;
    int editedSlotIndex = -1;

signals:
    void streamSaved();
    void streamCancelled();
private slots:
    void on_listWidget_connectedNodes_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void on_listWidget_connectedNodes_itemDoubleClicked(QListWidgetItem *item);
    void on_hierarchy_rowsMoved();
    void on_listWidget_hierarchy_itemDoubleClicked(QListWidgetItem *item);
    void on_pushButton_reset_clicked();
    void on_pushButton_save_clicked();
    void on_listWidget_itemList_itemClicked(QListWidgetItem *item);
    void on_listWidget_hierarchy_itemClicked(QListWidgetItem *item);
    void on_listWidget_properties_itemClicked(QListWidgetItem *item);
};

#endif // ITEMSTREAMSMANAGERFORM_H
