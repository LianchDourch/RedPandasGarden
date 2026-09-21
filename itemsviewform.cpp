#include "itemsviewform.h"
#include "ui_itemsviewform.h"

ItemsViewForm::ItemsViewForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ItemsViewForm)
{
    ui->setupUi(this);

    ui->tableWidget_viewItems->setColumnCount(3);
}

ItemsViewForm::~ItemsViewForm()
{
    delete ui;
}

void ItemsViewForm::refreshReadOnly() {
    ui->widget_edit->setVisible(!isReadOnly());
}
