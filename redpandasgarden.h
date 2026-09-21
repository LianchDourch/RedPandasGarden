#ifndef REDPANDASGARDEN_H
#define REDPANDASGARDEN_H

#include "asyncmanager.h"
#include "core.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

class RedPandasGarden
{
public:
    inline static QNetworkAccessManager* networkManager = nullptr;
    inline static AsyncTaskManager* TASK_MANAGER = nullptr;

    static void preInit() {
        TASK_MANAGER = new AsyncTaskManager(QCoreApplication::instance());
        TASK_MANAGER->addTask("Repag Pre Init", [] () { RedPandasGarden::networkManager = new QNetworkAccessManager{}; });
    }

    static void postInit() {

    }

    static void createSellOrder(
        const QString& repamToken,
        qint64 typeId,
        qint64 itemId,
        qint64 solarSystemId,
        std::shared_ptr<ItemPrice> price,
        int initialAmount,
        const QString& description,
        std::function<void(QJsonObject)> receiver = [] (QJsonObject) {}
        )
    {
        QUrl url("https://market.redpandasgarden.com/createorder.php");

        QNetworkRequest request(url);

        request.setHeader(
            QNetworkRequest::ContentTypeHeader,
            "application/x-www-form-urlencoded"
            );

        QUrlQuery postData;

        postData.addQueryItem(
            "pwkey",
            repamToken
            );

        postData.addQueryItem(
            "typeId",
            QString::number(typeId)
            );

        postData.addQueryItem(
            "itemId",
            QString::number(itemId)
            );

        postData.addQueryItem(
            "solarSystemId",
            QString::number(solarSystemId)
            );

        postData.addQueryItem(
            "priceType",
            QString::number(price->getPriceType())
            );

        postData.addQueryItem(
            "initialAmount",
            QString::number(initialAmount)
            );

        postData.addQueryItem(
            "description",
            description
            );

        QNetworkReply* reply = networkManager->post(
            request,
            postData.toString(QUrl::FullyEncoded).toUtf8()
            );

        QObject::connect(
            reply,
            &QNetworkReply::finished,
            [reply, receiver, price, repamToken]()
            {
                const QByteArray response = reply->readAll();

                if (reply->error() != QNetworkReply::NoError)
                {
                    qWarning() << "Erreur réseau :"
                               << reply->errorString();

                    qWarning() << "Réponse serveur :"
                               << response;

                    reply->deleteLater();
                    return;
                }

                qDebug() << "Réponse serveur :"
                         << response;

                // Parse JSON
                QJsonParseError parseError;

                const QJsonDocument json =
                    QJsonDocument::fromJson(response, &parseError);

                if (parseError.error != QJsonParseError::NoError)
                {
                    qWarning() << "JSON invalide :"
                               << parseError.errorString();

                    reply->deleteLater();
                    return;
                }

                const QJsonObject object = json.object();

                const bool success =
                    object.value("success").toBool(false);

                if (success)
                {
                    price->upload(repamToken, object, receiver);
                }
                else
                {
                    const QString error =
                        object.value("error")
                            .toString("Erreur inconnue");

                    qWarning() << "Création du sell order refusée :"
                               << error;
                }

                reply->deleteLater();
            }
            );
    }


    static void createBlueprintSellOrder(
        const QString& repamToken,
        qint64 typeId,
        qint64 itemId,
        qint64 solarSystemId,
        std::shared_ptr<ItemPrice> price,
        int initialAmount,
        const QString& description,
        int materialEfficiency,
        int timeEfficiency,
        bool isBpc,
        int runsRemaining
        )
    {
        createSellOrder(
            repamToken, typeId, itemId, solarSystemId, price, initialAmount, description,
            [repamToken, itemId, materialEfficiency, timeEfficiency, isBpc, runsRemaining] (QJsonObject obj) {
                const qint64 orderId =
                    obj.value("orderId").toInteger();
                sendBlueprintSellOrderDatas(orderId, repamToken, itemId, materialEfficiency, timeEfficiency, isBpc, runsRemaining);
            });
    }


    /**
     * @brief fetchRepamContainers executes in REPAG thread
     * @param receiver will be executed in REPAG thread
     * @param errorListener
     */
    static void fetchRepamContainers(EsiConnector *c,
                              std::function<void(const QList<Container>&)> receiver,
                              ERROR_LISTENER
                              );

private:
    static void sendBlueprintSellOrderDatas(
        qint64 orderId,
        const QString& repamToken,
        qint64 itemId,
        int materialEfficiency,
        int timeEfficiency,
        bool isBpc,
        int runsRemaining)
    {
        QUrl url("https://market.redpandasgarden.com/makebporder.php");

        QNetworkRequest request(url);

        request.setHeader(
            QNetworkRequest::ContentTypeHeader,
            "application/x-www-form-urlencoded"
            );

        QUrlQuery postData;

        postData.addQueryItem(
            "pwkey",
            repamToken
            );

        postData.addQueryItem(
            "itemId",
            QString::number(itemId)
            );

        postData.addQueryItem(
            "orderId",
            QString::number(orderId)
            );

        postData.addQueryItem(
            "materialEfficiency",
            QString::number(materialEfficiency)
            );

        postData.addQueryItem(
            "timeEfficiency",
            QString::number(timeEfficiency)
            );

        postData.addQueryItem(
            "bpc",
            QString::number(isBpc)
            );

        postData.addQueryItem(
            "runsRemaining",
            QString::number(runsRemaining)
            );

        QNetworkReply* reply = networkManager->post(
            request,
            postData.toString(QUrl::FullyEncoded).toUtf8()
            );

        QObject::connect(
            reply,
            &QNetworkReply::finished,
            [reply]()
            {
                const QByteArray response = reply->readAll();

                if (reply->error() != QNetworkReply::NoError)
                {
                    qWarning() << "Erreur réseau :"
                               << reply->errorString();

                    qWarning() << "Réponse serveur :"
                               << response;

                    reply->deleteLater();
                    return;
                }

                qDebug() << "Réponse serveur :"
                         << response;

                // Parse JSON
                QJsonParseError parseError;

                const QJsonDocument json =
                    QJsonDocument::fromJson(response, &parseError);

                if (parseError.error != QJsonParseError::NoError)
                {
                    qWarning() << "JSON invalide :"
                               << parseError.errorString();

                    reply->deleteLater();
                    return;
                }

                const QJsonObject object = json.object();

                const bool success =
                    object.value("success").toBool(false);

                if (success)
                {
                    qDebug() << object;
                }
                else
                {
                    const QString error =
                        object.value("error")
                            .toString("Erreur inconnue");

                    qWarning() << "Création du sell order refusée :"
                               << error;
                }

                reply->deleteLater();
            }
            );
    }

};

#endif // REDPANDASGARDEN_H
