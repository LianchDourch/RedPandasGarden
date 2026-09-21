#ifndef REDPANDASMARKETFORM_H
#define REDPANDASMARKETFORM_H

#include <QWidget>
#include <qsplitter.h>
#include <qtreewidget.h>
#include "core.h"
#include "itemwidget.h"
#include "redpandasmarket.h"
#include <QTableWidget>

namespace Ui {
class RedPandasMarketForm;
}

class RedPandasMarketForm : public QWidget
{
    Q_OBJECT

public:
    explicit RedPandasMarketForm(QWidget *parent = nullptr);
    ~RedPandasMarketForm();

    void refresh();
    void refreshMutableLists();

    void resetTree();
    void resetSellableTable();
private slots:
    void on_pushButton_viewOrders_clicked();

    void on_pushButton_addOrder_clicked();

    void on_pushButton_reset_clicked();

    void on_pushButton_tickall_clicked();

    void on_pushButton_unselectAll_clicked();

    void on_pushButton_upload_clicked();

private:
    Ui::RedPandasMarketForm *ui;
    QSplitter* splitter;
    QTreeWidget* canLocations;
    RepamListWidget* sellableItems;
    FULL_PROPERTY_PTR(Character*, character, nullptr, getCharacter, setCharacter, hasCharacter);

    Container currentContainer;
};

#endif // REDPANDASMARKETFORM_H
