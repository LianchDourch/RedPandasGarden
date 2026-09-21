#ifndef ZKBSMARTUPLOADERFORM_H
#define ZKBSMARTUPLOADERFORM_H

#include <QWidget>
#include "core.h"

namespace Ui {
class ZkbSmartUploaderForm;
}

class ZkbSmartUploaderForm : public QWidget
{
    Q_OBJECT

public:
    explicit ZkbSmartUploaderForm(QWidget *parent = nullptr);
    ~ZkbSmartUploaderForm();

    void reset();
    void resetCharacters();
    inline void refresh() {
        refreshOutput();
    }
    void refreshOutput();

    void addKillmailView(const Killmail& k) {}
private slots:
    void on_pushButton_fetchKills_clicked();

private:
    Ui::ZkbSmartUploaderForm *ui;
};

#endif // ZKBSMARTUPLOADERFORM_H
