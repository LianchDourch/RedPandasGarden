#ifndef SUBAPPLICATIONS_H
#define SUBAPPLICATIONS_H

#include <QString>
#include <QMdiArea>
#include <QMdiSubWindow>
#include "util.h"

class SubApplication
{
private:
    CONST_CONSTRUCTABLE_PROPERTY_BIGPOD(QString, name, getName);
    CONST_CONSTRUCTABLE_PROPERTY_BIGPOD(QString, smallName, getSmallName);
    FULL_PROPERTY_PTR(QMdiSubWindow*, window, nullptr, getWindow, _setWindow, hasWindow);
    CONST_CONSTRUCTABLE_PROPERTY_BIGPOD(std::function<QWidget*(QWidget* parent)>, widgetProvider, getWidgetProvider);
public:
    CONSTRUCTOR(SubApplication, name, smallName, widgetProvider) {}

    SubApplication* setWindow(QMdiSubWindow* window);
};

struct SubApplications {
    static SubApplication* MANUFACTURE_PLANNER;
    static SubApplication* STATION_MANAGER;
    static SubApplication* CHARACTERS_MANAGER;
    static SubApplication* PRODUCTION_CHAIN_MANAGER;
    static SubApplication* STANDING_MANAGER;
    static SubApplication* ZKB_UPLOADER;
    static SubApplication* REPAG_MARKET;

    inline static QList<SubApplication*> APPLICATIONS = {};
    inline static QHash<QMdiSubWindow*, SubApplication*> WINDOWS = {};

    static SubApplication* registerApp(SubApplication* app) {
        APPLICATIONS.append(app);
        return app;
    }

    static void onStationsListChange();
};

#endif // SUBAPPLICATIONS_H
