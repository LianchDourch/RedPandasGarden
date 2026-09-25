#include "esimanager.h"
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtConcurrent/QtConcurrent>

#include "redpandasgarden.h"

void EsiManager::fetchSystemCostIndices(QSet<int> targetSystemId, std::function<void(const QMap<int, VariableCostIndices>&)> then, std::function<void (const QString &)> errorListener) {
    if (!isInDataThread()) {
        QObject* temp = new QObject;

        TASK_MANAGER->addTask("Fetching System Cost Indices",
            [targetSystemId, then, errorListener, temp]() mutable
            {
                EsiManager::fetchSystemCostIndices(
                    targetSystemId,
                    [targetSystemId, then, errorListener, temp](const QMap<int, VariableCostIndices>& result)
                    {
                        QMetaObject::invokeMethod(
                            temp,
                            [then, temp, result = std::move(result)]() mutable
                            {
                                then(std::move(result));
                                temp->deleteLater();
                            },
                            Qt::QueuedConnection
                            );
                    }, errorListener
                    );
            }
            );

        return;
    }

    QUrl url("https://esi.evetech.net/latest/industry/systems/");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Engineering Red Panda");

    QNetworkReply* reply = networkManager->get(request);

    QObject::connect(reply, &QNetworkReply::finished, [reply, targetSystemId, then]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            Util::error("Erreur ESI : " + reply->errorString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isArray()) return;

        QJsonArray systemsArray = doc.array();

        QMap<int, VariableCostIndices> res = {};
        for (QJsonValueRef sysValue : systemsArray) {
            VariableCostIndices resultIndices;
            QJsonObject sysObj = sysValue.toObject();
            if (targetSystemId.contains(sysObj["solar_system_id"].toInt())) {

                QJsonArray indices = sysObj["cost_indices"].toArray();

                for (QJsonValueRef indValue : indices) {
                    QJsonObject indObj = indValue.toObject();
                    QString activity = indObj["activity"].toString();
                    double indexValue = indObj["cost_index"].toDouble();

                    if (activity == "manufacturing") resultIndices.set(0, indexValue);
                    else if (activity == "researching_time_efficiency") resultIndices.set(1, indexValue);
                    else if (activity == "researching_material_efficiency") resultIndices.set(2, indexValue);
                    else if (activity == "copying") resultIndices.set(3, indexValue);
                    else if (activity == "invention") resultIndices.set(4, indexValue);
                    else if (activity == "reactions") resultIndices.set(5, indexValue);

                }
                res.insert(sysObj["solar_system_id"].toInt(), resultIndices);
                if (res.size() == targetSystemId.size()) break;
            }
        }

        then(res);
    });
}

void EsiManager::fetchAdjustedPrices(Then then, ERROR_LISTENER_CPP) {
    if (!isInDataThread()) {
        Util::println("Wrong thread lol");

        QObject* temp = new QObject;

        TASK_MANAGER->addTask("Fetching Adjusted Prices",
            [then, errorListener, temp]() mutable
            {
                EsiManager::fetchAdjustedPrices(
                    [then, temp]()
                    {
                        QMetaObject::invokeMethod(
                            temp,
                            [then, temp]() mutable
                            {
                                then();
                                temp->deleteLater();
                            },
                            Qt::QueuedConnection
                            );
                    }, errorListener
                    );
            }
            );

        return;
    }

    Util::println("In fetchAdjustedPrice");

    requestERP("DELETE FROM adjustedPrices");
    Util::println("A");

    QNetworkRequest req = QNetworkRequest(QUrl("https://esi.evetech.net/latest/markets/prices/?datasource=tranquility"));

    req.setHeader(QNetworkRequest::UserAgentHeader, "Engineering Red Panda");

    Util::println(reinterpret_cast<qint64>(QThread::currentThread()), " =?= ", reinterpret_cast<qint64>(networkManager->thread()));

    QNetworkReply* reply = networkManager->get(req);
    Util::println("B");
    QObject::connect(reply, &QNetworkReply::finished, qApp, [reply, then, errorListener] () {
        Util::println("Pre - C");
        EsiManager::TASK_MANAGER->addTask("Receiving adjusted prices", [reply, then, errorListener] () {
            Util::println("C");
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                errorListener(reply->errorString());
                Util::println("D");
                then();
                return;
            }

            QByteArray rawData = reply->readAll();

            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);

            if (parseError.error != QJsonParseError::NoError) {
                errorListener("Erreur de parsing JSON: " + parseError.errorString());
                then();
                return;
            }

            if (!doc.isArray()) {
                errorListener("Format de réponse ESI invalide (Tableau attendu).");
                then();
                return;
            }

            QJsonArray priceArray = doc.array();


            ERP.transaction();

            for (const QJsonValue &value : priceArray) {
                QJsonObject obj = value.toObject();
                int typeId = obj["type_id"].toInt();
                double adjustedPrice, averagePrice;

                if (obj.contains("adjusted_price")) {
                    adjustedPrice = obj["adjusted_price"].toDouble();
                } else {
                    adjustedPrice = 0.0;
                }

                if (obj.contains("average_price")) {
                    averagePrice = obj["average_price"].toDouble();
                } else {
                    averagePrice = 0.0;
                }

                requestERP("INSERT INTO adjustedprices VALUES (:typeId, :adjustedPrice, :averagePrice)", {
                        {"typeId", typeId},
                        {"adjustedPrice", adjustedPrice},
                        {"averagePrice", averagePrice},
                });
            }

            ERP.commit();

            Util::println("Calling then");
            then();
        });});
}

double EsiManager::getAdjustedPrice(qint64 itemId, bool refreshPrices) {
    // Pas besoin de thread check, car assumé par les fonctions appelées
    bool ok = false;
    double price = -1.0;

    {
        QSqlQuery query = requestERP(
            "SELECT adjustedPrice FROM adjustedPrices WHERE typeId = :typeId",
            {{"typeId", itemId}}, &ok);

        if (query.next() && ok) {
            price = query.value("adjustedPrice").toDouble();
        }
    }

    return price;
}

double EsiManager::getEstimatedPrice(qint64 itemId, bool refreshPrices) {
    // Pas besoin de thread check, car assumé par les fonctions appelées
    bool ok = false;
    double price = -1.0;

    {
        QSqlQuery query = requestERP(
            "SELECT averagePrice FROM adjustedPrices WHERE typeId = :typeId",
            {{"typeId", itemId}}, &ok);

        if (query.next() && ok) {
            price = query.value("averagePrice").toDouble();
        }
    }

    return price;
}

void EsiManager::requestIcon(int typeId, int size, std::function<void(const QPixmap&)> receiver, std::function<void(const QString&)> error, QString terminaison) {
    if (!isInDataThread()) {
        QObject* temp = new QObject;

        TASK_MANAGER->addTask("Fetching Icon",
            [typeId, size, receiver, error, terminaison, temp]() mutable
            {
                EsiManager::requestIcon(
                    typeId, size,
                    [typeId, size, receiver, error, terminaison, temp](const QPixmap& result)
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
                    }, error, terminaison
                    );
            }
            );

        return;
    }
    // Construction de l'URL officielle de CCP
    QString urlStr = QString("https://images.evetech.net/types/%1/%3?size=%2")
                         .arg(typeId)
                         .arg(size)
                         .arg(terminaison);

    QUrl url(urlStr);
    QNetworkRequest request(url);

    // Lancement de la requête asynchrone
    QNetworkReply* reply = networkManager->get(request);

    // Connexion moderne Qt6 avec une fonction lambda pour traiter le résultat
    QObject::connect(reply, &QNetworkReply::finished, [reply, receiver, error]() {
        reply->deleteLater(); // Nettoyage de la mémoire de la requête

        if (reply->error() != QNetworkReply::NoError) {
            error("Unable to download icon :" + reply->errorString());
            return;
        }

        // Lecture des données brutes reçues
        QByteArray data = reply->readAll();
        QPixmap pixmap;

        if (pixmap.loadFromData(data)) {
            receiver(pixmap);
        } else {
            error("Unable to convert raw datas to an image.");
        }
    });
}

QSqlQuery EsiManager::request(QSqlDatabase* base, const QString &req, const QMap<QString, QVariant>& values, bool* ok, const std::function<void(QSqlError error)> onError) {
    QSqlQuery query(*base);
    query.prepare(req);
    for (auto [k, v] : values.asKeyValueRange()) {
        query.bindValue(":" + k, v);
    }

    if (!query.exec()) {
        std::cerr << "[" << base->connectionName().toStdString() << "] " << "Erreur SQL : '" << query.lastError().text().toStdString() << "' while executing : " << req.toStdString() << std::endl;
        if (ok != nullptr) *ok = false;
        return QSqlQuery();
    }

    if (ok != nullptr) *ok = true;
    return query;
}

QSqlQuery EsiManager::requestERP(const QString &req, const QMap<QString, QVariant>& values, bool* ok, const std::function<void(QSqlError error)> onError) {
    if (QThread::currentThread() != TASK_MANAGER->getWorkerThread()) return TASK_MANAGER->addTaskAndWait(QString(), [req, values, ok, onError] { return request(&ERP, req, values, ok, onError); });
    else return request(&ERP, req, values, ok, onError);
}

