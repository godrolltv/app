#include "weaponloader.h"
#include "seasonmapping.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QDebug>
#include <QUrlQuery>

WeaponLoader::WeaponLoader(QObject *parent, QNetworkAccessManager *networkManager,
                           int requestTimeoutMs, int refreshTimeoutMs)
    : QObject(parent)
    , m_networkManager(networkManager ? networkManager : new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
    , m_timeoutTimer(new QTimer(this))
    , m_retryCount(0)
    , m_maxRetries(MAX_RETRIES)
    , m_requestTimeoutMs(requestTimeoutMs)
    , m_refreshTimeoutMs(refreshTimeoutMs)
{
    // Setup timeout timer
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &WeaponLoader::onTimeout);
}

void WeaponLoader::loadWeapons(std::function<void(const QJsonArray&)> callback,
                               int maxRetries)
{
    m_reloading = false;
    m_callback = callback;
    m_retryCount = 0;
    m_maxRetries = qMax(0, maxRetries);
    m_forceFreshRequest = false;
    m_schemaRefreshAttempted = false;
    
    startRequest();
}

void WeaponLoader::startRequest()
{
    // Cleanup any previous request
    cleanupCurrentRequest();
    const int timeoutMs = m_reloading
        ? m_refreshTimeoutMs - int(m_refreshElapsed.elapsed()) : m_requestTimeoutMs;
    if (timeoutMs <= 0) {
        failRequest("Could not connect to the weapon service.");
        return;
    }
    
    qDebug() << "Loading weapons from API..." << (m_retryCount > 0 ? QString("(retry %1/%2)").arg(m_retryCount).arg(m_maxRetries) : "");
    emit loadStatusChanged(m_retryCount > 0
        ? QString("Trying to load your weapons again (%1 of %2)...").arg(m_retryCount).arg(m_maxRetries)
        : QString("Connecting to Godroll.tv..."));
    
    // Version the response contract so an older HTTP cache entry cannot fill
    // the in-memory cache after antiChampionType was added.
    QUrl url("https://godroll.tv/api/weapons/list");
    QUrlQuery query;
    query.addQueryItem("source", "app");
    query.addQueryItem("schema", "anti-champion-v1");
    const bool forceFresh = m_forceFreshRequest;
    m_forceFreshRequest = false;
    if (forceFresh)
        query.addQueryItem("refresh", QString::number(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::AlwaysNetwork);
    if (forceFresh) {
        request.setRawHeader("Cache-Control", "no-cache, no-store, max-age=0");
        request.setRawHeader("Pragma", "no-cache");
    }
    
    m_currentReply = m_networkManager->get(request);
    connect(m_currentReply, &QNetworkReply::finished, this,
            [this, reply = m_currentReply]() { onNetworkReply(reply); });
    
    // Start timeout timer
    m_timeoutTimer->start(timeoutMs);
}

void WeaponLoader::cleanupCurrentRequest()
{
    // Stop timeout timer
    m_timeoutTimer->stop();
    
    // Abort and cleanup current reply if exists
    if (m_currentReply) {
        auto *reply = m_currentReply;
        m_currentReply = nullptr;
        reply->abort();
        reply->deleteLater();
    }
}

void WeaponLoader::onTimeout()
{
    qWarning() << "Weapon request timed out";
    
    if (m_retryCount < m_maxRetries) {
        m_retryCount++;
        qDebug() << "Retrying... (" << m_retryCount << "/" << m_maxRetries << ")";
        startRequest();
    } else {
        qWarning() << "Max retries reached. Failed to load weapons.";
        failRequest("Could not connect to the weapon service.");
    }
}

void WeaponLoader::onNetworkReply(QNetworkReply *reply)
{
    // An aborted/superseded reply must not stop a newer request's watchdog.
    if (reply != m_currentReply) {
        reply->deleteLater();
        return;
    }
    // Stop timeout timer since we got a response
    m_timeoutTimer->stop();
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj["weapons"].isArray() && !obj["weapons"].toArray().isEmpty()
                    && obj["success"].toBool()) {
                emit loadStatusChanged("Organizing your weapons...");
                QJsonArray weapons = obj["weapons"].toArray();
                const bool hasCurrentSchema = hasCurrentAntiChampionSchema(weapons);

                if (!hasCurrentSchema && !m_schemaRefreshAttempted) {
                    qWarning() << "Weapon data is missing antiChampionType; invalidating caches and refreshing once";
                    m_schemaRefreshAttempted = true;
                    m_forceFreshRequest = true;
                    if (!hasCurrentAntiChampionSchema(m_cachedWeapons))
                        m_cachedWeapons = QJsonArray();
                    reply->deleteLater();
                    m_currentReply = nullptr;
                    startRequest();
                    return;
                }

                if (!hasCurrentSchema) {
                    qWarning() << "Weapon data still uses the old schema after refresh; champion UI will stay hidden";
                    if (m_reloading) {
                        failRequest("The weapon service returned outdated data.");
                        return;
                    }
                }
                
                // Process weapons to add season info from traitIds
                QJsonArray processedWeapons;
                for (const QJsonValue &value : weapons) {
                    QJsonObject weapon = value.toObject();
                    
                    // Extract exotic status from tierType (6 = Exotic) or tierTypeName
                    bool isExotic = (weapon["tierType"].toInt() == 6) || 
                                   (weapon["tierTypeName"].toString().toLower() == "exotic");
                    weapon["isExotic"] = isExotic;
                    
                    // Extract season from traitIds
                    if (weapon.contains("traitIds")) {
                        QJsonArray traitIds = weapon["traitIds"].toArray();
                        int seasonNumber = SeasonMapping::instance().getSeasonNumber(traitIds);
                        QString seasonName = SeasonMapping::instance().getSeasonName(traitIds);
                        QString seasonDisplay = SeasonMapping::instance().getSeasonFromTraitIds(traitIds);
                        weapon["seasonNumber"] = seasonNumber;
                        weapon["seasonName"] = seasonName;
                        // Add a searchable season field that always contains "Season X" format
                        // Even if seasonNumber is 0, we use "Season 0" so "Season" search works
                        weapon["season"] = QString("Season %1").arg(seasonNumber);
                        weapon["seasonDisplay"] = seasonDisplay;
                    } else {
                        // Fallback for weapons without traitIds
                        weapon["seasonNumber"] = 0;
                        weapon["seasonName"] = "";
                        weapon["season"] = "Season";
                        weapon["seasonDisplay"] = "";
                    }
                    
                    processedWeapons.append(weapon);
                }
                
                qDebug() << "Loaded" << processedWeapons.size() << "weapons";
                m_currentReply = nullptr;
                
                // Never retain an old-schema response in the memory cache.
                m_cachedWeapons = hasCurrentSchema ? processedWeapons : QJsonArray();
                const bool wasReloading = m_reloading;
                m_reloading = false;
                auto callback = std::move(m_callback);
                m_callback = {};
                
                // Emit signal for QML connections
                emit weaponsLoaded(processedWeapons);
                
                if (wasReloading)
                    emit reloadFinished(true);
                if (callback)
                    callback(processedWeapons);
                reply->deleteLater();
                return;
            } else {
                qWarning() << "API response does not contain weapons array";
                // Retry on invalid response
                if (m_retryCount < m_maxRetries) {
                    m_retryCount++;
                    qDebug() << "Invalid response, retrying... (" << m_retryCount << "/" << m_maxRetries << ")";
                    reply->deleteLater();
                    m_currentReply = nullptr;
                    startRequest();
                    return;
                } else {
                    failRequest("The weapon service returned an unexpected response.");
                    return;
                }
            }
        } else {
            qWarning() << "API response is not a JSON object";
            // Retry on invalid response
            if (m_retryCount < m_maxRetries) {
                m_retryCount++;
                qDebug() << "Invalid JSON, retrying... (" << m_retryCount << "/" << m_maxRetries << ")";
                reply->deleteLater();
                m_currentReply = nullptr;
                startRequest();
                return;
            } else {
                failRequest("The weapon service returned unreadable data.");
                return;
            }
        }
    } else {
        qWarning() << "Failed to load weapons:" << reply->errorString();
        // Retry on network error
        if (m_retryCount < m_maxRetries) {
            m_retryCount++;
            qDebug() << "Network error, retrying... (" << m_retryCount << "/" << m_maxRetries << ")";
            reply->deleteLater();
            m_currentReply = nullptr;
            startRequest();
            return;
        } else {
            failRequest("Could not connect to the weapon service.");
            return;
        }
    }
    
    m_currentReply = nullptr;
    reply->deleteLater();
}

