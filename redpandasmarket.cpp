#include "redpandasmarket.h"

using namespace RedPandasMarket;

ApplicationWidget::ApplicationWidget(QWidget* parent) : QWidget{parent} {

}

void RepamListWidget::setWholeCheckState(bool checked) {
    for (RepamItemWidget* w: getItemsView()) {
        if (w->getCheckBox()->isChecked() != checked) w->getCheckBox()->setChecked(checked);
    }
}
