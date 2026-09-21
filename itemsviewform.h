#ifndef ITEMSVIEWFORM_H
#define ITEMSVIEWFORM_H

#include <QWidget>

namespace Ui {
class ItemsViewForm;
}

class ItemsViewForm : public QWidget
{
    Q_OBJECT

public:
    explicit ItemsViewForm(QWidget *parent = nullptr);
    ~ItemsViewForm();

    inline bool isReadOnly() const { return readOnly; }
    inline void setReadOnly(bool readOnly) {
        this->readOnly = readOnly;
        refreshReadOnly();
    }

    void refreshReadOnly();
private:
    Ui::ItemsViewForm *ui;
    bool readOnly = true;
};

#endif // ITEMSVIEWFORM_H
