#include "killmailwidget.h"

KillmailWidget::KillmailWidget(const Killmail &killmail, QWidget *parent) {}

LinedKillmailWidget::LinedKillmailWidget(const Killmail &killmail, QWidget *parent) : KillmailWidget(killmail, parent)
{
    new QHBoxLayout(this);
    this->layout()->setContentsMargins(3, 2, 3, 2);

    icon = new IconOnlyItemWidget();
    layout()->addWidget(icon);

    label = new QLabel(this);
    layout()->addWidget(label);

    refresh();
}

void LinedKillmailWidget::refresh() {

}
