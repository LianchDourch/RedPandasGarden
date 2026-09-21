#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "asyncmanager.h"
#include "industryform.h"
#include <QMainWindow>
#include <QMdiArea>
#include <QMdiSubWindow>
#include "settingsform.h"
#include "subapplications.h"
#include "util.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    inline static MainWindow* INSTANCE = nullptr;
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void initialize();
    void refreshMutableLists();

    void openApp(SubApplication* app);

    void onStationsListChange() {
        SubApplications::onStationsListChange();
    }

    void refreshRunningTasks();
    void showRunningTasks();
    void setTasksWidgetVisibility(bool visibility);
    void refreshTasksWidget();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

    void resizeEvent(QResizeEvent* event) override;
private slots:
    void on_pushButton_settings_clicked();

private:
    Ui::MainWindow *ui;
    QMdiSubWindow* loadingScreen = nullptr;
    SettingsForm* settingsForm = nullptr;
    QLabel* runningTasks = nullptr;
    AsyncTaskListWidget* tasksWidget = nullptr;
};
#endif // MAINWINDOW_H