void WeaponLoader::reload()
{
    if (m_currentReply || m_reloading)
        return;
    // Spam protection: max 3 reloads per 60 seconds
    qint64 now = QDateTime::currentSecsSinceEpoch();
    // Remove timestamps older than the window
    while (!m_reloadTimestamps.isEmpty() && (now - m_reloadTimestamps.first()) > RELOAD_WINDOW_SECS) {
        m_reloadTimestamps.removeFirst();
    }
    if (m_reloadTimestamps.size() >= MAX_RELOADS_PER_WINDOW && !m_cachedWeapons.isEmpty()) {
        qDebug() << "Reload throttled - serving cached data";
        emit reloadStarted();
        emit reloadFinished(false);
        return;
    }
    m_reloadTimestamps.append(now);

    qDebug() << "Reloading weapons...";
    m_reloading = true;
    m_refreshElapsed.start();
    m_callback = {};
    m_retryCount = 0;
    m_maxRetries = 0; // Refresh is bounded; boot owns its separate retry policy.
    m_forceFreshRequest = false;
    m_schemaRefreshAttempted = false;
    emit reloadStarted();
    startRequest();
}

void WeaponLoader::failRequest(const QString &message)
{
    cleanupCurrentRequest();
    const bool wasReloading = m_reloading;
    m_reloading = false;
    auto callback = std::move(m_callback);
    m_callback = {};
    if (wasReloading) {
        // Leave the model, search query and last good cache untouched.
        emit reloadFinished(false);
        return;
    }
    emit loadFailed(message);
    if (callback)
        callback(QJsonArray());
}

bool WeaponLoader::hasCurrentAntiChampionSchema(const QJsonArray &weapons)
{
    for (const QJsonValue &value : weapons) {
        if (!value.toObject().contains("antiChampionType"))
            return false;
    }
    return true;
}
