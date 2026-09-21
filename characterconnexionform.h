#ifndef CHARACTERCONNEXIONFORM_H
#define CHARACTERCONNEXIONFORM_H

#include <QWidget>
#include "core.h"
#include "imagewidget.h"

namespace Ui {
class CharacterConnexionForm;
}

class CharacterConnexionForm : public QWidget
{
    Q_OBJECT

public:
    explicit CharacterConnexionForm(QWidget *parent = nullptr);
    ~CharacterConnexionForm();

    void refreshCharacters();
    void refreshCharacterView();
    void refreshPortrait(QSize newSize = {});
    void setPortrait(const QPixmap& original);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void on_pushButton_clicked();

    void on_comboBox_charlist_activated(int index);

    void on_pushButton_refresh_clicked();

    void on_pushButton_delete_clicked();

private:
    Ui::CharacterConnexionForm *ui;
    QPixmap original;
};

#endif // CHARACTERCONNEXIONFORM_H
