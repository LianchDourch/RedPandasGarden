#ifndef INDUSTRYFORM_H
#define INDUSTRYFORM_H

#include "core.h"
#include "itemwidget.h"
#include <QWidget>
#include "util.h"
#include <functional>

namespace Ui {
class IndustryForm;
}

class IndustryForm : public QWidget
{
    Q_OBJECT

public:
    explicit IndustryForm(QWidget *parent = nullptr);
    ~IndustryForm();

    void setItem(const ItemStack& item, int row, std::function<void(Item *)> then = [] (Item*) {});
    void addItem(const ItemStack& item, std::function<void(Item *)> then = [] (Item*) {});

    void setBlueprint(Blueprint* item);
    bool setBlueprint(const QString& item);
    void setBlueprintWaitingScreen();

    Station* getBuyingHub();
    Station* getSellingHub();
    Station* getFacility() const;
    inline bool hasFacility() const { return getFacility() != nullptr; }
    Character* getCharacter();

    void refreshMaterials();
    void refreshStationsList();
    void refreshCharactersList();
    void refreshJob();
    void refreshMutableLists() {
        refreshStationsList();
        refreshCharactersList();
    }

    bool setMaterials(const QMap<ItemStackDatas, int> &map);
    void _setMaterials(const QMap<ItemStackDatas, int> &map);

    void setValue(QLabel* label, double money);

    void onMaterialsFetchEnd();
private slots:
    void on_pushButton_submitMaterials_clicked();

    void on_pushButton_refreshMaterials_clicked();

    void on_pushButton_submitBp_clicked();

private:
    Ui::IndustryForm *ui;
    IconOnlyItemWidget* blueprintItemWidget;
    LinedItemWidget* outputItemWidget;
    FULL_PROPERTY_PTR(Blueprint*, currentBlueprint, nullptr, getBlueprint, _setBlueprint, hasBlueprint);

    int* materialFetchCounter = nullptr;
};

#endif // INDUSTRYFORM_H
