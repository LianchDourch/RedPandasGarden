#include "subapplications.h"
#include "addstationform.h"
#include "characterconnexionform.h"
#include "industryform.h"
#include "productionchainform.h"
#include "redpandasmarketform.h"
#include "standingmanagerform.h"
#include "zkbsmartuploaderform.h"

SubApplication* SubApplications::MANUFACTURE_PLANNER = SubApplications::registerApp(new SubApplication("Manufacture", "Manufacture", [] (QWidget* parent) {
    return new IndustryForm(parent);
}));

SubApplication* SubApplications::STATION_MANAGER = SubApplications::registerApp(new SubApplication("Stations Searcher", "Add Station", [] (QWidget* parent) {
    return new AddStationForm(parent);
}));

SubApplication* SubApplications::CHARACTERS_MANAGER = SubApplications::registerApp(new SubApplication("Character Manager", "Characters", [] (QWidget* parent) {
    return new CharacterConnexionForm(parent);
}));

SubApplication* SubApplications::PRODUCTION_CHAIN_MANAGER = SubApplications::registerApp(new SubApplication(
    "Production Chain Manager", "Prod. Chains", [] (QWidget* parent) { return new ProductionChainForm(parent); }
));

SubApplication* SubApplications::STANDING_MANAGER = SubApplications::registerApp(new SubApplication(
    "Eyes Standings Manager", "Eyes Stand.", [] (QWidget* parent) { return new StandingManagerForm(parent); }
));

SubApplication* SubApplications::ZKB_UPLOADER = SubApplications::registerApp(new SubApplication(
    "Zkb Smart Uploader", "Zkb Upload", [] (QWidget* parent) { return new ZkbSmartUploaderForm(parent); }
));

SubApplication* SubApplications::REPAG_MARKET = SubApplications::registerApp(new SubApplication(
    "Red Pandas Market", "REPAM", [] (QWidget* parent) { return new RedPandasMarketForm(parent); }
));

SubApplication* SubApplication::setWindow(QMdiSubWindow* window) {
    _setWindow(window);

    SubApplications::WINDOWS.insert(window, this);

    return this;
}

void SubApplications::onStationsListChange() {
    dynamic_cast<IndustryForm*>(MANUFACTURE_PLANNER->getWindow()->widget())->refreshStationsList();
}
