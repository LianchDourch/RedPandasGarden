#ifndef ITEMWIDGET_H
#define ITEMWIDGET_H

#include "core.h"
#include "flagmanager.h"
#include "settings.h"
#include "imagewidget.h"
#include <QWidget>
#include <qcheckbox.h>
#include <qpainter.h>
#include <qscrollarea.h>

struct ItemViewFlags : public FlagsArray1Byte {
    using FlagsArray1Byte::FlagsArray1Byte;

    inline bool isIconVisible() const { return get(0); }
    inline bool isNameVisible() const { return get(1); }
    inline bool isAmountVisible() const { return get(2); }

    inline void setIconVisible(bool visible) { set(0, visible); }
    inline void setNameVisible(bool visible) { set(1, visible); }
    inline void setAmountVisible(bool visible) { set(2, visible); }
};

class ItemWidget : public QWidget
{
    Q_OBJECT
private:
    ItemStack itemStack = {};
    bool initialized = false;

public:
    explicit ItemWidget(QWidget *parent = nullptr);

    void setItem(Item* item, int amount = 1) { setItem({item, amount}); }
    void setItem(const ItemStack &stack);
    inline bool hasItem() const { return getItem() != nullptr; }
    inline int getAmount() const { return itemStack.getQuantity(); }
    inline Item* getItem() const { return itemStack.getItem(); }
    inline ItemStack& getItemStack() { return itemStack; }
    inline const ItemStack& getItemStack() const { return itemStack; }

    void initialize() { initialized = true; init(); }
    void refresh() { if (!initialized) initialize(); refreshView(); }


    static QPixmap DEFAULT_ICON_GETTER(Item* item) { return item->getIcon(); }
protected:
    virtual void init() {}
    virtual void refreshView() = 0;
    virtual void onItemSet() {}
signals:
};

class IconOnlyItemWidget : public ItemWidget {
private:
    ImageWidget* viewport = new ImageWidget(true, this);

    std::function<QPixmap(Item*)> getter = IconOnlyItemWidget::DEFAULT_ICON_GETTER;
public:
    using ItemWidget::ItemWidget;

    void init() override {
    }

    void setBackground(QColor background) {
        viewport->setBackground(background);
    }

    QColor getBackground() const {
        return viewport->getBackground();
    }

    void setIconGetter(const std::function<QPixmap(Item*)>& func) { this->getter = func; }

    void refreshView() override {
        viewport->setPixmap(hasItem() ? getter(getItem()) : QPixmap());
        refreshSize();
    }

    void refreshSize() {
        int direction = std::min(width(), height());
        viewport->resize(direction, direction);
        viewport->move((width() - direction) / 2, (height() - direction) / 2);
    }
protected:
    void resizeEvent(QResizeEvent* event) override {
        refreshSize();

        ItemWidget::resizeEvent(event);
    }
};

class LinedItemWidget : public ItemWidget {
private:
    ImageWidget* iconWidget;
    QLabel* name,* amount;

    QColor background = APPLICATION_BACKGROUND_COLOR;

    ItemViewFlags flags = 0b00000111;

    std::function<QPixmap(Item*)> getter = IconOnlyItemWidget::DEFAULT_ICON_GETTER;
public:
    using ItemWidget::ItemWidget;

    inline void setBackground(QColor color) { this->background = color; }
    inline QColor getBackground() const { return this->background; }

    inline LinedItemWidget* setIconVisible(bool visibility) { flags.setIconVisible(visibility); return this; }
    inline bool isIconVisible() const { return flags.isIconVisible(); }

    inline LinedItemWidget* setAmountVisible(bool visibility) { flags.setAmountVisible(visibility); return this; }
    inline bool isAmountVisible() const { return flags.isAmountVisible(); }

    inline void setIconGetter(const std::function<QPixmap(Item*)>& func) { this->getter = func; }

    void init() override {
        Util::println("Initializating");

        setMinimumHeight(20);

        QHBoxLayout *layout = new QHBoxLayout(this);
        layout->setContentsMargins(5, 1, 5, 1);

        iconWidget = new ImageWidget(true, this);
        iconWidget->setBackground(getBackground());
        iconWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        layout->addWidget(iconWidget, 2);

        name = new QLabel(this);
        layout->addWidget(name, 6 + (isIconVisible() ? 0 : 2) + (isAmountVisible() ? 0 : 2));

        /*
        QWidget* spacer = new QWidget(this);
        spacer->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
        layout()->addWidget(spacer);
        */

        amount = new QLabel(this);
        amount->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        layout->addWidget(amount, 2);
    }

    void refreshView() override {
        name->setStyleSheet("background-color: " + getBackground().name() + ";");
        amount->setStyleSheet("background-color: " + getBackground().name() + ";");

        iconWidget->setPixmap(hasItem() ? getter(getItem()) : QPixmap());
        name->setText(hasItem() ? getItem()->getName() : QString());
        amount->setText(QString::number(getAmount()));

        iconWidget->setVisible(isIconVisible());
        amount->setVisible(hasItem() && isAmountVisible());
    }

protected:
    void resizeEvent(QResizeEvent* event) override {
        ItemWidget::resizeEvent(event);

        iconWidget->setFixedSize(height()-2, height()-2);
    }

    void paintEvent(QPaintEvent* event) override {
        QPainter painter(this);

        if (getBackground().isValid()) {
            painter.fillRect(rect(), getBackground());
        }
        ItemWidget::paintEvent(event);
    }
};

class LinedItemWidgetV2 : public ItemWidget {
private:
    QLabel* icon = nullptr;
    QLabel* name = nullptr;
    QLabel* amount = nullptr;
    uint iconMaxSize = -1;
    bool autoLoadItem;
public:
    LinedItemWidgetV2(QWidget* parent = nullptr, bool autoLoadItem = false) : ItemWidget(parent), autoLoadItem(autoLoadItem) {
        init();
    }

    void limitIconSize(uint size) {
        iconMaxSize = size;
    }



protected:
    void init() override {
        new QHBoxLayout(this);

        icon = new QLabel(this);
        name = new QLabel(this);
        amount = new QLabel(this);
        amount->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
        amount->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        layout()->addWidget(icon);
        layout()->addWidget(name);
        layout()->addWidget(amount);
        layout()->setContentsMargins(0, 0, 0, 0);

        icon->hide();
        name->hide();
        amount->hide();
    }

    void resizeEvent(QResizeEvent* event) override {
        refresh();
    }

    void refreshView() override {
        if (hasItem()) {
            name->setText(getItem()->getName());
            amount->setText(QString::number(getAmount()));

            int selfWidth = width();
            int iconWidth = std::min(static_cast<uint>(height()), iconMaxSize);
            int nameWidth = name->minimumSizeHint().width();
            int quantityWidth = amount->minimumSizeHint().width();

            if (selfWidth < (iconWidth + nameWidth + quantityWidth)) {
                icon->show();
                icon->setPixmap(getItemStack().getIcon().scaledToWidth(iconWidth, Qt::SmoothTransformation));
                icon->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
                name->show();
                amount->hide();
            } else {
                icon->show();
                icon->setPixmap(getItemStack().getIcon().scaledToWidth(iconWidth, Qt::SmoothTransformation));
                icon->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
                name->show();
                amount->show();
            }

        } else {
            icon->hide();
            name->hide();
            amount->hide();
        }
    }

    void onItemSet() override {
        if (autoLoadItem && hasItem() && !getItem()->isLoaded()) {
            getItem()->fetchDatas(nullptr, [this] (Item* i) { refresh(); }, Util::error);
        }
    }
};

#endif // ITEMWIDGET_H
