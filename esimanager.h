#ifndef ESIMANAGER_H
#define ESIMANAGER_H

#include <QSqlQuery>
#include <QSqlError>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QOAuthHttpServerReplyHandler>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <functional>
#include <QOAuth2AuthorizationCodeFlow>
#include <QHttpServer>
#include <QTcpServer>
#include "util.h"
#include "core.h"
#include "asyncmanager.h"

#define ERP_CONNECTION_NAME "ERP_Static_Conn"
#define SDE_CONNECTION_NAME "SDE_Static_Conn"

class RedPandasGarden;

class EsiManager {
    // Q_OBJECT
public:
    inline static QSqlDatabase SDE = QSqlDatabase();
    inline static QNetworkAccessManager* networkManager = nullptr;
    inline static QSqlDatabase ERP = QSqlDatabase();
    inline static QMap<int, double> ADJUSTED_PRICES = {};

    inline static AsyncTaskManager* TASK_MANAGER = nullptr;

    static bool isInDataThread() {
        return QThread::currentThread() == TASK_MANAGER->getWorkerThread();
    }

    static void initialize() {
        TASK_MANAGER = new AsyncTaskManager();
        TASK_MANAGER->addTask("Esi Manager Init", [] () { EsiManager::localInit(); });
    }

    static void clearCache() {
        if (QThread::currentThread() != TASK_MANAGER->getWorkerThread()) {
            TASK_MANAGER->addTaskAndWait("Clearing EsiManager cache", [] {
                EsiManager::requestERP("DELETE FROM assetscache");
                EsiManager::requestERP("DELETE FROM blueprintscache");
                return 0;
            });
        } else {
            EsiManager::requestERP("DELETE FROM assetscache");
            EsiManager::requestERP("DELETE FROM blueprintscache");
        }
    }

    static void load(Then then) {
        TASK_MANAGER->addTask("Loading ESI datas", [then] { fetchAdjustedPrices(then); });
    }
    static void loadAlliances(Then then) {
        TASK_MANAGER->addTask("Loading Alliances", [then] { fetchAlliances(then, Util::error); });
    }
    static void loadWars(Then then) {
        TASK_MANAGER->addTask("Loading Wars", [then] { fetchWars(then, Util::error); });
    }

    static void deepLoad(Then then) {
        load([then] () {
            loadAlliances([then] () {
                loadWars(then);
            });
        });
    }

    static bool isBlueprint(const QString& itemName);
    static bool isBlueprint(int typeId);

    static int requestTypeId(const QString& itemName, bool *ok = nullptr, std::function<void(QSqlError)> error = [] (QSqlError err) { Util::error(err.text()); }) {
        if (!isInDataThread()) return TASK_MANAGER->addTaskAndWait("Requesting type id", [itemName, ok, error] { return requestTypeId(itemName, ok, error); });
        QSqlQuery work = requestSDE("SELECT typeID FROM invtypes WHERE typeName = :name", {{"name", itemName}}, ok, error);

        if (work.next()) {
            if (ok != nullptr) *ok = true;
            return work.value("typeID").toInt();
        }

        if (ok != nullptr) *ok = false;
        Util::error("Unable to gather typeId for " + itemName);
        return 0;
    }

    static QSqlQuery request(QSqlDatabase* base, const QString &req, const QMap<QString, QVariant>& values = {}, bool* ok = nullptr, const std::function<void(QSqlError error)> onError = [] (QSqlError error) { Util::println("[ERROR] : ", error.text()); });
    static QSqlQuery requestSDE(const QString &req, const QMap<QString, QVariant>& values = {}, bool* ok = nullptr, const std::function<void(QSqlError error)> onError = [] (QSqlError error) { Util::println("[ERROR] : ", error.text()); });
    static QSqlQuery requestERP(const QString &req, const QMap<QString, QVariant>& values = {}, bool* ok = nullptr, const std::function<void(QSqlError error)> onError = [] (QSqlError error) { Util::println("[ERROR] : ", error.text()); });

    static bool isContainer(qint64 typeId);

