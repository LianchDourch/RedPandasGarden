#ifndef REDPANDASMARKET_H
#define REDPANDASMARKET_H

#include "itemwidget.h"
#include <QWidget>
#include <QString>
#include <qcheckbox.h>
#include <qinputdialog.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qscrollarea.h>
#include <qvalidator.h>
#include <qwidget.h>


struct MarketActor {
    qint64 localId;
    QString name;
    qint64 playerId;
};

namespace RedPandasMarket
{
class ApplicationWidget : public QWidget {
public:
    ApplicationWidget(QWidget* parent);
};
};

class RepamActionWidget : public QWidget {
private:
    CONST_PROPERTY_PTR(QCheckBox*, specifyPrice, getPriceSpecify);
    CONST_PROPERTY_PTR(QLabel*, label, getLabel);
    CONST_PROPERTY_PTR(QLineEdit*, priceLineEdit, getPriceEdit);
    CONST_PROPERTY_PTR(QWidget*, spacer1, getSpacer1);
    CONST_PROPERTY_PTR(QPushButton*, openDescriptionEdit, getDescriptionButton);
    CONST_PROPERTY_BIGPOD(QString, description, getDescription);

public:
    RepamActionWidget() {
        new QHBoxLayout(this);
        layout()->setContentsMargins(1, 1, 1, 1);

        specifyPrice = new QCheckBox;
        specifyPrice->setText("Set price");
        specifyPrice->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
        layout()->addWidget(specifyPrice);

        spacer1 = new QWidget();
        spacer1->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
        layout()->addWidget(spacer1);

        label = new QLabel(this);
        label->setText("Selling price (ISK/u): ");
        label->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
        layout()->addWidget(label);


        priceLineEdit = new QLineEdit();
        (new QDoubleValidator(priceLineEdit))->setLocale(QLocale::c());
        priceLineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        layout()->addWidget(priceLineEdit);

        openDescriptionEdit = new QPushButton;
        openDescriptionEdit->setText("Edit Description");
        openDescriptionEdit->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
        connect(openDescriptionEdit, &QPushButton::clicked, this, [this] () {
            QString str = QInputDialog::getMultiLineText(this, "Edit Description", "Enter the description of the order here.", description);
            description = str;
        });
        layout()->addWidget(openDescriptionEdit);

        setContentVisible(false);

        connect(specifyPrice, &QCheckBox::checkStateChanged, this, [this] () {
            setPriceEditVisible(specifyPrice->isChecked());
        });
    }

    void reset() {
        specifyPrice->setChecked(false);
        priceLineEdit->setText("");
        description = QString();
    }

    void setContentVisible(bool visible) {
        reset();
        specifyPrice->setVisible(visible);
        setPriceEditVisible(specifyPrice->isChecked());
        openDescriptionEdit->setVisible(visible);
    }

    void setPriceEditVisible(bool visible) {
        label->setVisible(visible);
        priceLineEdit->setVisible(visible);
        spacer1->setVisible(!visible);
    }
};

class RepamItemWidget : public QWidget {
private:
    CONST_PROPERTY_PTR(QCheckBox*, checkBox, getCheckBox);
    CONST_PROPERTY_PTR(LinedItemWidgetV2*, itemWidget, getItemWidget);
    CONST_PROPERTY_PTR(RepamActionWidget*, actionWidget, getActionWidget);
    CONST_PROPERTY_BIGPOD(CharacterAsset, asset, getAsset);

public:
    RepamItemWidget(const CharacterAsset &asset) {
        new QHBoxLayout(this);
        this->asset = asset;
        layout()->setContentsMargins(2, 2, 2, 2);

        checkBox = new QCheckBox;
        checkBox->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
        checkBox->setText(QString());
        layout()->addWidget(checkBox);

        itemWidget = new LinedItemWidgetV2(this, true);
        itemWidget->setItem(Items::fromId(asset.typeId), asset.quantity);
        layout()->addWidget(itemWidget);

        actionWidget = new RepamActionWidget();
        layout()->addWidget(actionWidget);

        connect(checkBox, &QCheckBox::checkStateChanged, this, [this] () {
            actionWidget->setContentVisible(checkBox->isChecked());
        });
    }
    RepamItemWidget(const BlueprintAsset& asset) : RepamItemWidget(static_cast<CharacterAsset>(asset)) {
        ItemStack it{Items::fromId(asset.typeId), asset.quantity};
        it.setBpc(asset.bpc);
        itemWidget->setItem(it);
    }
};

class RepamListWidget : public QScrollArea {
private:
    QWidget* viewport = nullptr;
    QList<RepamItemWidget*> items;

public:
    RepamListWidget(QWidget* parent = nullptr) : QScrollArea(parent) {
        setWidgetResizable(true);

        viewport = new QWidget();
        new QVBoxLayout(viewport);

        setWidget(viewport);
    }

    void clear() {
        Util::clearChildren(viewport);

        items = {};
    }

    void addItem(const CharacterAsset &item) {
        RepamItemWidget* temp = new RepamItemWidget{item};

        items.append(temp);
        viewport->layout()->addWidget(temp);
    }

    const QList<RepamItemWidget*>& getItemsView() const { return items; }

    void setWholeCheckState(bool checked);
};


#endif // REDPANDASMARKET_H
