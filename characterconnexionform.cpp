#include "characterconnexionform.h"
#include "ui_characterconnexionform.h"
#include "mainwindow.h"

CharacterConnexionForm::CharacterConnexionForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CharacterConnexionForm)
{
    ui->setupUi(this);

    ui->label_portrait->installEventFilter(this);

    refreshCharacterView();
}

void CharacterConnexionForm::refreshPortrait(QSize newSize) {
    if (newSize.isNull()) newSize = ui->label_portrait->size();
    ui->label_portrait->setPixmap(original.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void CharacterConnexionForm::setPortrait(const QPixmap& original) {
    this->original = original;
    refreshPortrait();
}

bool CharacterConnexionForm::eventFilter(QObject* watched, QEvent* event) {
    if (watched == ui->label_portrait &&
        event->type() == QEvent::Resize)
    {
        refreshPortrait(dynamic_cast<QResizeEvent*>(event)->size());
    }

    return false;
}

CharacterConnexionForm::~CharacterConnexionForm()
{
    delete ui;
}

void CharacterConnexionForm::refreshCharacterView() {
    Character* character = Characters::fromName(ui->comboBox_charlist->currentText());

    ui->label_characterName->setText(character == nullptr ? "" : character->getName());

    if (character == nullptr) {
        ui->checkBox_omega->setChecked(false);
    } else {
        ui->checkBox_omega->setChecked(character->isOmega());
    }
    setPortrait(character == nullptr ? QPixmap() : character->getPortrait());
}

void CharacterConnexionForm::refreshCharacters() {
    ui->comboBox_charlist->clear();
    for (Character* c: Characters::VALUES) ui->comboBox_charlist->addItem(c->getName());
    refreshCharacterView();
}

void CharacterConnexionForm::on_pushButton_clicked()
{
    bool omega = ui->checkBox_omega->isChecked();
    Characters::logCharacter([this, omega] (Character* c) { Util::println("refreshing..."); c->setOmega(omega); refreshCharacters(); MainWindow::INSTANCE->refreshMutableLists(); });
}


void CharacterConnexionForm::on_comboBox_charlist_activated(int index)
{
    refreshCharacterView();
}


void CharacterConnexionForm::on_pushButton_refresh_clicked()
{
    Character* c = Characters::fromName(ui->comboBox_charlist->currentText());
    if (c != nullptr) {
        c->setOmega(ui->checkBox_omega->isChecked());
    }

    refreshCharacters();
}


void CharacterConnexionForm::on_pushButton_delete_clicked()
{
    Character* c = Characters::fromName(ui->comboBox_charlist->currentText());
    if (c != nullptr) {
        Characters::VALUES.removeAll(c);
        delete c;
    }

    refreshCharacters();
}

