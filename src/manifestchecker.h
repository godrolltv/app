#ifndef MANIFESTCHECKER_H
#define MANIFESTCHECKER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QPointer>

class ManifestChecker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool autoRefresh READ autoRefresh WRITE setAutoRefresh NOTIFY autoRefreshChanged)

public:
    explicit ManifestChecker(QObject *parent = nullptr);

    // Check immediately (initial load — stores version, never triggers reload)
    Q_INVOKABLE void checkNow();

    // Check only if 4+ hours since last check (foreground trigger, with jitter)
    Q_INVOKABLE void checkIfNeeded();

    bool autoRefresh() const { return m_autoRefresh; }
    void setAutoRefresh(bool enabled);

signals:
    void manifestChanged();
    void autoRefreshChanged();

private slots:
    void onNetworkReply(QNetworkReply *reply);

private:
    void checkManifest();

    QNetworkAccessManager *m_networkManager;
    QPointer<QNetworkReply> m_pendingReply;
    QString m_lastKnownVersion;
    qint64 m_lastCheckTime;
    bool m_autoRefresh;
    bool m_pendingJitterCheck;
    bool m_isInitialCheck;

    static const qint64 CHECK_INTERVAL = 4 * 60 * 60; // 4 hours in seconds
    static const int JITTER_MAX_MS = 30000; // 0-30 seconds random jitter
};

#endif // MANIFESTCHECKER_H
