#include "esimanager.h"
#include "mainwindow.h"
#include "settings.h"
#include <QDebug>
#include "redpandasgarden.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setStyleSheet(Settings::GENERAL_STYLESHEET);


    QObject::connect(qApp, &QCoreApplication::aboutToQuit, []() {
        Core::save();
    });

    EsiManager::initialize();
    RedPandasGarden::preInit();
    Stations::readActivities({
                             {1, 0},
                             {2, 1},
                             {3, 2},
                             {4, 3},
                             {5, 4},
                             {6, 5},
                            });

    MainWindow w;
    w.showMaximized();

    Core::preload([] () {
        EsiManager::load([] () {
            Util::println("loaded esi");
            Core::load([] () {
                RedPandasGarden::postInit();
                QMetaObject::invokeMethod(qApp, [] () {
                    MainWindow::INSTANCE->initialize();
                });
            });
        });
    });

    return a.exec();
}