void EsiManager::fetchFreeports(Then then, ERROR_LISTENER_CPP) {
    if (!isInDataThread()) {
        QObject* temp = new QObject;

        TASK_MANAGER->addTask("Fetching Freeports",
            [then, errorListener, temp]() mutable
            {
                EsiManager::fetchFreeports(
                    [then, temp]()
                    {
                        QMetaObject::invokeMethod(
                            temp,
                            [then, temp]() mutable
                            {
                                then();
                                temp->deleteLater();
                            },
                            Qt::QueuedConnection
                            );
                    }, errorListener
                    );
            }
            );

        return;
    }

    QUrl url("https://esi.evetech.net/latest/universe/structures/?datasource=tranquility");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "EngineeringRedPanda");

    QNetworkReply *reply = networkManager->get(request);

    QObject::connect(reply, &QNetworkReply::finished, [reply, errorListener, then]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            return errorListener(reply->errorString());
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isArray()) {
            resolveFreeportNamesInBatches(doc.array(), then, errorListener);
        }
    });
}

void EsiManager::resolveFreeportNamesInBatches(const QJsonArray& allIds, Then then, ERROR_LISTENER_CPP) {
    int i = 0;
    while (i < allIds.size()) {
        QJsonArray batch;
        for (int j = 0; j < 1000 && i < allIds.size(); ++j, ++i) {
            batch.append(allIds[i]);
        }

        sendFreeportBatchRequest(batch, then, errorListener);
    }
}

void EsiManager::sendFreeportBatchRequest(const QJsonArray& batchIds, Then then, ERROR_LISTENER_CPP) {
    if (!isInDataThread()) {
        QObject* temp = new QObject;

        TASK_MANAGER->addTask("Getting freeports",
            [batchIds, then, errorListener, temp]() mutable
            {
                EsiManager::sendFreeportBatchRequest(
                    batchIds,
                    [then, temp]()
                    {
                        QMetaObject::invokeMethod(
                            temp,
                            [then, temp]() mutable
                            {
                                then();
                                temp->deleteLater();
                            },
                            Qt::QueuedConnection
                            );
                    }, errorListener
                    );
            }
            );

        return;
    }

    QUrl url("https://esi.evetech.net/latest/universe/names/?datasource=tranquility");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, "EngineeringRedPanda");

    //QJsonDocument doc(batchIds);
    QJsonDocument doc(QJsonArray({QJsonValue(1050444740652ll)}));
    QNetworkReply *reply = networkManager->post(request, doc.toJson(QJsonDocument::Compact));

    QObject::connect(reply, &QNetworkReply::finished, [reply, then, errorListener]() {
        struct Datas {
            qint64 structureId;
            QString name;
        };

        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return errorListener("=( " + reply->errorString());

        qDebug() << reply->readAll();
        /*
        QJsonDocument resDoc = QJsonDocument::fromJson(reply->readAll());
        if (!resDoc.isArray()) return;

        QJsonArray results = resDoc.array();
        for (const QJsonValue& val : results) {
            QJsonObject obj = val.toObject();
            // L'ESI renvoie la catégorie pour être sûr que c'est une structure
            if (obj["category"].toString() == "structure") {
                Datas data;
                data.stationId = obj["id"].toVariant().toLongLong();
                data.stationName = obj["name"].toString();
                resolvedFreeports.append(data);
            }
        }*/
    });
}
/*
void EsiManager::parseAndStore(const QByteArray& jsonData) {

    qDebug() << jsonData;

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isArray()) {
        Util::error("Invalid JSON Format while parsing structures");
        return;
    }

    QJsonArray structuresArray = doc.array();

    EsiManager::requestERP("DELETE FROM structures");
    ERP.transaction();

    bool ok = true;

    for (const QJsonValue& value : structuresArray) {
        QJsonObject obj = value.toObject();

        Datas data;
        data.structureId = obj["structureID"].toString().toLongLong();
        data.name = obj["name"].toString();
        data.solarSystemId = obj["solarSystemID"].toInt();
        data.typeId = obj["typeID"].toInt();

        if (data.structureId > 0 && !data.name.isEmpty()) {
            bool temp;
            EsiManager::requestERP(
                "INSERT INTO structures VALUES (:stationId, :stationName, :systemId, :typeId)",
                {{"stationId", data.structureId}, {"stationName", data.name},
                 {"systemId", data.solarSystemId}, {"typeId", data.typeId}}, &temp);
            ok &= temp;

        }
    }

    ERP.commit();

    if (ok) Util::println("[INFO] : freeports récupérés avec succès.");
    else Util::println("[WARNING] : An error occured while fetching freeports");
}*/

QSqlQuery EsiManager::requestSDE(const QString &req, const QMap<QString, QVariant>& values, bool* ok, const std::function<void(QSqlError error)> onError) {
    if (QThread::currentThread() != TASK_MANAGER->getWorkerThread()) return TASK_MANAGER->addTaskAndWait(QString(), [req, values, ok, onError] {
            return request(&SDE, req, values, ok, onError);
        });

    return request(&SDE, req, values, ok, onError);
}

bool EsiManager::isBlueprint(const QString& itemName) {
    if (!isInDataThread()) {
        return TASK_MANAGER->addTaskAndWait(QString(), [itemName] { return isBlueprint(itemName); });
    } else {
        bool ok = false;
        int id = requestTypeId(itemName, &ok);
        if (!ok) return false;

        return isBlueprint(id);
    }
}

bool EsiManager::isBlueprint(int typeId) {
    if (!isInDataThread()) return TASK_MANAGER->addTaskAndWait(QString(), [typeId] { return isBlueprint(typeId); });
    else {
        QSqlQuery res = EsiManager::requestSDE("SELECT 1 FROM industryBlueprints WHERE typeID = :typeId;", {{"typeId", typeId}});
        bool found = res.next();

        return found;
    }
}


static constexpr auto AUTHORIZE_URL =
    "https://login.eveonline.com/v2/oauth/authorize";

static constexpr auto TOKEN_URL =
    "https://login.eveonline.com/v2/oauth/token";

static constexpr auto VERIFY_URL =
    "https://login.eveonline.com/oauth/verify";

static constexpr auto ESI_SKILLS_URL =
    "https://esi.evetech.net/latest/characters/%1/skills/";

static constexpr auto ESI_CHARACTER_URL =
    "https://esi.evetech.net/latest/characters/%1/";

EsiConnector::EsiConnector(QObject* parent)
    : QObject(parent)
{
    if (QThread::currentThread() != EsiManager::TASK_MANAGER->getWorkerThread()) {
        EsiManager::TASK_MANAGER->addTaskAndWait(QString(), [this] () mutable { setupOAuth(); return 0; });
    } else {
        setupOAuth();
    }
}

void EsiConnector::setupOAuth()
{
    m_replyHandler =
        new QOAuthHttpServerReplyHandler(
            12042,
            this);

    // Important pour EVE SSO
    m_replyHandler->setCallbackPath("/");

    m_oauth.setReplyHandler(m_replyHandler);

    m_oauth.setAuthorizationUrl(
        QUrl(AUTHORIZE_URL));

    // API moderne Qt6
    m_oauth.setTokenUrl(
        QUrl(TOKEN_URL));

    m_oauth.setClientIdentifier(
        m_clientId);

    m_oauth.setRequestedScopeTokens({
        "esi-skills.read_skills.v1",
        "esi-universe.read_structures.v1",
        "esi-characters.read_contacts.v1",
        "esi-characters.write_contacts.v1",
        "esi-killmails.read_killmails.v1",
        "esi-assets.read_assets.v1",
        "esi-characters.read_blueprints.v1"
    });

    // PKCE moderne (recommandé par CCP)
    m_oauth.setPkceMethod(
        QOAuth2AuthorizationCodeFlow::PkceMethod::S256);

    connect(
        &m_oauth,
        &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser,
        this,
        [](const QUrl& url)
        {
            QDesktopServices::openUrl(url);
        });

    connect(
        &m_oauth,
        &QOAuth2AuthorizationCodeFlow::granted,
        this,
        [this]()
        {
            fetchCharacterInfo();
        });

    // API moderne Qt6
    connect(
        &m_oauth,
        &QAbstractOAuth::requestFailed,
        this,
        [this](QAbstractOAuth::Error error)
        {
            emit loginFailed(
                QString("OAuth error: %1")
                    .arg(static_cast<int>(error)));
        });
}

void EsiConnector::connectToREPAM()
{
    if (!EsiManager::isInDataThread()) {
        EsiManager::TASK_MANAGER->addTaskAndWait(QString(), [this] () mutable { connectToREPAM(); return 0; });
        return;
    }

    QNetworkRequest request(
        QUrl("https://eve.redpandasgarden.com/api/login.php"));

    request.setHeader(
        QNetworkRequest::ContentTypeHeader,
        "application/json");

    request.setRawHeader(
        "X-EVE-Access-Token",
        m_oauth.token().toUtf8());

    QNetworkReply* reply =
        m_network.post(request, QByteArray());

    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply]()
        {
            const QByteArray data =
                reply->readAll();

            const QNetworkReply::NetworkError error =
                reply->error();

            const QString errorString =
                reply->errorString();

            reply->deleteLater();

            if (error != QNetworkReply::NoError)
            {
                emit loginFailed(
                    QString("Market login error: %1")
                        .arg(errorString));

                return;
            }

            QJsonParseError parseError;

            const QJsonDocument doc =
                QJsonDocument::fromJson(
                    data,
                    &parseError);

            if (parseError.error !=
                    QJsonParseError::NoError ||
                !doc.isObject())
            {
                emit loginFailed(
                    "Invalid market login response");

                return;
            }

            const QJsonObject json =
                doc.object();

            if (!json["success"].toBool())
            {
                emit loginFailed(
                    json["error"]
                        .toString(
                            "Market login failed"));

                return;
            }

            const QString token =
                json["token"].toString();

            if (token.isEmpty())
            {
                emit loginFailed(
                    "Market server returned an empty token");

                return;
            }

            m_repamToken = token;

            emit loginSucceeded();
        });
}

