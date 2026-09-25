#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "addstationform.h"
#include "asyncmanager.h"
#include "characterconnexionform.h"
#include "loadingform.h"
#include "redpandasgarden.h"
#include "redpandasmarketform.h"
#include "subapplications.h"
#include "standingmanagerform.h"
#include <qboxlayout.h>
#include <qpushbutton.h>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    INSTANCE = this;

    settingsForm = new SettingsForm(this);
    settingsForm->setWindowFlag(Qt::Window);

    loadingScreen = ui->mdiArea_mainview->addSubWindow(new LoadingForm);
    loadingScreen->showMaximized();

    QWidget* bottom = new QWidget();
    new QHBoxLayout(bottom);
    bottom->layout()->setContentsMargins(1, 1, 1, 1);
    runningTasks = new QLabel("  tasks running...", bottom);
    bottom->layout()->addWidget(runningTasks);
    QPushButton* work = new QPushButton("Show running tasks");
    work->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    bottom->layout()->addWidget(work);
    QObject::connect(work, &QPushButton::pressed, this, &MainWindow::showRunningTasks);

    tasksWidget = new AsyncTaskListWidget(this);
    tasksWidget->setWindowFlag(Qt::Window);

    ui->statusbar->addWidget(bottom);
}

void MainWindow::refreshRunningTasks() {
    int counter = 0;
    for (rpt::SafePtr<AsyncTaskManager> manager: tasksWidget->getLinkedManagers()) if (manager->isRunning()) counter++;
    runningTasks->setText(QString::number(counter) + " tasks running...");
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
}

void MainWindow::refreshTasksWidget() {
    for (rpt::SafePtr<AsyncTaskManager> manager: tasksWidget->getLinkedManagers()) {
        if (tasksWidget->contains(manager)) continue;
        tasksWidget->connect(manager);
        QObject::connect(manager.rawPtr(), &AsyncTaskManager::taskStarted, [this] (AsyncTask* task) { refreshRunningTasks(); });
        QObject::connect(manager.rawPtr(), &AsyncTaskManager::taskFinished, this, &MainWindow::refreshRunningTasks);
        QObject::connect(manager.rawPtr(), &AsyncTaskManager::taskFailed, this, [this] (AsyncTask* task, const QString& error) {
            QMessageBox::critical(this, "Unable to perform task", "Error while performing task" + (" '" + task->getName() + "'") + "\n" + error);
            refreshRunningTasks();
        });
    }
}

void MainWindow::setTasksWidgetVisibility(bool b) {
    tasksWidget->setVisible(b);

    if (b) {
        refreshTasksWidget();
    }
}

void MainWindow::showRunningTasks() {
    setTasksWidgetVisibility(true);
}

void MainWindow::initialize() {
    setWindowTitle("Red Pandas' Garden");
    Util::println("Initializing mainwindow");

    COLORED_TOP_BAR(45, 45, 45);

    QPushButton* button;
    for (SubApplication* app: SubApplications::APPLICATIONS) {
        app->setWindow(ui->mdiArea_mainview->addSubWindow(app->getWidgetProvider()(this)));
        app->getWindow()->setAttribute(Qt::WA_DeleteOnClose, false);
        app->getWindow()->installEventFilter(this);
        app->getWindow()->setWindowTitle(app->getName());
        app->getWindow()->hide();

        button = new QPushButton(app->getSmallName(), ui->widget_navbuttons);
        button->setMinimumHeight(40);
        ui->widget_navbuttons->layout()->addWidget(button);
        button->setFlat(true);
        connect(button, &QPushButton::clicked, this, [this, app] () { openApp(app); });
    }

    loadingScreen->deleteLater();
    loadingScreen = nullptr;


    refreshRunningTasks();
    refreshMutableLists();

}

void MainWindow::refreshMutableLists() {
    dynamic_cast<CharacterConnexionForm*>(SubApplications::CHARACTERS_MANAGER->getWindow()->widget())->refreshCharacters();
    dynamic_cast<AddStationForm*>(SubApplications::STATION_MANAGER->getWindow()->widget())->refreshCharacters();
    dynamic_cast<IndustryForm*>(SubApplications::MANUFACTURE_PLANNER->getWindow()->widget())->refreshMutableLists();
    dynamic_cast<StandingManagerForm*>(SubApplications::STANDING_MANAGER->getWindow()->widget())->refreshMutableLists();
    dynamic_cast<RedPandasMarketForm*>(SubApplications::REPAG_MARKET->getWindow()->widget())->refreshMutableLists();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::Close) {
        QMdiSubWindow *subWindow = qobject_cast<QMdiSubWindow*>(obj);
        if (subWindow) {
            subWindow->hide();
            event->ignore();
            return true;
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::openApp(SubApplication* app) {
    QMdiSubWindow *temp = ui->mdiArea_mainview->currentSubWindow();

    if (loadingScreen != nullptr) return;

    if (temp != nullptr && temp->isMaximized()) {
        app->getWindow()->showMaximized();
    } else {
        app->getWindow()->hide();
        app->getWindow()->show();
    }
}

MainWindow::~MainWindow()
{
    delete ui;
    INSTANCE = nullptr;
}

void MainWindow::on_pushButton_settings_clicked()
{
    settingsForm->hide();
    settingsForm->refresh();
    settingsForm->show();
}

