#ifndef ITEMFLUXWIDGET_H
#define ITEMFLUXWIDGET_H

#include <QWidget>
#include "core.h"

class ItemFluxWidgetItem : public QWidget {
    Q_OBJECT
private:
    ItemStack item;
public:
    ItemFluxWidgetItem(ItemStack item = ItemStack(), QWidget* parent = nullptr) : QWidget{parent}, item(item) {

    }
};

class ItemFluxWidget : public QWidget
{
    Q_OBJECT
private:
    QList<ItemFluxWidgetItem*> items;

public:
    explicit ItemFluxWidget(QWidget *parent = nullptr);
    ~ItemFluxWidget() {
    }

    ItemFluxWidgetItem* addItem(const ItemStack& item) {
        ItemFluxWidgetItem* w = new ItemFluxWidgetItem(item);
        addItem(w);
        return w;
    }

    void addItem(ItemFluxWidgetItem* item) {
        item->setParent(this);

        layout()->addWidget(item);
    }

    void removeItem(ItemFluxWidgetItem* item) {
        items.removeAll(item);
        item->deleteLater();
    }

    ItemFluxWidgetItem* item(int index) {
        return items[index];
    }

    void clearContents() {
        for (ItemFluxWidgetItem* item: items) removeItem(item);
    }
signals:
};

#endif // ITEMFLUXWIDGET_H