void EsiConnector::login()
{
    if (!EsiManager::isInDataThread()) {
        EsiManager::TASK_MANAGER->addTaskAndWait("Logging In", [this] () mutable { login(); return 0; });
        return;
    }
    m_oauth.grant();
}

void EsiConnector::logout()
{
    if (!EsiManager::isInDataThread()) {
        EsiManager::TASK_MANAGER->addTaskAndWait("Logging out", [this] () mutable { logout(); return 0; });
        return;
    }
    m_oauth.setToken({});
}

bool EsiConnector::isLoggedIn() const
{
    if (!EsiManager::isInDataThread()) {
        return EsiManager::TASK_MANAGER->addTaskAndWait(QString(), [this] () mutable { return isLoggedIn(); });
    }
    return !m_oauth.token().isEmpty() && !m_repamToken.isNull();
}

QString EsiConnector::characterName() const
{
    return m_characterName;
}

qint64 EsiConnector::characterId() const
{
    return m_characterId;
}

void EsiConnector::fetchCharacterInfo()
{
    if (!EsiManager::isInDataThread()) {
        EsiManager::TASK_MANAGER->addTaskAndWait("Fetching Character Information", [this] () mutable { fetchCharacterInfo(); return 0; });
        return;
    }

    QNetworkRequest req = QNetworkRequest(QUrl(VERIFY_URL));

    req.setRawHeader(
        "Authorization",
        QString("Bearer %1")
            .arg(m_oauth.token())
            .toUtf8());

    auto* reply =
        m_network.get(req);

    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply]()
        {
            const auto data =
                reply->readAll();


            qDebug() << "=== EVE VERIFY ===";
            qDebug() << "HTTP status:"
                     << reply->attribute(
                            QNetworkRequest::HttpStatusCodeAttribute);
            qDebug() << "Network error:"
                     << reply->errorString();
            qDebug() << "Response:"
                     << data;
            qDebug() << "==================";

            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError)
            {
                emit loginFailed(
                    QString("EVE verify error: %1")
                        .arg(reply->errorString()));

                reply->deleteLater();
                return;
            }

            const auto doc =
                QJsonDocument::fromJson(data);

            if (!doc.isObject())
            {
                emit loginFailed(
                    "Invalid verify response");
                return;
            }

            const auto json =
                doc.object();

            m_characterName =
                json["CharacterName"]
                    .toString();

            m_characterId =
                json["CharacterID"]
                    .toInteger();

            connectToREPAM();
        });
}

void EsiConnector::fetchSkills()
{
    if (!EsiManager::isInDataThread()) {
        EsiManager::TASK_MANAGER->addTaskAndWait("Fetching Character Skills", [this] () mutable { fetchSkills(); return 0; });
        return;
    }

    if (m_characterId == 0)
    {
        emit loginFailed(
            "Character ID not available");
        return;
    }

    const auto url =
        QUrl(
            QString(ESI_SKILLS_URL)
                .arg(m_characterId));

    QNetworkRequest req(url);

    req.setRawHeader(
        "Authorization",
        QString("Bearer %1")
            .arg(m_oauth.token())
            .toUtf8());

    auto* reply =
        m_network.get(req);

    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply]()
        {
            const auto data =
                reply->readAll();

            reply->deleteLater();

            const auto doc =
                QJsonDocument::fromJson(data);

            if (!doc.isObject())
            {
                emit loginFailed(
                    "Invalid skills response");
                return;
            }

            emit skillsReceived(
                doc.object());
        });
}

void EsiConnector::fetchPlayerStation(qint64 structureId,
                                      std::function<void(const PlayerStation&)> receiver,
                                      ERROR_LISTENER)
{
    if (!EsiManager::isInDataThread()) {
        QObject* temp = new QObject;

        EsiManager::TASK_MANAGER->addTask("Fetching Player Stations",
            [this, structureId, receiver, errorListener, temp]() mutable
            {
                fetchPlayerStation(
                    structureId,
                    [receiver, temp](const PlayerStation& result)
                    {
                        QMetaObject::invokeMethod(
                            temp,
                            [receiver, result, temp]() mutable
                            {
                                receiver(result);
                                temp->deleteLater();
                            },
                            Qt::QueuedConnection
                            );
                    }, errorListener
                    );
            }
            );

        return;
    }

    if (accessToken().isEmpty()) {
        errorListener("Token d'accès vide. Un joueur doit être connecté.");
        return;
    }

    QString urlStr = QString("https://esi.evetech.net/latest/universe/structures/%1/?datasource=tranquility")
                         .arg(structureId);

    QNetworkRequest request((QUrl(urlStr)));
    request.setHeader(QNetworkRequest::UserAgentHeader, "Engineering Red Panda");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(accessToken()).toUtf8());

    QNetworkReply* reply = m_network.get(request);

    connect(reply, &QNetworkReply::finished, [reply, structureId, receiver, errorListener]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            errorListener(QString("Erreur ESI Structure %1 : %2")
                              .arg(structureId)
                              .arg(reply->errorString()));
            return;
        }

        QByteArray data = reply->readAll();
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            errorListener("Erreur de parsing JSON ou format invalide pour la structure.");
            return;
        }

        QJsonObject obj = doc.object();
        PlayerStation station;
        station.structureId = structureId;
        station.name = obj["name"].toString();
        station.solarSystemId = obj["solar_system_id"].toInt();
        station.typeId = obj["type_id"].toInt(); // L'ESI renvoie aussi le type d'Athanor/Keepstar/etc.

        receiver(station);
    });
}


QList<Alliance> EsiManager::searchAlliances(
    const QString& search,
    std::function<void(const QString&)> errorListener)
{
    // Pas besoin de thread check, car assumé par les fonctions appelées
    bool ok = false;

    QSqlQuery work = EsiManager::requestERP(R"(
        SELECT id, name, ticker
        FROM alliances
        WHERE name LIKE '%' || LOWER(:search) || '%'
        ORDER BY name
    )", {{"search", search}}, &ok, [errorListener] (QSqlError err) { errorListener(err.text()); });

    if (!ok) return QList<Alliance>();

    QList<Alliance> res = {};
    while (work.next()) {
        res.append(Alliance{work.value("id").toLongLong(), work.value("name").toString(), work.value("ticker").toString()});
    }
    return res;
}

Alliance EsiManager::getAllianceFromExactName(const QString& name) {
    // Pas besoin de thread check, car assumé par les fonctions appelées
    bool ok = false;

    QSqlQuery query = EsiManager::requestERP(R"(
        SELECT id, name, ticker
        FROM alliances
        WHERE name = :name
    )", {{"name", name}}, &ok, [] (QSqlError err) { Util::error(err.text()); });

    if (ok && query.next()) {
        return Alliance{query.value("id").toLongLong(), query.value("name").toString(), query.value("ticker").toString()};
    } else return Alliance();
}

Alliance EsiManager::getAllianceFromId(qint64 id) {
    // Pas besoin de thread check, car assumé par les fonctions appelées
    bool ok = false;

    QSqlQuery query = EsiManager::requestERP(R"(
        SELECT id, name, ticker
        FROM alliances
        WHERE id = :id
    )", {{"id", id}}, &ok, [] (QSqlError err) { Util::error(err.text()); });

    if (ok && query.next()) {
        return Alliance{query.value("id").toLongLong(), query.value("name").toString(), query.value("ticker").toString()};
    } else return Alliance();
}

