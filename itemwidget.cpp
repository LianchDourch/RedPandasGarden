#include "itemwidget.h"

ItemWidget::ItemWidget(QWidget *parent)
    : QWidget{parent}
{

}

void ItemWidget::setItem(const ItemStack& itemStack) {
    this->itemStack = itemStack;
    onItemSet();
    refresh();
}
