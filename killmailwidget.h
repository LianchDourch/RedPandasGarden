#ifndef KILLMAILWIDGET_H
#define KILLMAILWIDGET_H

#include <qwidget.h>
#include "core.h"
#include "itemwidget.h"

class KillmailWidget : public QWidget
{
protected:
    GENERAL_PROPERTY_BIGPOD(Killmail, killmail, Killmail(), getKillmail, setKillmail)
public:
    KillmailWidget(const Killmail &killmail, QWidget* parent = nullptr);
};

class LinedKillmailWidget : public KillmailWidget {
public:
    LinedKillmailWidget(const Killmail &killmail, QWidget *parent = nullptr);

    void refresh();
private:
    IconOnlyItemWidget* icon;
    QLabel* label;
};

#endif // KILLMAILWIDGET_H