void EsiManager::fetchAlliances(
    std::function<void()> receiver,
    std::function<void(const QString&)> errorListener)
{
    if (!isInDataThread()) {
        QObject* temp = new QObject;

        TASK_MANAGER->addTask("Fetching Alliances",
            [receiver, errorListener, temp]() mutable
            {
                EsiManager::fetchAlliances(
                    [receiver, temp] ()
                    {
                        QMetaObject::invokeMethod(
                            temp,
                            [receiver, temp]() mutable
                            {
                                receiver();
                                temp->deleteLater();
                            },
                            Qt::QueuedConnection
                            );
                    }, errorListener
                    );
            }
            );

        return;
    }

    // Création de la table si nécessaire
    bool ok = false;

    requestERP(
        R"(
            CREATE TABLE IF NOT EXISTS alliances (
                id INTEGER PRIMARY KEY,
                name TEXT NOT NULL,
                ticker TEXT NOT NULL
            )
        )",
        {},
        &ok,
        [errorListener](QSqlError error)
        {
            errorListener(error.text());
        }
        );

    if (!ok) {
        return;
    }

    // Récupération de la liste des IDs d'alliances
    QUrl url(
        "https://esi.evetech.net/latest/alliances/"
        "?datasource=tranquility"
        );

    QNetworkRequest request(url);

    QNetworkReply* reply = networkManager->get(request);

    QObject::connect(
        reply,
        &QNetworkReply::finished,
        [reply, receiver, errorListener]()
        {
            if (reply->error() != QNetworkReply::NoError) {
                const QString error =
                    QString("Unable to fetch alliances: %1")
                        .arg(reply->errorString());

                reply->deleteLater();
                errorListener(error);
                return;
            }

            const QByteArray data = reply->readAll();
            reply->deleteLater();

            QJsonParseError parseError;

            const QJsonDocument document =
                QJsonDocument::fromJson(
                    data,
                    &parseError
                    );

            if (parseError.error != QJsonParseError::NoError ||
                !document.isArray())
            {
                errorListener(
                    "Invalid ESI alliances response: " +
                    parseError.errorString()
                    );
                return;
            }

            const QJsonArray allianceIds = document.array();

            if (allianceIds.isEmpty()) {
                receiver();
                return;
            }

            // Nombre de requêtes restantes
            auto remaining =
                std::make_shared<int>(allianceIds.size());

            auto failed =
                std::make_shared<bool>(false);

            for (const QJsonValue& value : allianceIds) {

                const qint64 allianceId =
                    value.toVariant().toLongLong();

                if (allianceId <= 0) {
                    --(*remaining);
                    continue;
                }

                const QString url =
                    QString(
                        "https://esi.evetech.net/latest/"
                        "alliances/%1/"
                        "?datasource=tranquility"
                        ).arg(allianceId);

                QNetworkRequest request{QUrl(url)};

                QNetworkReply* allianceReply =
                    networkManager->get(request);

                QObject::connect(
                    allianceReply,
                    &QNetworkReply::finished,
                    [allianceReply,
                     allianceId,
                     remaining,
                     failed,
                     receiver,
                     errorListener]()
                    {
                        if (allianceReply->error() !=
                            QNetworkReply::NoError)
                        {
                            if (!*failed) {
                                *failed = true;

                                const QString error =
                                    QString(
                                        "Unable to fetch "
                                        "alliance %1: %2"
                                        )
                                        .arg(allianceId)
                                        .arg(
                                            allianceReply->errorString()
                                            );

                                errorListener(error);
                            }

                            allianceReply->deleteLater();

                            --(*remaining);

                            return;
                        }

                        const QByteArray data =
                            allianceReply->readAll();

                        allianceReply->deleteLater();

                        QJsonParseError parseError;

                        const QJsonDocument document =
                            QJsonDocument::fromJson(
                                data,
                                &parseError
                                );

                        if (parseError.error !=
                                QJsonParseError::NoError ||
                            !document.isObject())
                        {
                            if (!*failed) {
                                *failed = true;

                                errorListener(
                                    "Invalid ESI response "
                                    "for alliance " +
                                    QString::number(allianceId) +
                                    ": " +
                                    parseError.errorString()
                                    );
                            }

                            --(*remaining);
                            return;
                        }

                        const QJsonObject object =
                            document.object();

                        const QString name =
                            object.value("name").toString();

                        const QString ticker =
                            object.value("ticker").toString();

                        if (name.isEmpty()) {
                            if (!*failed) {
                                *failed = true;

                                errorListener(
                                    "Alliance " +
                                    QString::number(allianceId) +
                                    " has no name."
                                    );
                            }

                            --(*remaining);
                            return;
                        }

                        // Sauvegarde dans ERP
                        bool ok = false;

                        EsiManager::requestERP(
                            R"(
                                INSERT INTO alliances
                                    (id, name, ticker)
                                VALUES
                                    (:id, :name, :ticker)
                                ON CONFLICT(id)
                                DO UPDATE SET
                                    name = excluded.name,
                                    ticker = excluded.ticker
                            )",
                            {
                                {"id", allianceId},
                                {"name", name},
                                {"ticker", ticker}
                            },
                            &ok,
                            [allianceId,
                             failed,
                             remaining,
                             receiver,
                             errorListener]
                            (QSqlError error)
                            {
                                if (!*failed) {
                                    *failed = true;

                                    errorListener(
                                        QString(
                                            "Unable to save "
                                            "alliance %1: %2"
                                            )
                                            .arg(allianceId)
                                            .arg(error.text())
                                        );
                                }

                                --(*remaining);

                                if (*remaining == 0 &&
                                    !*failed) {
                                    receiver();
                                }
                            }
                            );

                        if (!ok && !*failed) {
                            *failed = true;

                            errorListener(
                                QString(
                                    "Unable to save "
                                    "alliance %1"
                                    ).arg(allianceId)
                                );

                            --(*remaining);
                        } else if (ok) {
                            --(*remaining);

                            if (*remaining == 0 &&
                                !*failed) {
                                receiver();
                            }
                        }
                    }
                    );
            }
        }
        );
}

void EsiConnector::fetchAllianceStandings(
    std::function<void(const QList<AllianceStandings>&)> receiver,
    ERROR_LISTENER_CPP)
{
    if (!EsiManager::isInDataThread()) {
        QObject* temp = new QObject;

        EsiManager::TASK_MANAGER->addTask("Fetching Alliance Standings",
            [this, receiver, errorListener, temp]() mutable
            {
                fetchAllianceStandings(
                    [receiver, temp](const QList<AllianceStandings>& result)
                    {
                        QMetaObject::invokeMethod(
                            temp,
                            [receiver, result, temp]() mutable
                            {
                                receiver(result);
                                temp->deleteLater();
                            },
                            Qt::QueuedConnection
                            );
                    }, errorListener
                    );
            }
            );

        return;
    }

    if (!isLoggedIn())
        return errorListener("Connector not Logged In");

    const qint64 characterId = this->m_characterId;

    if (characterId <= 0) {
        errorListener("Invalid character ID.");
        return;
    }

    const QString urlStr =
        QString("https://esi.evetech.net/latest/characters/%1/contacts/"
                "?datasource=tranquility")
            .arg(characterId);

    QNetworkRequest request{QUrl(urlStr)};

    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        "Engineering Red Panda"
        );

    request.setRawHeader(
        "Authorization",
        QString("Bearer %1").arg(accessToken()).toUtf8()
        );

    QNetworkReply* reply = m_network.get(request);

    QObject::connect(
        reply,
        &QNetworkReply::finished,
        [reply, receiver, errorListener]()
        {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                /*const auto statusCode =
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

                const QByteArray body = reply->readAll();

                qDebug() << "HTTP status:" << statusCode;
                qDebug() << "Qt error:" << reply->error();
                qDebug() << "Qt error string:" << reply->errorString();
                qDebug() << "Response body:" << body;*/

                errorListener(
                    "Unable to fetch character contacts: " +
                    reply->errorString()
                    );
                return;
            }

            const QByteArray data = reply->readAll();

            QJsonParseError parseError;
            const QJsonDocument document =
                QJsonDocument::fromJson(data, &parseError);

            if (parseError.error != QJsonParseError::NoError ||
                !document.isArray())
            {
                errorListener(
                    "Invalid ESI contacts response: " +
                    parseError.errorString()
                    );
                return;
            }

            QList<AllianceStandings> standings;

            for (const QJsonValue& value : document.array()) {
                const QJsonObject object = value.toObject();

                // On ne garde que les contacts de type alliance.
                if (object.value("contact_type").toString() != "alliance")
                    continue;

                AllianceStandings standing;

                standing.id =
                    object.value("contact_id")
                        .toVariant()
                        .toLongLong();

                standing.standing =
                    object.value("standing").toDouble();

                standings.append(standing);
            }

            receiver(standings);
        }
        );
}