    static void requestIcon(int typeId, int size, std::function<void(const QPixmap&)> receiver, ERROR_LISTENER, QString terminaison = "icon");

    static void fetchSystemCostIndices(QSet<int> targetSystemId, std::function<void (const QMap<int, VariableCostIndices> &)> then, ERROR_LISTENER);

    static void fetchAdjustedPrices(Then receiver, ERROR_LISTENER);
    static void fetchFreeports(Then then, ERROR_LISTENER);
    static double getAdjustedPrice(qint64 itemId, bool refreshIfPossible = false);
    static double getEstimatedPrice(qint64 itemId, bool refreshIfPossible = false);
    static void fetchRegionalOrders(int typeId, int regionId, std::function<void(const QByteArray&)> callback, int page = 1) {
        auto allData = std::make_shared<QByteArray>("[");
        fetchPage(typeId, regionId, page, allData, callback);
    }

    static QList<Alliance> searchAlliances(
        const QString& search,
        std::function<void(const QString&)> errorListener);
    static void fetchAlliances(
        std::function<void()> receiver,
        std::function<void(const QString&)> errorListener);
    static Alliance getAllianceFromExactName(const QString& name);
    static Alliance getAllianceFromId(qint64 id);

    static void fetchWars(Then then, ERROR_LISTENER);

    static QString getSystemName(qint64 solarSystemId);

    static void isKillmailOnZkillboard(
        qint64 killmailId,
        std::function<void(bool)> receiver,
        ERROR_LISTENER);
    static void fetchKillmail(
        qint64 killmailId,
        const QString& killmailHash,
        std::function<void(const Killmail&)> receiver,
        ERROR_LISTENER);

    /**
     * @brief parseMarketOrders
     * @param jsonData
     * @param targetStationId
     * @return {sellOrders, buyOrders}
     */
    static QPair<std::vector<MarketOrder>, std::vector<MarketOrder>> parseMarketOrders(const QByteArray& jsonData, long long targetStationId) {
        QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        if (!doc.isArray()) return {{}, {}};

        QJsonArray ordersArray = doc.array();

        std::vector<MarketOrder> sellOrders;
        std::vector<MarketOrder> buyOrders;

        for (const QJsonValue& value : ordersArray) {
            QJsonObject orderObj = value.toObject();

            if (orderObj["location_id"].toVariant().toLongLong() == targetStationId) {

                MarketOrder order {
                    orderObj["order_id"].toVariant().toLongLong(),
                    orderObj["price"].toDouble(),
                    orderObj["volume_remain"].toInt(),
                    orderObj["is_buy_order"].toBool()
                };

                if (order.isBuy) {
                    buyOrders.push_back(order);
                } else {
                    sellOrders.push_back(order);
                }
            }
        }

        std::sort(sellOrders.begin(), sellOrders.end(), [](const MarketOrder& a, const MarketOrder& b) {
            return a.price < b.price;
        });

        std::sort(buyOrders.begin(), buyOrders.end(), [](const MarketOrder& a, const MarketOrder& b) {
            return a.price > b.price;
        });

        return {sellOrders, buyOrders};
    }

    static void checkForEnd(int* counter, QList<Container>* list, std::function<void(const QList<Container>&)> receiver);

    static QString characterPortraitUrl(qint64 characterId, int size)
    {
        return QString(
                   "https://images.evetech.net/characters/%1/portrait?size=%2")
            .arg(characterId)
            .arg(size);
    }

    static void fetchPlayerPortrait(qint64 playerId, int size, std::function<void(QPixmap)> receiver) {
        if (!isInDataThread()) {
            QObject* temp = new QObject;

            TASK_MANAGER->addTask("Fetching player portrait",
                [playerId, size, receiver, temp]() mutable
                {
                    EsiManager::fetchPlayerPortrait(
                        playerId, size,
                        [playerId, size, receiver, temp](QPixmap result)
                        {
                            QMetaObject::invokeMethod(
                                temp,
                                [receiver, temp, result = std::move(result)]() mutable
                                {
                                    receiver(std::move(result));
                                    temp->deleteLater();
                                },
                                Qt::QueuedConnection
                                );
                        }
                        );
                }
                );

            return;
        }

        QNetworkRequest req(
            QUrl(characterPortraitUrl(playerId, size)));

        auto* reply =
            networkManager->get(req);

        QObject::connect(reply,
                &QNetworkReply::finished,
                [reply, receiver]()
                {
                    QPixmap pix;

                    pix.loadFromData(
                        reply->readAll());

                    receiver(std::move(pix));

                    reply->deleteLater();
                });
    }

