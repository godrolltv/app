#include <QtTest>
#include <QQueue>
#include <QTemporaryDir>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstring>
#include "weaponloader.h"
#include "weaponsearchmodel.h"
#include "updatechecker.h"

struct Response {
    QByteArray body;
    int status = 200;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    int delay = 0; // -1 simulates a connection that never finishes.
    QList<QPair<QByteArray, QByteArray>> headers;
};

class FakeReply : public QNetworkReply {
public:
    FakeReply(const QNetworkRequest &request, const Response &response, QObject *parent)
        : QNetworkReply(parent), m_response(response) {
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, response.status);
        for (const auto &header : response.headers)
            setRawHeader(header.first, header.second);
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        if (response.delay >= 0)
            QTimer::singleShot(response.delay, this, [this]() { finish(); });
    }
    void abort() override {
        if (isFinished()) return;
        setError(OperationCanceledError, "raw internal cancellation");
        setFinished(true);
        emit finished();
    }
    qint64 bytesAvailable() const override {
        return m_response.body.size() - m_offset + QNetworkReply::bytesAvailable();
    }
protected:
    qint64 readData(char *data, qint64 maxSize) override {
        const qint64 count = qMin(maxSize, qint64(m_response.body.size()) - m_offset);
        if (count <= 0) return -1;
        std::memcpy(data, m_response.body.constData() + m_offset, size_t(count));
        m_offset += count;
        return count;
    }
private:
    void finish() {
        if (isFinished()) return;
        if (m_response.error != NoError)
            setError(m_response.error, "raw internal network error");
        setFinished(true);
        emit readyRead();
        emit finished();
    }
    Response m_response;
    qint64 m_offset = 0;
};

class FakeNetwork : public QNetworkAccessManager {
public:
    QQueue<Response> responses;
    QList<QNetworkRequest> requests;
protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override {
        if (responses.isEmpty()) qFatal("Unexpected network request");
        requests.append(request);
        return new FakeReply(request, responses.dequeue(), this);
    }
};

static QByteArray weapons(const char *name = "Red Tape", bool schema = true) {
    QJsonObject weapon{{"name", name}, {"hash", "123"}, {"weaponType", "Scout Rifle"}};
    if (schema) weapon.insert("antiChampionType", QJsonValue::Null);
    return QJsonDocument(QJsonObject{{"success", true}, {"weapons", QJsonArray{weapon}}}).toJson();
}
static QByteArray release() {
    return R"({"tag_name":"v99.0.0","html_url":"https://github.com/godrolltv/app/releases/tag/v99.0.0","assets":[]})";
}

class NetworkFailureTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QVERIFY(m_settings.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
    }
    void init() { QSettings("Godroll.tv", "GodrollLauncher").clear(); }

    void updateErrors_data() {
        QTest::addColumn<int>("status");
        QTest::addColumn<int>("error");
        QTest::addColumn<QByteArray>("remaining");
        QTest::addColumn<QString>("expected");
        QTest::newRow("offline") << 0 << int(QNetworkReply::HostNotFoundError) << QByteArray() << "internet connection";
        QTest::newRow("tls") << 0 << int(QNetworkReply::SslHandshakeFailedError) << QByteArray() << "secure connection";
        QTest::newRow("missing-release-or-private") << 404 << int(QNetworkReply::ContentNotFoundError) << QByteArray() << "isn't available";
        QTest::newRow("unauthorized") << 401 << int(QNetworkReply::AuthenticationRequiredError) << QByteArray() << "allowing access";
        QTest::newRow("forbidden") << 403 << int(QNetworkReply::ContentAccessDenied) << QByteArray() << "allowing access";
        QTest::newRow("github-rate-limit") << 403 << int(QNetworkReply::ContentAccessDenied) << QByteArray("0") << "Too many";
        QTest::newRow("rate-limit") << 429 << int(QNetworkReply::UnknownContentError) << QByteArray() << "Too many";
        QTest::newRow("server") << 503 << int(QNetworkReply::ServiceUnavailableError) << QByteArray() << "temporarily unavailable";
    }
    void updateErrors() {
        QFETCH(int, status); QFETCH(int, error); QFETCH(QByteArray, remaining); QFETCH(QString, expected);
        FakeNetwork network;
        Response response{ "{}", status, QNetworkReply::NetworkError(error) };
        if (!remaining.isEmpty()) response.headers.append({"X-RateLimit-Remaining", remaining});
        network.responses.enqueue(response);
        UpdateChecker checker(nullptr, &network, 200);
        QSignalSpy failed(&checker, &UpdateChecker::updateCheckFailed);
        QSignalSpy succeeded(&checker, &UpdateChecker::updateCheckComplete);
        checker.checkForUpdates();
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(checker.checkError().contains(expected));
        QVERIFY(!checker.checkError().contains("raw internal"));
        QVERIFY(!checker.checking());
        QCOMPARE(succeeded.count(), 0);
    }
    void updateTimeoutAndRecovery() {
        FakeNetwork network;
        network.responses.enqueue({{}, 0, QNetworkReply::NoError, -1});
        network.responses.enqueue({release()});
        UpdateChecker checker(nullptr, &network, 40);
        QSignalSpy failed(&checker, &UpdateChecker::updateCheckFailed);
        QSignalSpy completed(&checker, &UpdateChecker::updateCheckComplete);
        checker.checkForUpdates();
        checker.checkForUpdates(); // Do not create competing checks.
        QCOMPARE(network.requests.size(), 1);
        QCOMPARE(network.requests.first().url(), QUrl("https://godroll.tv/app/update"));
        QCOMPARE(network.requests.first().rawHeader("Accept"), QByteArray("application/json"));
        QCOMPARE(network.requests.first().rawHeader("Cache-Control"), QByteArray("no-cache, no-store"));
        QCOMPARE(network.requests.first().attribute(QNetworkRequest::CacheLoadControlAttribute).toInt(),
                 int(QNetworkRequest::AlwaysNetwork));
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(checker.checkError().contains("too long"));
        checker.checkForUpdates();
        QVERIFY(checker.checkError().isEmpty());
        QTRY_COMPARE(completed.count(), 1);
        QVERIFY(!checker.checking());
        QVERIFY(checker.updateAvailable());
        QTest::qWait(60);
        QCOMPARE(failed.count(), 1);
    }
    void malformedUpdate_data() {
        QTest::addColumn<QByteArray>("body");
        QTest::newRow("html") << QByteArray("<html>Unavailable</html>");
        QTest::newRow("empty-object") << QByteArray("{}");
        QTest::newRow("bad-version") << QByteArray(R"({"tag_name":"oops","assets":[]})");
    }
    void malformedUpdate() {
        QFETCH(QByteArray, body);
        FakeNetwork network;
        network.responses.enqueue({body});
        UpdateChecker checker(nullptr, &network);
        QSignalSpy failed(&checker, &UpdateChecker::updateCheckFailed);
        QSignalSpy completed(&checker, &UpdateChecker::updateCheckComplete);
        checker.checkForUpdates();
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(!checker.checking());
        QVERIFY(!checker.checkError().isEmpty());
        QCOMPARE(completed.count(), 0);
    }
    void refreshKeepsLastData_data() {
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<int>("error");
        QTest::addColumn<int>("delay");
        QTest::newRow("timeout") << QByteArray() << int(QNetworkReply::NoError) << -1;
        QTest::newRow("offline") << QByteArray() << int(QNetworkReply::HostNotFoundError) << 0;
        QTest::newRow("bad-json") << QByteArray("bad json") << int(QNetworkReply::NoError) << 0;
        QTest::newRow("wrong-array-type") << QByteArray(R"({"success":true,"weapons":{}})") << int(QNetworkReply::NoError) << 0;
        QTest::newRow("empty-array") << QByteArray(R"({"success":true,"weapons":[]})") << int(QNetworkReply::NoError) << 0;
    }
    void refreshKeepsLastData() {
        QFETCH(QByteArray, body); QFETCH(int, error); QFETCH(int, delay);
        FakeNetwork network;
        network.responses.enqueue({weapons()});
        network.responses.enqueue({body, 200, QNetworkReply::NetworkError(error), delay});
        network.responses.enqueue({weapons("Red Tape New")});
        WeaponLoader loader(nullptr, &network, 200, 40);
        WeaponSearchModel model;
        connect(&loader, &WeaponLoader::weaponsLoaded, &model, &WeaponSearchModel::setWeapons);
        QSignalSpy loaded(&loader, &WeaponLoader::weaponsLoaded);
        QSignalSpy failed(&loader, &WeaponLoader::loadFailed);
        QSignalSpy refreshed(&loader, &WeaponLoader::reloadFinished);
        int callbacks = 0;
        loader.loadWeapons([&](const QJsonArray &) { callbacks++; }, 0);
        QTRY_COMPARE(loaded.count(), 1);
        model.setSearchQuery("red tape");
        QCOMPARE(model.rowCount(), 1);
        loader.reload();
        loader.reload();
        QTRY_COMPARE(refreshed.count(), 1);
        QCOMPARE(refreshed[0][0].toBool(), false);
        QCOMPARE(loaded.count(), 1);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(callbacks, 1);
        QCOMPARE(model.searchQuery(), QString("red tape"));
        QCOMPARE(model.data(model.index(0), WeaponSearchModel::NameRole).toString(), QString("Red Tape"));
        loader.reload();
        QTRY_COMPARE(refreshed.count(), 2);
        QCOMPARE(refreshed[1][0].toBool(), true);
        QCOMPARE(model.data(model.index(0), WeaponSearchModel::NameRole).toString(), QString("Red Tape New"));
    }
    void oldSchemaRefreshIsBounded() {
        FakeNetwork network;
        network.responses.enqueue({weapons()});
        network.responses.enqueue({weapons("Old", false)});
        network.responses.enqueue({weapons("Old", false)});
        WeaponLoader loader(nullptr, &network, 200, 100);
        QSignalSpy loaded(&loader, &WeaponLoader::weaponsLoaded);
        QSignalSpy refreshed(&loader, &WeaponLoader::reloadFinished);
        loader.loadWeapons({}, 0);
        QTRY_COMPARE(loaded.count(), 1);
        loader.reload();
        QTRY_COMPARE(refreshed.count(), 1);
        QCOMPARE(refreshed[0][0].toBool(), false);
        QCOMPARE(loaded.count(), 1);
        QCOMPARE(network.requests.size(), 3);
        QVERIFY(network.requests.last().url().query().contains("refresh="));
    }
    void bootFailureStillReportsAndCompletesCallback() {
        FakeNetwork network;
        network.responses.enqueue({"bad json"});
        WeaponLoader loader(nullptr, &network);
        QSignalSpy failed(&loader, &WeaponLoader::loadFailed);
        int callbacks = 0;
        loader.loadWeapons([&](const QJsonArray &data) { QVERIFY(data.isEmpty()); callbacks++; }, 0);
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(callbacks, 1);
    }
    void supersededReplyDoesNotDisableWatchdog() {
        FakeNetwork network;
        network.responses.enqueue({{}, 0, QNetworkReply::NoError, -1});
        network.responses.enqueue({{}, 0, QNetworkReply::NoError, -1});
        WeaponLoader loader(nullptr, &network, 40, 40);
        QSignalSpy failed(&loader, &WeaponLoader::loadFailed);
        int oldCallbacks = 0;
        int currentCallbacks = 0;
        loader.loadWeapons([&](const QJsonArray &) { oldCallbacks++; }, 0);
        loader.loadWeapons([&](const QJsonArray &) { currentCallbacks++; }, 0);
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(oldCallbacks, 0);
        QCOMPARE(currentCallbacks, 1);
    }
private:
    QTemporaryDir m_settings;
};

QTEST_GUILESS_MAIN(NetworkFailureTests)
#include "network_failure_tests.moc"
