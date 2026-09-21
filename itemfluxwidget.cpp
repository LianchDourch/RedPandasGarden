#include "itemfluxwidget.h"

ItemFluxWidget::ItemFluxWidget(QWidget *parent)
    : QWidget{parent}
{
    new QVBoxLayout(this);
    layout()->setContentsMargins(2, 2, 2, 2);
}