void EsiManager::fetchWars(Then then, ERROR_LISTENER_CPP)
{
    if (!isInDataThread()) {
        QObject* temp = new QObject;

        TASK_MANAGER->addTask("Fetching Wars",
            [then, errorListener, temp]() mutable
            {
                EsiManager::fetchWars(
                    [then, temp]()
                    {
                        QMetaObject::invokeMethod(
                            temp,
                            [then, temp]() mutable
                            {
                                then();
                                temp->deleteLater();
                            },
                            Qt::QueuedConnection
                            );
                    }, errorListener
                    );
            }
            );

        return;
    }

    constexpr int MAX_CONCURRENT_REQUESTS = 10;

    const QUrl url(
        "https://esi.evetech.net/latest/wars/"
        "?datasource=tranquility"
        );

    QNetworkRequest request{url};

    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        "Engineering Red Panda"
        );

    QNetworkReply* reply = networkManager->get(request);

    QObject::connect(
        reply,
        &QNetworkReply::finished,
        [reply, then, errorListener]()
        {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                errorListener(
                    "Unable to fetch ESI wars: " +
                    reply->errorString()
                    );
                return;
            }

            const QByteArray data = reply->readAll();

            QJsonParseError parseError;
            const QJsonDocument document =
                QJsonDocument::fromJson(data, &parseError);

            if (parseError.error != QJsonParseError::NoError ||
                !document.isArray())
            {
                errorListener(
                    "Invalid ESI wars response: " +
                    parseError.errorString()
                    );
                return;
            }

            const QJsonArray array = document.array();

            if (array.isEmpty()) {
                then();
                return;
            }

            auto warIds = std::make_shared<QList<qint64>>();

            for (const QJsonValue& value : array) {
                const qint64 warId =
                    value.toVariant().toLongLong();

                if (warId > 0)
                    warIds->append(warId);
            }

            if (warIds->isEmpty()) {
                then();
                return;
            }

            auto nextIndex =
                std::make_shared<int>(0);

            auto activeRequests =
                std::make_shared<int>(0);

            auto remaining =
                std::make_shared<int>(warIds->size());

            auto failed =
                std::make_shared<bool>(false);

            /*
             * Fonction récursive qui lance les prochaines requêtes
             * jusqu'à atteindre MAX_CONCURRENT_REQUESTS.
             */
            auto processNext =
                std::make_shared<std::function<void()>>();

            *processNext =
                [warIds,
                 nextIndex,
                 activeRequests,
                 remaining,
                 failed,
                 processNext,
                 then,
                 errorListener]()
            {
                if (*failed)
                    return;

                /*
                     * Toutes les guerres ont été traitées.
                     */
                if (*remaining == 0) {
                    then();
                    return;
                }

                /*
                     * Lance jusqu'à MAX_CONCURRENT_REQUESTS.
                     */
                while (*activeRequests <
                           MAX_CONCURRENT_REQUESTS &&
                       *nextIndex < warIds->size())
                {
                    const qint64 warId =
                        warIds->at(*nextIndex);

                    ++(*nextIndex);
                    ++(*activeRequests);

                    const QUrl warUrl(
                        QString(
                            "https://esi.evetech.net/latest/"
                            "wars/%1/"
                            "?datasource=tranquility"
                            ).arg(warId)
                        );

                    QNetworkRequest warRequest{warUrl};

                    warRequest.setHeader(
                        QNetworkRequest::UserAgentHeader,
                        "Engineering Red Panda"
                        );

                    QNetworkReply* warReply =
                        EsiManager::networkManager->get(warRequest);

                    QObject::connect(
                        warReply,
                        &QNetworkReply::finished,
                        [warReply,
                         warId,
                         activeRequests,
                         remaining,
                         failed,
                         processNext,
                         then,
                         errorListener]()
                        {
                            warReply->deleteLater();

                            --(*activeRequests);

                            /*
                                 * Erreur HTTP / réseau.
                                 */
                            if (warReply->error() !=
                                QNetworkReply::NoError)
                            {
                                if (!*failed) {
                                    *failed = true;

                                    errorListener(
                                        QString(
                                            "Unable to fetch "
                                            "war %1: %2"
                                            )
                                            .arg(warId)
                                            .arg(
                                                warReply
                                                    ->errorString()
                                                )
                                        );
                                }

                                return;
                            }

                            const QByteArray data =
                                warReply->readAll();

                            QJsonParseError parseError;

                            const QJsonDocument document =
                                QJsonDocument::fromJson(
                                    data,
                                    &parseError
                                    );

                            if (
                                parseError.error !=
                                    QJsonParseError::NoError ||
                                !document.isObject()
                                ) {
                                if (!*failed) {
                                    *failed = true;

                                    errorListener(
                                        QString(
                                            "Invalid ESI "
                                            "response for war %1: %2"
                                            )
                                            .arg(warId)
                                            .arg(
                                                parseError
                                                    .errorString()
                                                )
                                        );
                                }

                                return;
                            }

                            const QJsonObject war =
                                document.object();

                            const QJsonObject aggressor =
                                war.value("aggressor")
                                    .toObject();

                            const QJsonObject defender =
                                war.value("defender")
                                    .toObject();

                            /*
                                 * Pour une guerre d'alliance,
                                 * ESI fournit alliance_id.
                                 */
                            const qint64 aggressorId =
                                aggressor
                                    .value("alliance_id")
                                    .toVariant()
                                    .toLongLong();

                            const qint64 victimId =
                                defender
                                    .value("alliance_id")
                                    .toVariant()
                                    .toLongLong();

                            /*
                                 * On ne stocke que les guerres
                                 * Alliance vs Alliance.
                                 */
                            if (aggressorId > 0 &&
                                victimId > 0)
                            {
                                bool ok = false;

                                requestERP(
                                    R"(
                                            INSERT INTO wars
                                                (warId, agressorId, victimId)
                                            VALUES
                                                (:warId,
                                                 :agressorId,
                                                 :victimId)
                                            ON CONFLICT(warId)
                                            DO UPDATE SET
                                                agressorId =
                                                    excluded.agressorId,
                                                victimId =
                                                    excluded.victimId
                                        )",
                                    {
                                        { "warId", warId },
                                        { "agressorId", aggressorId },
                                        { "victimId", victimId }
                                    },
                                    &ok
                                    );

                                if (!ok) {
                                    if (!*failed) {
                                        *failed = true;

                                        errorListener(
                                            QString(
                                                "Unable to insert "
                                                "war %1 into database."
                                                ).arg(warId)
                                            );
                                    }

                                    return;
                                }
                            }

                            --(*remaining);

                            /*
                                 * Lance les suivantes.
                                 */
                            (*processNext)();
                        }
                        );
                }
            };

            (*processNext)();
        }
        );
}

void EsiConnector::fetchRecentKillmails(
    std::function<void(const QList<KillmailReference>&)> receiver,
    ERROR_LISTENER_CPP)
{

    if (!EsiManager::isInDataThread()) {
        QObject* temp = new QObject;

        EsiManager::TASK_MANAGER->addTask("Fetching Recent Killmails",
            [this, receiver, errorListener, temp]() mutable
            {
                fetchRecentKillmails(
                    [receiver, temp](const QList<KillmailReference>& result)
                    {
                        QMetaObject::invokeMethod(
                            temp,
                            [receiver, result, temp]() mutable
                            {
                                receiver(result);
                                temp->deleteLater();
                            },
                            Qt::QueuedConnection
                            );
                    }, errorListener
                    );
            }
            );

        return;
    }

    if (!isLoggedIn())
        return errorListener("Connector not Logged In");

    const qint64 characterId = m_characterId;

    if (characterId <= 0) {
        errorListener("Invalid character ID.");
        return;
    }

    const QString urlStr =
        QString(
            "https://esi.evetech.net/latest/"
            "characters/%1/killmails/recent/"
            "?datasource=tranquility"
            ).arg(characterId);

    QNetworkRequest request{QUrl(urlStr)};

    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        "Engineering Red Panda"
        );

    request.setRawHeader(
        "Authorization",
        QString("Bearer %1").arg(accessToken()).toUtf8()
        );

    QNetworkReply* reply = m_network.get(request);

    QObject::connect(
        reply,
        &QNetworkReply::finished,
        [reply, receiver, errorListener]()
        {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                errorListener(
                    "Unable to fetch recent killmails: " +
                    reply->errorString()
                    );
                return;
            }

            const QByteArray data = reply->readAll();

            QJsonParseError parseError;
            const QJsonDocument document =
                QJsonDocument::fromJson(data, &parseError);

            if (parseError.error != QJsonParseError::NoError ||
                !document.isArray())
            {
                errorListener(
                    "Invalid ESI killmails response: " +
                    parseError.errorString()
                    );
                return;
            }

            QList<KillmailReference> killmails;

            for (const QJsonValue& value : document.array()) {
                const QJsonObject object = value.toObject();

                KillmailReference killmail;

                killmail.id =
                    object.value("killmail_id")
                        .toVariant()
                        .toLongLong();

                killmail.hash =
                    object.value("killmail_hash").toString();

                killmails.append(killmail);
            }

            receiver(killmails);
        }
        );
}

void EsiManager::isKillmailOnZkillboard(
    qint64 killmailId,
    std::function<void(bool)> receiver,
    ERROR_LISTENER_CPP)
{
    if (!isInDataThread()) {
        QObject* temp = new QObject;

        TASK_MANAGER->addTask("Checking Killmail",
            [killmailId, receiver, errorListener, temp]() mutable
            {
                EsiManager::isKillmailOnZkillboard(killmailId,
                    [receiver, temp](bool b)
                    {
                        QMetaObject::invokeMethod(
                            temp,
                            [receiver, b, temp]() mutable
                            {
                                receiver(b);
                                temp->deleteLater();
                            },
                            Qt::QueuedConnection
                            );
                    }, errorListener
                    );
            }
            );

        return;
    }

    if (killmailId <= 0) {
        errorListener("Invalid killmail ID.");
        return;
    }

    const QString urlStr =
        QString("https://zkillboard.com/api/killID/%1/")
            .arg(killmailId);

    QNetworkRequest request{QUrl(urlStr)};

    request.setRawHeader(
        "User-Agent",
        "Engineering Red Panda"
        );

    QNetworkReply* reply = networkManager->get(request);

    QObject::connect(
        reply,
        &QNetworkReply::finished,
        [reply, receiver, errorListener]()
        {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                errorListener(
                    "Unable to query zKillboard: " +
                    reply->errorString()
                    );
                return;
            }

            const QByteArray data = reply->readAll();

            QJsonParseError parseError;
            const QJsonDocument document =
                QJsonDocument::fromJson(data, &parseError);

            if (parseError.error != QJsonParseError::NoError ||
                !document.isArray())
            {
                errorListener(
                    "Invalid zKillboard response: " +
                    parseError.errorString()
                    );
                return;
            }

            receiver(!document.array().isEmpty());
        }
        );
}

