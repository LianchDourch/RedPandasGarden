#include "loadingform.h"
#include "ui_loadingform.h"

LoadingForm::LoadingForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LoadingForm)
{
    ui->setupUi(this);

    setWindowTitle("Loading...");
}

LoadingForm::~LoadingForm()
{
    delete ui;
}
