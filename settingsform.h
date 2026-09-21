#ifndef SETTINGSFORM_H
#define SETTINGSFORM_H

#include <QWidget>

namespace Ui {
class SettingsForm;
}

class SettingsForm : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsForm(QWidget *parent = nullptr);
    ~SettingsForm();

    void refresh();
private slots:
    void on_pushButton_loadAlliances_clicked();

    void on_pushButton_loadWars_clicked();

    void on_pushButton_loadRepam_clicked();

    void on_pushButton_clearCache_clicked();

private:
    Ui::SettingsForm *ui;
};

#endif // SETTINGSFORM_H