void EsiManager::fetchKillmail(
    qint64 killmailId,
    const QString& killmailHash,
    std::function<void(const Killmail&)> receiver,
    ERROR_LISTENER_CPP)
{
    if (!isInDataThread()) {
        QObject* temp = new QObject;

        TASK_MANAGER->addTask("Fetching Killmail",
            [receiver, killmailId, killmailHash, errorListener, temp]() mutable
            {
                EsiManager::fetchKillmail(
                    killmailId, killmailHash,
                    [killmailId, killmailHash, receiver, errorListener, temp](const Killmail& result)
                    {
                        QMetaObject::invokeMethod(
                            temp,
                            [receiver, temp, result]() mutable
                            {
                                receiver(result);
                                temp->deleteLater();
                            },
                            Qt::QueuedConnection
                            );
                    }, errorListener
                    );
            }
            );

        return;
    }

    const QString urlStr =
        QString(
            "https://esi.evetech.net/latest/killmails/%1/%2/"
            "?datasource=tranquility"
            )
            .arg(killmailId)
            .arg(killmailHash);

    QNetworkRequest request{QUrl(urlStr)};

    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        "Engineering Red Panda"
        );

    QNetworkReply* reply = networkManager->get(request);

    QObject::connect(
        reply,
        &QNetworkReply::finished,
        [reply, killmailId, killmailHash,
         receiver, errorListener]()
        {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                errorListener(
                    "Unable to fetch killmail " +
                    QString::number(killmailId) +
                    ": " +
                    reply->errorString()
                    );
                return;
            }

            const QByteArray data = reply->readAll();

            QJsonParseError parseError;

            const QJsonDocument document =
                QJsonDocument::fromJson(
                    data,
                    &parseError
                    );

            if (parseError.error !=
                QJsonParseError::NoError)
            {
                errorListener(
                    "Invalid killmail JSON: " +
                    parseError.errorString()
                    );
                return;
            }

            if (!document.isObject()) {
                errorListener(
                    "Invalid killmail response."
                    );
                return;
            }

            const QJsonObject object =
                document.object();

            Killmail killmail;

            /*
             * Référence du killmail
             */
            killmail.ref.id = killmailId;
            killmail.ref.hash = killmailHash;

            /*
             * Date
             */
            const QString time =
                object.value("killmail_time").toString();

            killmail.date =
                QDateTime::fromString(
                    time,
                    Qt::ISODate
                    ).date();

            /*
             * Système solaire
             */
            killmail.systemId =
                object.value("solar_system_id")
                    .toVariant()
                    .toLongLong();

            /*
             * Victime
             */
            const QJsonObject victim =
                object.value("victim").toObject();

            CharacterView victimView;

            victimView.id =
                victim.value("character_id")
                    .toVariant()
                    .toLongLong();

            /*
             * Type du vaisseau détruit
             */
            killmail.deadTypeId =
                victim.value("ship_type_id")
                    .toVariant()
                    .toLongLong();

            killmail.victim = victimView;

            /*
             * Attaquants
             */
            const QJsonArray attackers =
                object.value("attackers").toArray();

            killmail.agressors.clear();
            killmail.finalBlowId = -1;

            for (int i = 0; i < attackers.size(); ++i) {

                const QJsonObject attacker =
                    attackers.at(i).toObject();

                CharacterView attackerView;

                attackerView.id =
                    attacker.value("character_id")
                        .toVariant()
                        .toLongLong();

                killmail.agressors.append(
                    attackerView
                    );

                /*
                 * ESI garantit normalement qu'un seul
                 * attacker possède final_blow = true.
                 */
                if (attacker.value("final_blow").toBool()) {
                    killmail.finalBlowId = i;
                }
            }

            receiver(killmail);
        }
        );
}

void EsiConnector::writeAllianceStandings(
    const QMap<qint64, double>& standings,
    ERROR_LISTENER_CPP)
{
    if (!EsiManager::isInDataThread()) {
        EsiManager::TASK_MANAGER->addTaskAndWait("Writing Alliance Standings", [this, standings, errorListener] () mutable { writeAllianceStandings(standings, errorListener); return 0; });
        return;
    }

    if (!isLoggedIn())
        return errorListener("Connector not Logged In");

    if (m_characterId <= 0)
        return errorListener("Invalid character ID.");

    if (standings.isEmpty())
        return;

    // ESI accepte plusieurs contacts dans une même requête,
    // mais tous doivent recevoir le même standing.
    //
    // On regroupe donc les alliances par valeur de standing.
    QMap<double, QJsonArray> groupedStandings;

    for (auto [allianceId, standing] : standings.asKeyValueRange()) {
        if (allianceId <= 0) {
            errorListener(
                QString("Invalid alliance ID: %1")
                    .arg(allianceId));
            return;
        }

        if (standing < -10.0 || standing > 10.0) {
            errorListener(
                QString("Invalid standing %1 for alliance %2. "
                        "Standing must be between -10 and 10.")
                    .arg(standing)
                    .arg(allianceId));
            return;
        }

        groupedStandings[standing].append(allianceId);
    }

    auto remaining =
        std::make_shared<int>(groupedStandings.size());

    auto failed =
        std::make_shared<bool>(false);

    for (auto [standing, allianceIds] : groupedStandings.asKeyValueRange()) {

        const QString urlStr =
            QString(
                "https://esi.evetech.net/latest/"
                "characters/%1/contacts/"
                "?datasource=tranquility"
                "&standing=%2"
                )
                .arg(m_characterId)
                .arg(QLocale::c().toString(standing, 'f', 10));

        QNetworkRequest request{QUrl(urlStr)};

        request.setHeader(
            QNetworkRequest::UserAgentHeader,
            "Engineering Red Panda");

        request.setHeader(
            QNetworkRequest::ContentTypeHeader,
            "application/json");

        request.setRawHeader(
            "Authorization",
            QString("Bearer %1")
                .arg(accessToken())
                .toUtf8());

        QNetworkReply* reply =
            m_network.put(
                request,
                QJsonDocument(allianceIds)
                    .toJson(QJsonDocument::Compact));

        QObject::connect(
            reply,
            &QNetworkReply::finished,
            this,
            [reply,
             standing,
             remaining,
             failed,
             errorListener]()
            {
                reply->deleteLater();

                if (reply->error() != QNetworkReply::NoError) {

                    if (!*failed) {
                        *failed = true;

                        const auto statusCode =
                            reply->attribute(
                                     QNetworkRequest::
                                     HttpStatusCodeAttribute)
                                .toInt();

                        errorListener(
                            QString(
                                "Unable to set alliance standings "
                                "(standing %1, HTTP %2): %3")
                                .arg(standing)
                                .arg(statusCode)
                                .arg(reply->errorString()));
                    }

                    return;
                }

                --(*remaining);

                // Toutes les requêtes sont terminées avec succès.
                if (*remaining == 0 && !*failed) {
                    // Rien à appeler : ERROR_LISTENER est utilisé
                    // uniquement en cas d'erreur.
                }
            });
    }
}

bool EsiManager::isContainer(qint64 typeId) {
    static QSet<qint64> CONTAINERS_TYPE_ID = {
       23,
       2263,
       3293,
       3296,
       3297,
       41567,
       3465,
       3466,
       3467,
       11488,
       11489,
       11490,
       56362,
       17363,
       17364,
       17365,
       17366,
       17367,
       17368,
       3468,
       24445,
       33003,
       33005,
       33007,
       33009,
       33011
    };

    return CONTAINERS_TYPE_ID.contains(typeId);
}

void EsiConnector::removeAllianceContacts(
    const QList<qint64>& allianceIds,
    std::function<void()> receiver,
    ERROR_LISTENER_CPP)
{

    if (!EsiManager::isInDataThread()) {
        QObject* temp = new QObject;

        EsiManager::TASK_MANAGER->addTask("Removing Alliance Contacts",
            [this, allianceIds, receiver, errorListener, temp]() mutable
            {
                removeAllianceContacts(
                    allianceIds,
                    [receiver, temp]()
                    {
                        QMetaObject::invokeMethod(
                            temp,
                            [receiver, temp]() mutable
                            {
                                receiver();
                                temp->deleteLater();
                            },
                            Qt::QueuedConnection
                            );
                    }, errorListener
                    );
            }
            );

        return;
    }

    if (!isLoggedIn())
        return errorListener("Connector not Logged In");

    if (m_characterId <= 0)
        return errorListener("Invalid character ID.");

    if (allianceIds.isEmpty()) {
        receiver();
        return;
    }

    QJsonArray contacts;

    for (const qint64 allianceId : allianceIds) {
        if (allianceId <= 0)
            return errorListener(
                QString("Invalid alliance ID: %1")
                    .arg(allianceId));

        contacts.append(allianceId);
    }

    const QString urlStr =
        QString(
            "https://esi.evetech.net/latest/"
            "characters/%1/contacts/"
            "?datasource=tranquility"
            ).arg(m_characterId);

    QNetworkRequest request{QUrl(urlStr)};

    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        "Engineering Red Panda");

    request.setHeader(
        QNetworkRequest::ContentTypeHeader,
        "application/json");

    request.setRawHeader(
        "Authorization",
        QString("Bearer %1")
            .arg(accessToken())
            .toUtf8());

    QNetworkReply* reply =
        m_network.sendCustomRequest(
            request,
            "DELETE",
            QJsonDocument(contacts)
                .toJson(QJsonDocument::Compact));

    QObject::connect(
        reply,
        &QNetworkReply::finished,
        this,
        [reply, receiver, errorListener]()
        {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                const int statusCode =
                    reply->attribute(
                             QNetworkRequest::HttpStatusCodeAttribute)
                        .toInt();

                errorListener(
                    QString(
                        "Unable to remove alliance contacts "
                        "(HTTP %1): %2")
                        .arg(statusCode)
                        .arg(reply->errorString()));
                return;
            }

            receiver();
        });
}