    template<typename Result, typename... Args>
    static void recallInThread(
        std::function<void(Args..., std::function<void(Result)>)> out,
        std::function<void(Result)> recv,
        Args... args) {

        QObject* temp = new QObject;

        TASK_MANAGER->addTask(
            [out, recv, temp, args...]() mutable
            {
                out(
                    args...,
                    [recv, temp](Result result)
                    {
                        QMetaObject::invokeMethod(
                            temp,
                            [recv, temp, result = std::move(result)]() mutable
                            {
                                recv(std::move(result));
                                temp->deleteLater();
                            },
                            Qt::QueuedConnection
                            );
                    }
                    );
            }
            );
    }

    template<typename Result, typename... Args>
    static void proxyFunction(
        std::function<void(Args..., std::function<void(Result)>)> out,
        std::function<void(Result)> recv,
        Args... args)
    {
        if (QThread::currentThread() == TASK_MANAGER->getWorkerThread()) {
            out(
                args...,
                std::move(recv)
                );
            return;
        }

        recallInThread<Result, Args...>(out, recv, args...);
    }

    static CharacterAsset loadCharacterAsset(qint64 itemId, qint64 characterId);
    static Container loadContainer(const QString& name, qint64 characterId);
    static BlueprintAsset loadBlueprint(qint64 itemId, qint64 characterId);
private:
    static void resolveFreeportNamesInBatches(const QJsonArray& allIds, Then then, ERROR_LISTENER);
    // DELETE ME static void parseAndStoreFreeports(const QByteArray& jsonData);
    static void sendFreeportBatchRequest(const QJsonArray& batchIds, Then then, ERROR_LISTENER);

    static void _fetchPage(int typeId, int regionId, int page,
                           std::shared_ptr<QByteArray> accumulatedData,
                           std::function<void(const QByteArray&)> callback) {

        QString urlStr = QString("https://esi.evetech.net/latest/markets/%1/orders/"
                                 "?datasource=tranquility&order_type=all&type_id=%2&page=%3")
                             .arg(regionId).arg(typeId).arg(page);

        QNetworkRequest request((QUrl(urlStr)));
        QNetworkReply* reply = networkManager->get(request);

        QObject::connect(reply, &QNetworkReply::finished, [=]() {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                qWarning() << "Erreur ESI page" << page << ":" << reply->errorString();
                callback(QByteArray()); // Renvoie un tableau vide en cas d'erreur
                return;
            }

            QByteArray pageData = reply->readAll();

            if (pageData.startsWith('[')) {
                pageData.remove(0, 1);
            }
            if (pageData.endsWith(']')) {
                pageData.chop(1);
            }

            if (accumulatedData->size() > 1 && !pageData.isEmpty()) {
                accumulatedData->append(",");
            }
            accumulatedData->append(pageData);

            int totalPages = 1;

            if (reply->hasRawHeader("X-Pages")) {
                totalPages = reply->rawHeader("X-Pages").toInt();
            }

            if (page < totalPages) {
                fetchPage(typeId, regionId, page + 1, accumulatedData, callback);
            } else {
                accumulatedData->append("]");
                callback(*accumulatedData);
            }
        });
    }

    static void fetchPage(int typeId, int regionId, int page,
                   std::shared_ptr<QByteArray> accumulatedData,
                   std::function<void(const QByteArray&)> callback)
    {
        if (QThread::currentThread() != TASK_MANAGER->getWorkerThread()) {
            QObject* temp = new QObject(nullptr);
            temp->moveToThread(QThread::currentThread());
            TASK_MANAGER->addTask("Fetching page", [typeId, regionId, page, accumulatedData, callback, temp] () {
                _fetchPage(typeId, regionId, page, accumulatedData, [callback, temp] (const QByteArray& arg) {
                    QMetaObject::invokeMethod(temp, [callback, temp, arg] () {
                        callback(arg);
                        temp->deleteLater();
                    }, Qt::QueuedConnection);
            });});
        } else _fetchPage(typeId, regionId, page, accumulatedData, callback);
    }

    static void localInit() {
        networkManager = new QNetworkAccessManager;

        //! SDE
        SDE = QSqlDatabase::addDatabase("QSQLITE", SDE_CONNECTION_NAME);

        SDE.setDatabaseName("Datas/eve.db");

        if (!SDE.open()) {
            Util::println("[ERROR] : SDE not connected.");
        } else {
            Util::println("[INFO] : SDE connected.");
        }

        if (QSqlDatabase::contains(ERP_CONNECTION_NAME)) {
            Util::error("The connection already exists: " + QString(ERP_CONNECTION_NAME));
        } else {
            ERP = QSqlDatabase::addDatabase("QSQLITE", ERP_CONNECTION_NAME);
            ERP.setDatabaseName("Datas/erp.db");
            ERP.setConnectOptions("QSQLITE_BUSY_TIMEOUT=500");

            if (!ERP.open()) {
                Util::error("ERP DB not connected");
            } else {
                Util::println("[INFO] : ERP DB connected");
            }
        }
    }
};

