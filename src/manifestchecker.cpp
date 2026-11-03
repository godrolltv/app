#include "manifestchecker.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QDateTime>
#include <QTimer>
#include <QRandomGenerator>
#include <QDebug>

ManifestChecker::ManifestChecker(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_pendingJitterCheck(false)
    , m_isInitialCheck(false)
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &ManifestChecker::onNetworkReply);

    // Load persisted state
    QSettings settings("Godroll.tv", "GodrollLauncher");
    m_lastKnownVersion = settings.value("manifestLastKnownVersion", "").toString();
    m_lastCheckTime = settings.value("manifestLastCheckTime", 0).toLongLong();
    m_autoRefresh = settings.value("autoRefreshWeapons", true).toBool();

    qDebug() << "ManifestChecker initialized. Last known version:" << m_lastKnownVersion;
}

void ManifestChecker::checkNow()
{
    // Initial check: only stores the current version, never triggers a reload.
    // Weapons were just loaded fresh from API, so reloading would be redundant.
    m_isInitialCheck = true;
    checkManifest();
}

void ManifestChecker::checkIfNeeded()
{
    if (!m_autoRefresh) return;
    if (m_pendingJitterCheck) return;

    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    qint64 timeSinceLastCheck = currentTime - m_lastCheckTime;

    if (timeSinceLastCheck < CHECK_INTERVAL) {
        qDebug() << "Manifest check skipped, only" << (timeSinceLastCheck / 60) << "minutes since last check";
        return;
    }

    // Random jitter to prevent thundering herd
    int jitterMs = QRandomGenerator::global()->bounded(JITTER_MAX_MS);
    m_pendingJitterCheck = true;
    qDebug() << "Manifest check scheduled with" << jitterMs << "ms jitter";
    QTimer::singleShot(jitterMs, this, [this]() {
        m_pendingJitterCheck = false;
        checkManifest();
    });
}

void ManifestChecker::setAutoRefresh(bool enabled)
{
    if (m_autoRefresh == enabled) return;
    m_autoRefresh = enabled;

    QSettings settings("Godroll.tv", "GodrollLauncher");
    settings.setValue("autoRefreshWeapons", m_autoRefresh);

    emit autoRefreshChanged();
    qDebug() << "Auto refresh weapons:" << (m_autoRefresh ? "enabled" : "disabled");
}

void ManifestChecker::checkManifest()
{
    if (m_pendingReply)
        return;
    qDebug() << "Checking manifest...";
    QNetworkRequest request(QUrl("https://godroll.tv/api/manifest"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    m_pendingReply = m_networkManager->get(request);
    auto *reply = m_pendingReply.data();
    QTimer::singleShot(10000, reply, [reply]() {
        if (!reply->isFinished())
            reply->abort();
    });
}

void ManifestChecker::onNetworkReply(QNetworkReply *reply)
{
    if (m_pendingReply == reply)
        m_pendingReply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Manifest check failed:" << reply->errorString();
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(responseData);

    if (!doc.isObject()) {
        qWarning() << "Manifest response is not a JSON object";
        return;
    }

    QJsonObject obj = doc.object();
    QString newVersion = obj["version"].toString();

    if (newVersion.isEmpty()) {
        qWarning() << "Manifest response has no version field";
        return;
    }

    // Update last check time
    m_lastCheckTime = QDateTime::currentSecsSinceEpoch();
    QSettings settings("Godroll.tv", "GodrollLauncher");
    settings.setValue("manifestLastCheckTime", m_lastCheckTime);

    if (m_lastKnownVersion.isEmpty() || m_isInitialCheck) {
        // First time ever or initial app load — just store version, no reload
        qDebug() << "Manifest check (initial), storing version:" << newVersion;
        m_lastKnownVersion = newVersion;
        settings.setValue("manifestLastKnownVersion", m_lastKnownVersion);
        m_isInitialCheck = false;
        return;
    }

    if (newVersion != m_lastKnownVersion) {
        qDebug() << "Manifest changed:" << m_lastKnownVersion << "->" << newVersion;
        m_lastKnownVersion = newVersion;
        settings.setValue("manifestLastKnownVersion", m_lastKnownVersion);
        emit manifestChanged();
    } else {
        qDebug() << "Manifest unchanged:" << newVersion;
    }
}
