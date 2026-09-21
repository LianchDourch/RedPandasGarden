#include "redpandasgarden.h"
#include "esimanager.h"

void RedPandasGarden::fetchRepamContainers(EsiConnector* c, std::function<void(const QList<Container>&)> receiver, ERROR_LISTENER_CPP) {
    EsiManager::TASK_MANAGER->addTask("Fetching REPAM Containers", [errorListener, c, receiver] () {
        c->fetchContainers("REPAM", [c, receiver, errorListener] (const QList<Container>& containers) {
            if (containers.isEmpty()) {
                receiver(containers);
                return;
            }

            c->fetchBlueprints([conn = c, receiver, errorListener, containers] () {
                QList<Container>* work = new QList<Container>();
                int* missingContainers = new int(containers.size());
                std::function<void(const QList<Container>&)> receiverProxy = [conn, receiver] (const QList<Container>& l) {
                    Util::println("end");
                    EsiManager::requestERP("DELETE FROM erpmarketcontainers WHERE characterId = :characterId", {{"characterId", conn->m_characterId}});
                    for (const Container &c: l) {
                        EsiManager::requestERP(
                            "INSERT INTO erpmarketcontainers(itemId, containerName, characterId) VALUES (:itemId, :containerName, :charId);",
                            {{"itemId", c.itemId}, {"containerName", c.name}, {"charId", conn->m_characterId}});
                    }

                    receiver(l);
                };
                for (const Container &c: containers) {
                    Util::println("Sending " + c.name);
                    conn->fetchContainerContents(c.itemId, [work, missingContainers, c, receiverProxy] (const QList<CharacterAsset>& contents) {
                        Container temp = c;
                        temp.contents = contents;
                        work->append(temp);
                        *missingContainers -= 1;
                        EsiManager::checkForEnd(missingContainers, work, receiverProxy);
                    }, [work, missingContainers, receiverProxy, errorListener] (const QString& err) {
                                               *missingContainers -= 1;
                                               EsiManager::checkForEnd(missingContainers, work, receiverProxy);
                                               errorListener(err);
                                           });
                }
            }, errorListener);

        }, errorListener);
    } );

}