void EsiConnector::fetchContainers(
    const QString& key,
    std::function<void(const QList<Container>&)> receiver,
    ERROR_LISTENER_CPP)
{
    if (!EsiManager::isInDataThread()) {
        QObject* temp = new QObject;

        EsiManager::TASK_MANAGER->addTask("Fetching Containers",
            [this, key, receiver, errorListener, temp]() mutable
            {
                fetchContainers(
                    key,
                    [receiver, temp](const QList<Container>& result)
                    {
                        QMetaObject::invokeMethod(
                            temp,
                            [receiver, result, temp]() mutable
                            {
                                receiver(result);
                                temp->deleteLater();
                            },
                            Qt::QueuedConnection
                            );
                    }, errorListener
                    );
            }
            );

        return;
    }

    EsiManager::requestERP("DELETE FROM assetscache WHERE characterId = :charid", {{"charid", m_characterId}});

    if (!isLoggedIn()) {
        return errorListener("Connector not Logged In");
    }

    if (m_characterId <= 0) {
        return errorListener("Invalid character ID.");
    }

    const qint64 characterId = m_characterId;

    /*
     * Tous les assets du personnage.
     *
     * /assets/ est paginé : on récupère donc toutes les pages.
     */
    auto allAssets =
        std::make_shared<QList<QJsonObject>>();

    auto fetchPage =
        std::make_shared<std::function<void(int)>>();

    *fetchPage =
        [this,
         characterId,
         key,
         receiver,
         errorListener,
         allAssets,
         fetchPage](int page)
    {
        const QString urlStr =
            QString(
                "https://esi.evetech.net/latest/"
                "characters/%1/assets/"
                "?datasource=tranquility"
                "&page=%2"
                )
                .arg(characterId)
                .arg(page);

        QNetworkRequest request{QUrl(urlStr)};

        request.setHeader(
            QNetworkRequest::UserAgentHeader,
            "Engineering Red Panda"
            );

        request.setRawHeader(
            "Authorization",
            QString("Bearer %1")
                .arg(accessToken())
                .toUtf8()
            );

        QNetworkReply* reply =
            EsiManager::networkManager->get(request);

        connect(
            reply,
            &QNetworkReply::finished,
            this,
            [this,
             reply,
             page,
             key,
             receiver,
             errorListener,
             allAssets,
             fetchPage, characterId]()
            {
                reply->deleteLater();

                if (reply->error() !=
                    QNetworkReply::NoError)
                {
                    return errorListener(
                        "Unable to fetch character assets: " +
                        reply->errorString()
                        );
                }

                const QByteArray data =
                    reply->readAll();

                QJsonParseError parseError;

                const QJsonDocument document =
                    QJsonDocument::fromJson(
                        data,
                        &parseError
                        );

                if (parseError.error !=
                        QJsonParseError::NoError ||
                    !document.isArray())
                {
                    return errorListener(
                        "Invalid ESI assets response: " +
                        parseError.errorString()
                        );
                }

                const QJsonArray assets =
                    document.array();

                /*
                 * Ajoute les assets de cette page.
                 */
                for (const QJsonValue& value : assets) {
                    if (value.isObject()) {
                        allAssets->append(
                            value.toObject()
                            );
                    }
                }

                /*
                 * Nombre total de pages.
                 */
                const QByteArray xPages =
                    reply->rawHeader("X-Pages");

                const int totalPages =
                    xPages.isEmpty()
                        ? 1
                        : xPages.toInt();

                Util::println("Total Pages: ", totalPages);

                /*
                 * Page suivante.
                 */
                if (page < totalPages) {
                    (*fetchPage)(page + 1);
                    return;
                }

                /*
                 * --------------------------------------------------
                 * Tous les assets sont maintenant récupérés.
                 * --------------------------------------------------
                 */

                /*
                 * Index des assets par item_id.
                 *
                 * Cela évite de parcourir tous les assets à chaque
                 * fois qu'un nom est reçu.
                 */
                auto assetsById =
                    std::make_shared<QHash<qint64, QJsonObject>>();

                /*
                 * Seuls les containers sont envoyés à
                 * /assets/names/.
                 */
                QJsonArray containerIds;

                for (const QJsonObject& asset : *allAssets)
                {
                    CharacterAsset characterAsset;

                    characterAsset.itemId =
                        asset.value("item_id")
                            .toVariant()
                            .toLongLong();

                    characterAsset.typeId =
                        asset.value("type_id")
                            .toVariant()
                            .toLongLong();

                    characterAsset.quantity =
                        asset.value("quantity")
                            .toVariant()
                            .toLongLong();

                    characterAsset.locationId =
                        asset.value("location_id")
                            .toVariant()
                            .toLongLong();

                    characterAsset.locationType =
                        asset.value("location_type")
                            .toString();

                    characterAsset.locationFlag =
                        asset.value("location_flag")
                            .toString();

                    characterAsset.isSingleton =
                        asset.value("is_singleton")
                            .toBool();

                    if (characterAsset.itemId <= 0) {
                        continue;
                    }

                    /*
                     * Sauvegarde de l'asset dans le cache ERP.
                     */
                                    EsiManager::requestERP(
                                        R"(
                            INSERT INTO assetscache (
                                typeId,
                                itemId,
                                locationId,
                                quantity,
                                locationType,
                                locationFlag,
                                isSingleton,
                                characterId
                            )
                            VALUES (
                                :typeId,
                                :itemId,
                                :locationId,
                                :quantity,
                                :locationType,
                                :locationFlag,
                                :isSingleton,
                                :characterId
                            )
                        )",
                        {
                            {"typeId",       characterAsset.typeId},
                            {"itemId",       characterAsset.itemId},
                            {"locationId",   characterAsset.locationId},
                            {"quantity",     characterAsset.quantity},
                            {"locationType", characterAsset.locationType},
                            {"locationFlag", characterAsset.locationFlag},
                            {"isSingleton",  characterAsset.isSingleton ? 1 : 0},
                            {"characterId",  characterId}
                        }
                        );

                    (*assetsById)[characterAsset.itemId] = asset;

                    /*
                     * IMPORTANT :
                     * On ne demande le nom personnalisé
                     * QUE pour les containers.
                     */
                    if (!EsiManager::isContainer(characterAsset.typeId)) {
                        continue;
                    }

                    containerIds.append(characterAsset.itemId);
                }

                /*
                 * Aucun container trouvé.
                 */
                if (containerIds.isEmpty()) {
                    Util::println("No Container Found");
                    receiver({});
                    return;
                }

                /*
                 * ESI accepte les IDs par lots.
                 */
                constexpr int BATCH_SIZE = 1000;

                auto containers =
                    std::make_shared<QList<Container>>();

                auto processBatch =
                    std::make_shared<std::function<void(int)>>();

                *processBatch =
                    [this,
                     characterId,
                     key,
                     containerIds,
                     assetsById,
                     containers,
                     receiver,
                     errorListener,
                     processBatch](int offset)
                {
                    if (offset >= containerIds.size()) {
                        receiver(*containers);
                        return;
                    }

                    const int end =
                        qMin(
                            offset + BATCH_SIZE,
                            containerIds.size()
                            );

                    QJsonArray batch;

                    for (int i = offset; i < end; ++i) {
                        batch.append(
                            containerIds.at(i)
                            );
                    }

                    const QString namesUrl =
                        QString(
                            "https://esi.evetech.net/latest/"
                            "characters/%1/assets/names/"
                            "?datasource=tranquility"
                            )
                            .arg(characterId);

                    QNetworkRequest request{
                        QUrl(namesUrl)
                    };

                    request.setHeader(
                        QNetworkRequest::UserAgentHeader,
                        "Engineering Red Panda"
                        );

                    request.setHeader(
                        QNetworkRequest::ContentTypeHeader,
                        "application/json"
                        );

                    request.setRawHeader(
                        "Authorization",
                        QString("Bearer %1")
                            .arg(accessToken())
                            .toUtf8()
                        );

                    QNetworkReply* reply =
                        m_network.post(
                            request,
                            QJsonDocument(batch)
                                .toJson(
                                    QJsonDocument::Compact
                                    )
                            );

                    connect(
                        reply,
                        &QNetworkReply::finished,
                        this,
                        [reply,
                         key,
                         assetsById,
                         containers,
                         receiver,
                         errorListener,
                         processBatch,
                         end]()
                        {
                            reply->deleteLater();

                            if (reply->error() !=
                                QNetworkReply::NoError)
                            {
                                return errorListener(
                                    "Unable to fetch asset names: " +
                                    reply->errorString()
                                    );
                            }

                            const QByteArray data =
                                reply->readAll();

                            QJsonParseError parseError;

                            const QJsonDocument document =
                                QJsonDocument::fromJson(
                                    data,
                                    &parseError
                                    );

                            if (parseError.error !=
                                    QJsonParseError::NoError ||
                                !document.isArray())
                            {
                                return errorListener(
                                    "Invalid ESI asset names response: " +
                                    parseError.errorString()
                                    );
                            }

                            /*
                             * ESI renvoie :
                             *
                             * {
                             *     "item_id": ...,
                             *     "name": "..."
                             * }
                             */
                            for (const QJsonValue& value :
                                 document.array())
                            {
                                const QJsonObject namedAsset =
                                    value.toObject();

                                const qint64 itemId =
                                    namedAsset
                                        .value("item_id")
                                        .toVariant()
                                        .toLongLong();

                                const QString name =
                                    namedAsset
                                        .value("name")
                                        .toString();

                                /*
                                 * Filtre sur le nom personnalisé.
                                 *
                                 * Exemple :
                                 * REPAM | T2 Blueprints
                                 */
                                if (!name.startsWith(
                                        key + " |", Qt::CaseInsensitive))
                                {
                                    continue;
                                }

                                const auto it =
                                    assetsById->constFind(itemId);

                                if (it ==
                                    assetsById->constEnd())
                                {
                                    continue;
                                }

                                const QJsonObject& asset =
                                    it.value();

                                Container container;

                                container.itemId =
                                    itemId;

                                container.typeId =
                                    asset.value("type_id")
                                        .toVariant()
                                        .toLongLong();

                                container.quantity =
                                    asset.value("quantity")
                                        .toVariant()
                                        .toLongLong();

                                container.locationId =
                                    asset.value("location_id")
                                        .toVariant()
                                        .toLongLong();

                                container.locationType =
                                    asset.value("location_type")
                                        .toString();

                                container.locationFlag =
                                    asset.value("location_flag")
                                        .toString();

                                container.isSingleton =
                                    asset.value("is_singleton")
                                        .toBool();

                                container.name =
                                    name;

                                /*
                                 * Le contenu sera récupéré
                                 * ultérieurement.
                                 */
                                container.contents = {};

                                containers->append(
                                    container
                                    );
                            }

                            /*
                             * Batch suivant.
                             */
                            (*processBatch)(end);
                        }
                        );
                };

                (*processBatch)(0);
            }
            );
    };

    /*
     * Les pages ESI commencent à 1.
     */
    (*fetchPage)(1);
}