struct PlayerStation {
    qint64 structureId = 0;
    QString name;
    int solarSystemId = 0;
    int typeId = 0;
};

class EsiConnector : public QObject
{
    Q_OBJECT

public:
    friend RedPandasGarden;

    explicit EsiConnector(QObject* parent = nullptr);
    ~EsiConnector() {
        Util::println("Destructing EsiConnector");
    }

    void login();
    void logout();

    bool isLoggedIn() const;

    QString characterName() const;
    qint64 characterId() const;

    QString accessToken() const { return EsiManager::isInDataThread() ? m_oauth.token() : EsiManager::TASK_MANAGER->addTaskAndWait("Getting auth token", [this] () mutable { return accessToken(); }); }

    void fetchSkills();
    void fetchPlayerStation(qint64 structureId,
                                   std::function<void(const PlayerStation&)> receiver,
                                   std::function<void(const QString&)> errorListener);

    void fetchAllianceStandings(
        std::function<void(const QList<AllianceStandings>&)> receiver,
        ERROR_LISTENER
        );

    void fetchRecentKillmails(std::function<void(const QList<KillmailReference>&)> receiver, ERROR_LISTENER);

    void writeAllianceStandings(
        const QMap<qint64, double>& standings,
        ERROR_LISTENER
        );
    void removeAllianceContacts(
        const QList<qint64>& allianceIds,
        std::function<void()> receiver,
        ERROR_LISTENER
        );

    void fetchContainers(const QString& key,
        std::function<void(const QList<Container>&)> receiver,
        ERROR_LISTENER
        );
    void fetchContainerContents(
        qint64 containerId,
        std::function<void(const QList<CharacterAsset>&)> receiver,
        ERROR_LISTENER
        );
    void fetchBlueprints(
        std::function<void()> receiver,
        ERROR_LISTENER_CPP
        );

    inline const QString& getRepamToken() const { return m_repamToken; }
signals:
    void loginSucceeded();
    void loginFailed(QString reason);

    void skillsReceived(QJsonObject skills);

private:
    void setupOAuth();
    void fetchCharacterInfo();
    void connectToREPAM();

private:
    QOAuth2AuthorizationCodeFlow m_oauth;
    QOAuthHttpServerReplyHandler* m_replyHandler = nullptr;

    QNetworkAccessManager m_network;

    QString m_characterName;
    qint64 m_characterId = 0;

    QString m_clientId = "c8c02509619845b896226aefe267db22";
    QString m_repamToken;
};


#endif // EVEMARKETFETCHER_H