CharacterAsset EsiManager::loadCharacterAsset(qint64 itemId, qint64 characterId) {
    // Pas besoin de thread check, car assumé par les fonctions appelées
    bool ok = false;
    QSqlQuery query = EsiManager::requestERP("SELECT * FROM assetscache WHERE itemId = :itemId AND characterId = :characterId", {{"itemId", itemId}, {"characterId", characterId}}, &ok);
    if (ok && query.next()) {
        return CharacterAsset::parse(&query);
    } else return CharacterAsset();
}

BlueprintAsset EsiManager::loadBlueprint(qint64 itemId, qint64 characterId) {
    // Pas besoin de thread check, car assumé par les fonctions appelées
    CharacterAsset asset = loadCharacterAsset(itemId, characterId);
    if (asset.isNull()) return BlueprintAsset{};

    bool ok = false;
    QSqlQuery query = EsiManager::requestERP("SELECT * FROM blueprintscache WHERE itemId = :itemId", {{"itemId", itemId}}, &ok);
    if (!ok || !query.next()) return BlueprintAsset{};

    return BlueprintAsset{asset, query.value("materialEfficiency").toInt(), query.value("timeEfficiency").toInt(), query.value("bpc").toBool(), query.value("runsRemaining").toInt()};
}

Container EsiManager::loadContainer(const QString& name, qint64 characterId) {
    if (!isInDataThread()) {
        return TASK_MANAGER->addTaskAndWait("Loading Container", [name, characterId] { return EsiManager::loadContainer(name, characterId); });
    }
    bool ok = false;
    QSqlQuery query = EsiManager::requestERP("SELECT itemId FROM erpmarketcontainers WHERE containerName = :contName AND characterId = :charId", {{"contName", name}, {"charId", characterId}}, &ok);

    if (!ok || !query.next()) {
        Util::error("No container found");
        return Container{};
    }

    qint64 itemId = query.value("itemId").toLongLong();

    Container work = Container{EsiManager::loadCharacterAsset(itemId, characterId), name, {}};
    if (work.isNull()) Util::println("Error... ", itemId, " ", characterId);
    ok = false;
    query = EsiManager::requestERP("SELECT * FROM assetscache WHERE locationId = :locId AND characterId = :charId", {{"locId", itemId}, {"charId", characterId}}, &ok);

    if (!ok) return work;

    while (query.next()) {
        work.contents.append(CharacterAsset::parse(&query));
    }

    return work;
}

void EsiConnector::fetchContainerContents(
    qint64 containerId,
    std::function<void(const QList<CharacterAsset>&)> receiver,
    ERROR_LISTENER_CPP)
{
    bool ok = false;
    QSqlQuery work = EsiManager::requestERP("SELECT * FROM assetscache WHERE locationId = :contId AND characterId = :charId", {{"contId", containerId}, {"charId", m_characterId}}, &ok, [errorListener] (QSqlError err) { errorListener(err.text()); });
    QList<CharacterAsset> res = {};
    while (work.next()) {
        res.append(CharacterAsset::parse(&work));
    }
    receiver(res);
}

void EsiManager::checkForEnd(int* counter, QList<Container>* list, std::function<void(const QList<Container>&)> receiver) {
    if (*counter <= 0) {
        QList<Container> work = QList<Container>(*list);
        delete counter;
        delete list;
        receiver(work);
    }
}


void EsiConnector::fetchBlueprints(
    std::function<void()> receiver,
    ERROR_LISTENER_CPP)
{
    if (!EsiManager::isInDataThread()) {
        QObject* temp = new QObject;

        EsiManager::TASK_MANAGER->addTask("Fetching Blueprints",
            [this, receiver, errorListener, temp]() mutable
            {
                fetchBlueprints(
                    [receiver, temp]()
                    {
                        QMetaObject::invokeMethod(
                            temp,
                            [receiver, temp]() mutable
                            {
                                receiver();
                                temp->deleteLater();
                            },
                            Qt::QueuedConnection
                            );
                    }, errorListener
                    );
            }
            );

        return;
    }

    if (!isLoggedIn()) {
        return errorListener("Connector not Logged In");
    }

    if (m_characterId <= 0) {
        return errorListener("Invalid character ID.");
    }

    const qint64 characterId = m_characterId;

    auto blueprints = std::make_shared<QList<QJsonObject>>();
    auto fetchPage = std::make_shared<std::function<void(int)>>();

    *fetchPage =
        [this, characterId, receiver, errorListener, blueprints, fetchPage](int page)
    {
        const QString urlStr =
            QString(
                "https://esi.evetech.net/latest/"
                "characters/%1/blueprints/"
                "?datasource=tranquility"
                "&page=%2"
                )
                .arg(characterId)
                .arg(page);

        QNetworkRequest request{QUrl(urlStr)};

        request.setHeader(
            QNetworkRequest::UserAgentHeader,
            "Engineering Red Panda"
            );

        request.setRawHeader(
            "Authorization",
            QString("Bearer %1").arg(accessToken()).toUtf8()
            );

        QNetworkReply* reply = m_network.get(request);

        connect(reply, &QNetworkReply::finished, this,
                [this, reply, page, receiver, errorListener,
                 blueprints, fetchPage]()
                {
                    reply->deleteLater();

                    if (reply->error() != QNetworkReply::NoError) {
                        return errorListener(
                            "Unable to fetch character blueprints: " +
                            reply->errorString()
                            );
                    }

                    const QByteArray data = reply->readAll();

                    QJsonParseError parseError;
                    const QJsonDocument document =
                        QJsonDocument::fromJson(data, &parseError);

                    if (parseError.error != QJsonParseError::NoError ||
                        !document.isArray())
                    {
                        return errorListener(
                            "Invalid ESI blueprints response: " +
                            parseError.errorString()
                            );
                    }

                    for (const QJsonValue& value : document.array()) {
                        if (value.isObject())
                            blueprints->append(value.toObject());
                    }

                    const QByteArray xPages =
                        reply->rawHeader("X-Pages");

                    const int totalPages =
                        xPages.isEmpty() ? 1 : xPages.toInt();

                    Util::println(
                        "Blueprints page ",
                        page,
                        "/",
                        totalPages
                        );

                    if (page < totalPages) {
                        (*fetchPage)(page + 1);
                        return;
                    }

                    // ---------------------------------------------------------
                    // Toutes les pages ont été récupérées.
                    // On remplace le cache ERP.
                    // ---------------------------------------------------------

                    EsiManager::ERP.transaction();
                    for (const QJsonObject& blueprint : *blueprints)
                    {
                        const qint64 itemId =
                            blueprint.value("item_id")
                                .toVariant()
                                .toLongLong();

                        if (itemId <= 0)
                            continue;

                        const int quantity =
                            blueprint.value("quantity").toInt();

                        const bool isBPC =
                            (quantity == -2);

                        const double materialEfficiency =
                            blueprint.value("material_efficiency").toDouble();

                        const double timeEfficiency =
                            blueprint.value("time_efficiency").toDouble();

                        const int runsRemaining =
                            blueprint.value("runs").toInt();

                        EsiManager::requestERP(
                            R"(
                            INSERT INTO blueprintscache (
                                itemId,
                                bpc,
                                materialEfficiency,
                                timeEfficiency,
                                runsRemaining
                            )
                            VALUES (
                                :itemId,
                                :bpc,
                                :materialEfficiency,
                                :timeEfficiency,
                                :runsRemaining
                            )
                        )",
                            {
                                {"itemId", itemId},
                                {"bpc", isBPC ? 1 : 0},
                                {"materialEfficiency", materialEfficiency},
                                {"timeEfficiency", timeEfficiency},
                                {"runsRemaining", runsRemaining}
                            }
                            );
                    }
                    EsiManager::ERP.commit();

                    Util::println(
                        "Blueprint cache updated: ",
                        blueprints->size(),
                        " blueprints."
                        );

                    receiver();
                });
    };

    (*fetchPage)(1);
}

QString EsiManager::getSystemName(qint64 solarSystemId) {
    // Pas besoin de thread check, car assumé par les fonctions appelées
    bool ok = false;
    QSqlQuery query = EsiManager::requestSDE("SELECT solarSystemName FROM mapsolarsystems WHERE solarSystemId = :id", {{"id", solarSystemId}}, &ok);
    if (ok && query.next()) return query.value("solarSystemName").toString();
    else return QString();
}
