#include <QtTest>
#include <QCoreApplication>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
#include <QTemporaryDir>
#include <QUdpSocket>
#include <QUuid>
#include "engine/ConfigurationStore.hpp"
#include <bit>
#include <algorithm>
#include <functional>
#include <memory>

// These tests use the real engine/control processes. The OSC and IPC readers are
// deliberately independent of the production serializers/framing helpers.
namespace {
bool eventually(const std::function<bool()>& condition, int timeoutMs = 5000) {
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        if (condition()) return true;
        QTest::qWait(5);
    }
    return condition();
}

QByteArray frame(const QJsonObject& object) {
    const auto payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    const auto length = static_cast<quint32>(payload.size());
    QByteArray encoded;
    for (int shift : {24, 16, 8, 0}) encoded.append(char((length >> shift) & 0xff));
    encoded.append(payload);
    return encoded;
}

class Client {
public:
    QLocalSocket socket;
    QByteArray buffered;
    QString error;
    QJsonObject lastTelemetry;
    qint64 sequence = 0;

    bool connect(const QString& endpoint) {
        return eventually([&] {
            if (socket.state() == QLocalSocket::ConnectedState) return true;
            if (socket.state() == QLocalSocket::UnconnectedState) socket.connectToServer(endpoint);
            return socket.waitForConnected(20);
        });
    }

    QList<QJsonObject> read() {
        buffered += socket.readAll();
        QList<QJsonObject> result;
        while (buffered.size() >= 4) {
            quint32 size = 0;
            for (int index = 0; index < 4; ++index) size = (size << 8) | quint8(buffered[index]);
            if (size == 0 || size > 65536) {
                error = QStringLiteral("invalid IPC frame size");
                return {};
            }
            if (buffered.size() < 4 + size) break;
            QJsonParseError parse;
            const auto document = QJsonDocument::fromJson(buffered.mid(4, size), &parse);
            buffered.remove(0, 4 + size);
            if (parse.error != QJsonParseError::NoError || !document.isObject()) {
                error = QStringLiteral("invalid IPC JSON");
                return {};
            }
            const auto object = document.object();
            if (object.value("event").toString() == "state") lastTelemetry = object;
            result.push_back(object);
        }
        return result;
    }

    QJsonObject request(const QString& command, QJsonObject arguments = {}) {
        const auto id = ++sequence;
        arguments["version"] = 1;
        arguments["id"] = id;
        arguments["command"] = command;
        socket.write(frame(arguments));
        QJsonObject response;
        if (!eventually([&] {
                for (const auto& object : read()) {
                    if (object.value("id").toInteger(-1) == id) response = object;
                }
                return !response.isEmpty();
            })) error = QStringLiteral("IPC request timed out: ") + command;
        return response;
    }
};

struct Scalar { QString address; float value; };
Scalar decode(const QByteArray& packet) {
    qsizetype offset = 0;
    const auto string = [&]() {
        const auto end = packet.indexOf('\0', offset);
        if (end < 0) return QString{};
        const auto value = QString::fromLatin1(packet.mid(offset, end - offset));
        offset = (end + 4) & ~qsizetype(3);
        return value;
    };
    const auto address = string();
    const auto tags = string();
    if (!address.startsWith("/leap/") || tags != ",f" || offset + 4 != packet.size()) return {};
    quint32 bits = 0;
    for (int index = 0; index < 4; ++index) bits = (bits << 8) | quint8(packet[offset + index]);
    return {address, std::bit_cast<float>(bits)};
}

QList<Scalar> receive(QUdpSocket& receiver, int durationMs) {
    QList<Scalar> result;
    QElapsedTimer elapsed;
    elapsed.start();
    do {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        while (receiver.hasPendingDatagrams()) {
            QByteArray packet;
            packet.resize(receiver.pendingDatagramSize());
            receiver.readDatagram(packet.data(), packet.size());
            result.push_back(decode(packet));
        }
        if (elapsed.elapsed() < durationMs) QTest::qWait(5);
    } while (elapsed.elapsed() < durationMs);
    return result;
}

void set(QJsonObject& config, const QString& group, const QString& key, const QJsonValue& value) {
    auto nested = config.value(group).toObject();
    nested[key] = value;
    config[group] = nested;
}

void palmOnly(QJsonObject& config) {
    auto channels = config.value("channels").toObject();
    for (auto it = channels.begin(); it != channels.end(); ++it) it.value() = false;
    channels["sendPalm"] = true;
    config["channels"] = channels;
}

void device(QJsonObject& config, const QString& serial, const QString& key, const QJsonValue& value) {
    auto devices = config.value("devices").toObject();
    auto selected = devices.value(serial).toObject();
    selected[key] = value;
    devices[serial] = selected;
    config["devices"] = devices;
}
}

class ControlIntegration : public QObject {
    Q_OBJECT
private:
    std::unique_ptr<QTemporaryDir> directory_;
    std::unique_ptr<QUdpSocket> receiver_;
    std::unique_ptr<Client> client_;
    QProcess engine_;
    QString endpoint_;

    bool startEngine(bool explicitPort = true) {
        engine_.setProgram(QString::fromUtf8(TEST_ENGINE_PATH));
        QStringList arguments{"--simulate", "--headless", "--endpoint", endpoint_,
            "--state-dir", directory_->path()};
        if (explicitPort) arguments << "--port" << QString::number(receiver_->localPort());
        engine_.setArguments(arguments);
        engine_.start();
        if (!engine_.waitForStarted(5000)) return false;
        client_ = std::make_unique<Client>();
        if (!client_->connect(endpoint_)) return false;
        return eventually([&] {
            const auto reply = client_->request("get_state");
            return reply.value("ok").toBool() && reply.value("config").toObject().value("devices").toObject().size() == 2;
        });
    }

    void stopEngine() {
        client_.reset();
        if (engine_.state() != QProcess::NotRunning) {
            engine_.terminate();
            if (!engine_.waitForFinished(500)) {
                engine_.kill();
                engine_.waitForFinished(3000);
            }
        }
    }

    QJsonObject state() { return client_->request("get_state"); }
    QJsonObject apply(const QJsonObject& config, qint64 revision) {
        return client_->request("apply_config", {{"expected_revision", revision}, {"config", config}});
    }
    void settle(QUdpSocket& receiver) { receive(receiver, 60); }

private slots:
    void init() {
        directory_ = std::make_unique<QTemporaryDir>();
        QVERIFY(directory_->isValid());
        receiver_ = std::make_unique<QUdpSocket>();
        QVERIFY(receiver_->bind(QHostAddress(QHostAddress::LocalHost), 0));
        endpoint_ = "handbridge-test-" + QUuid::createUuid().toString(QUuid::Id128);
        QVERIFY2(startEngine(), qPrintable(engine_.readAllStandardError()));
    }
    void cleanup() {
        stopEngine();
        receiver_.reset();
        directory_.reset();
    }

    void transactionalRoutingAndProcessingAffectActualUdp() {
        const auto before = state();
        auto config = before.value("config").toObject();
        const auto oldRevision = before.value("revision").toInteger();
        palmOnly(config);
        device(config, "SIM-A", "alias", "dev7");
        device(config, "SIM-B", "enabled", false);
        set(config, "processing", "bypass", false);
        set(config, "processing", "scale", QJsonArray{1., 2., 1.});
        set(config, "processing", "offset_mm", QJsonArray{0., 5., 0.});
        const auto acknowledged = apply(config, oldRevision);
        QVERIFY2(acknowledged.value("ok").toBool(), qPrintable(QJsonDocument(acknowledged).toJson()));
        QCOMPARE(acknowledged.value("revision").toInteger(), oldRevision + 1);
        settle(*receiver_);
        const auto packets = receive(*receiver_, 250);
        QVERIFY(!packets.isEmpty());
        bool sawTransformedY = false;
        for (const auto& packet : packets) {
            QVERIFY2(packet.address.startsWith("/leap/dev7/left/palm/"), qPrintable(packet.address));
            if (packet.address.endsWith("/ty")) {
                QCOMPARE(packet.value, 405.f);
                sawTransformedY = true;
            }
        }
        QVERIFY(sawTransformedY);

        auto stale = apply(before.value("config").toObject(), oldRevision);
        QVERIFY(!stale.value("ok").toBool());
        QVERIFY(!stale.value("error").toString().isEmpty());
        auto invalid = config;
        set(invalid, "osc", "port", 0);
        QVERIFY(!apply(invalid, oldRevision + 1).value("ok").toBool());
        invalid = config;
        invalid["unknown_setting"] = true;
        QVERIFY(!apply(invalid, oldRevision + 1).value("ok").toBool());
        invalid = config;
        device(invalid, "SIM-B", "alias", "dev7");
        QVERIFY(!apply(invalid, oldRevision + 1).value("ok").toBool());
        const auto after = state();
        QCOMPARE(after.value("revision").toInteger(), oldRevision + 1);
        QCOMPARE(after.value("config").toObject(), config);
    }

    void outputRateDestinationAndEnableControlRealSends() {
        auto snapshot = state();
        auto config = snapshot.value("config").toObject();
        palmOnly(config);
        device(config, "SIM-B", "enabled", false);
        set(config, "osc", "rate_hz", 10);
        auto response = apply(config, snapshot.value("revision").toInteger());
        QVERIFY(response.value("ok").toBool());
        settle(*receiver_);
        const auto limited = receive(*receiver_, 350);
        const auto frameCount = std::count_if(limited.begin(), limited.end(), [](const Scalar& scalar) {
            return scalar.address == "/leap/dev1/left/palm/ty";
        });
        QVERIFY2(frameCount >= 2 && frameCount <= 6, qPrintable(QString::number(frameCount)));

        QUdpSocket alternate;
        QVERIFY(alternate.bind(QHostAddress(QHostAddress::LocalHost), 0));
        set(config, "osc", "port", alternate.localPort());
        response = apply(config, response.value("revision").toInteger());
        QVERIFY(response.value("ok").toBool());
        settle(*receiver_);
        QVERIFY(receive(*receiver_, 150).isEmpty());
        QVERIFY(!receive(alternate, 200).isEmpty());

        set(config, "osc", "enabled", false);
        response = apply(config, response.value("revision").toInteger());
        QVERIFY(response.value("ok").toBool());
        settle(alternate);
        QVERIFY(receive(alternate, 250).isEmpty());
        const auto disabledState = state();
        const auto devices = disabledState.value("state").toObject().value("devices").toArray();
        QVERIFY(!devices.isEmpty());
        for (const auto& item : devices) QVERIFY(item.toObject().value("frames_in").toInteger() > 0);
    }

    void profilesPersistAndRestoreAcrossEngineRestart() {
        auto snapshot = state();
        auto config = snapshot.value("config").toObject();
        palmOnly(config);
        device(config, "SIM-A", "alias", "dev7");
        auto applied = apply(config, snapshot.value("revision").toInteger());
        QVERIFY(applied.value("ok").toBool());
        auto saved = client_->request("save_profile", {{"name", "Live Rig"}});
        QVERIFY2(saved.value("ok").toBool(), qPrintable(QJsonDocument(saved).toJson()));
        QCOMPARE(saved.value("saved_revision").toInteger(), saved.value("revision").toInteger());
        QVERIFY(client_->request("save_profile", {{"name", "Live Rig"}}).value("ok").toBool());
        const auto listed = client_->request("list_profiles").value("profiles").toArray();
        QVERIFY(listed.contains("Live Rig"));
        for (const auto& invalid : {QString("../escape"), QString("/absolute"), QString("x.json")}) {
            QVERIFY(!client_->request("save_profile", {{"name", invalid}}).value("ok").toBool());
        }

        set(config, "osc", "enabled", false);
        applied = apply(config, applied.value("revision").toInteger());
        QVERIFY(applied.value("ok").toBool());
        const auto loaded = client_->request("load_profile", {{"name", "Live Rig"},
            {"expected_revision", applied.value("revision")}});
        QVERIFY2(loaded.value("ok").toBool(), qPrintable(QJsonDocument(loaded).toJson()));
        QVERIFY(loaded.value("config").toObject().value("osc").toObject().value("enabled").toBool());
        stopEngine();
        QVERIFY2(startEngine(), qPrintable(engine_.readAllStandardError()));
        const auto restored = state().value("config").toObject();
        QCOMPARE(restored.value("devices").toObject().value("SIM-A").toObject().value("alias").toString(), "dev7");
        QCOMPARE(restored.value("osc").toObject().value("port").toInt(), receiver_->localPort());
        settle(*receiver_);
        const auto packets = receive(*receiver_, 150);
        QVERIFY(std::any_of(packets.begin(), packets.end(), [](const Scalar& item) {
            return item.address.startsWith("/leap/dev7/left/palm/");
        }));
        QDirIterator backups(directory_->path(), {"*.bak"}, QDir::Files, QDirIterator::Subdirectories);
        QVERIFY2(backups.hasNext(), "Saving an existing profile/current snapshot must preserve a backup");
    }

    void persistenceRejectsLateAcknowledgementsWithoutTouchingCurrentOrBackup() {
        QTemporaryDir isolated;
        QVERIFY(isolated.isValid());
        handbridge::engine::ConfigurationStore store(isolated.path());
        const auto read = [](const QString& path) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) return QByteArray{};
            return file.readAll();
        };
        const auto path = isolated.filePath("current.json");
        const auto baseline = state().value("config").toObject();
        auto newer = baseline;
        set(newer, "processing", "offset_mm", QJsonArray{0., 3., 0.});
        auto stale = baseline;
        set(stale, "processing", "offset_mm", QJsonArray{0., 2., 0.});
        QVERIFY(store.saveCurrent(baseline, 1));
        QVERIFY(store.saveCurrent(newer, 3));
        const auto currentBytes = read(path);
        const auto backupBytes = read(path + ".bak");
        QVERIFY(!currentBytes.isEmpty());
        QVERIFY(!backupBytes.isEmpty());
        QCOMPARE(QJsonDocument::fromJson(currentBytes).object(), newer);

        // Deliver the older load acknowledgement after the newer save. This is
        // the exact production persistence helper, with no timing/race hooks.
        QVERIFY(!store.saveCurrent(stale, 2));
        QCOMPARE(store.savedRevision(), quint64(3));
        QCOMPARE(read(path), currentBytes);
        QCOMPARE(read(path + ".bak"), backupBytes);
        QVERIFY(store.saveCurrent(newer, 3)); // Saving the current revision remains valid.
        auto latest = newer;
        set(latest, "processing", "offset_mm", QJsonArray{0., 4., 0.});
        QVERIFY(store.saveCurrent(latest, 4));
        QCOMPARE(store.savedRevision(), quint64(4));
        QCOMPARE(QJsonDocument::fromJson(read(path)).object(), latest);
        QCOMPARE(QJsonDocument::fromJson(read(path + ".bak")).object(), newer);
    }

    void restoredConfigurationDiscoveryIsAppliedButNotFalselySaved() {
        auto startup = state().value("config").toObject();
        startup["devices"] = QJsonObject{};
        set(startup, "osc", "port", receiver_->localPort());
        stopEngine();
        handbridge::engine::ConfigurationStore::write(directory_->filePath("current.json"), startup);
        // No CLI overrides: a restored schema2 snapshot begins saved at revision1.
        QVERIFY2(startEngine(false), qPrintable(engine_.readAllStandardError()));
        const auto discovered = state();
        QCOMPARE(discovered.value("revision").toInteger(), qint64(2));
        QCOMPARE(discovered.value("saved_revision").toInteger(), qint64(1));
        QCOMPARE(discovered.value("config").toObject().value("devices").toObject().size(), 2);
        QFile persisted(directory_->filePath("current.json"));
        QVERIFY(persisted.open(QIODevice::ReadOnly));
        QVERIFY(QJsonDocument::fromJson(persisted.readAll()).object().value("devices").toObject().isEmpty());
    }

    void malformedClientsAndDuplicateEngineDoNotStopTracking() {
        QLocalSocket malformed;
        malformed.connectToServer(endpoint_);
        QVERIFY(malformed.waitForConnected(1000));
        malformed.write(QByteArray::fromHex("00010001")); // 65537 exceeds the documented maximum.
        QVERIFY(eventually([&] { return malformed.state() == QLocalSocket::UnconnectedState; }));
        QLocalSocket invalidJson;
        invalidJson.connectToServer(endpoint_);
        QVERIFY(invalidJson.waitForConnected(1000));
        invalidJson.write(QByteArray::fromHex("00000001") + '{');
        QVERIFY(eventually([&] { return invalidJson.state() == QLocalSocket::UnconnectedState; }));
        Client future;
        QVERIFY(future.connect(endpoint_));
        future.socket.write(frame({{"version", 2}, {"id", 90000}, {"command", "get_state"}}));
        QJsonObject rejectedVersion;
        QVERIFY(eventually([&] {
            for (const auto& response : future.read())
                if (response.value("id").toInteger(-1) == 90000) rejectedVersion = response;
            return !rejectedVersion.isEmpty();
        }));
        QVERIFY(!rejectedVersion.value("ok").toBool());
        QVERIFY(!client_->request("unknown_command").value("ok").toBool());
        QVERIFY(state().value("ok").toBool());

        QProcess duplicate;
        QTemporaryDir otherStateDirectory;
        QVERIFY(otherStateDirectory.isValid());
        duplicate.start(QString::fromUtf8(TEST_ENGINE_PATH), {"--simulate", "--endpoint", endpoint_,
            "--state-dir", otherStateDirectory.path()});
        QVERIFY(duplicate.waitForStarted(3000));
        QVERIFY2(duplicate.waitForFinished(5000), "Second engine must fail promptly instead of stealing a live endpoint");
        QVERIFY(duplicate.exitCode() != 0);
        QCOMPARE(engine_.state(), QProcess::Running);
        QVERIFY(state().value("ok").toBool());
        settle(*receiver_);
        QVERIFY(!receive(*receiver_, 150).isEmpty());
    }

    void batchedRequestsBeyondOneDrainAndUnreadClientAreIsolated() {
        QByteArray batched;
        QSet<qint64> expected;
        for (qint64 id = 50000; id < 50020; ++id) {
            expected.insert(id);
            batched += frame({{"version", 1}, {"id", id}, {"command", "get_state"}});
        }
        // One write with >16 frames catches parsers that stop their drain after
        // a fairness budget but never schedule the bytes already in memory.
        client_->socket.write(batched);
        QVERIFY2(eventually([&] {
            for (const auto& reply : client_->read()) expected.remove(reply.value("id").toInteger(-1));
            return expected.isEmpty();
        }), qPrintable(QString("Unanswered batched requests: %1").arg(expected.size())));

        QLocalSocket unread;
        unread.setReadBufferSize(1);
        unread.connectToServer(endpoint_);
        QVERIFY(unread.waitForConnected(1000));
        QByteArray flood;
        for (qint64 id = 60000; id < 61000; ++id)
            flood += frame({{"version", 1}, {"id", id}, {"command", "get_state"}});
        unread.write(flood);
        QVERIFY2(state().value("ok").toBool(), qPrintable(client_->error));
        settle(*receiver_);
        QVERIFY(!receive(*receiver_, 200).isEmpty());
        QCOMPARE(engine_.state(), QProcess::Running);
        unread.abort();
    }

    void actualGuiCloseKillAndReopenLeaveEngineAndUdpAlive() {
        auto initial = state();
        auto config = initial.value("config").toObject();
        palmOnly(config);
        set(config, "processing", "bypass", false);
        set(config, "processing", "offset_mm", QJsonArray{0., 17., 0.});
        QVERIFY(apply(config, initial.value("revision").toInteger()).value("ok").toBool());
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("QT_QPA_PLATFORM", "offscreen");
        environment.insert("QT_QUICK_BACKEND", "software");
        for (int cycle = 0; cycle < 3; ++cycle) {
            QProcess gui;
            gui.setProcessEnvironment(environment);
            const auto screenshot = directory_->filePath(QString("controller-%1.png").arg(cycle));
            QStringList arguments{"--endpoint", endpoint_, "--screenshot", screenshot};
            if (cycle != 1) arguments << "--exit-after-ms" << "3000";
            gui.start(QString::fromUtf8(TEST_CONTROL_PATH), arguments);
            QVERIFY2(gui.waitForStarted(3000), qPrintable(gui.errorString()));
            // A different process creates the file before PNG compression/write
            // finishes; existence alone is not a completed native capture.
            QVERIFY2(eventually([&] { return QFileInfo(screenshot).size() > 1000 || gui.state() == QProcess::NotRunning; }, 10000),
                     qPrintable(gui.readAllStandardError()));
            QVERIFY2(QFile::exists(screenshot), qPrintable(gui.readAllStandardError()));
            QFile capture(screenshot);
            QVERIFY(capture.open(QIODevice::ReadOnly));
            const auto header = capture.read(24);
            QCOMPARE(header.size(), 24);
            QCOMPARE(header.left(8), QByteArray::fromHex("89504e470d0a1a0a"));
            const auto dimension = [&](int offset) {
                quint32 value = 0;
                for (int byte = 0; byte < 4; ++byte) value = (value << 8) | quint8(header[offset + byte]);
                return value;
            };
            QVERIFY2(dimension(16) >= 1100 && dimension(20) >= 700,
                     qPrintable(QString("Native capture is %1x%2").arg(dimension(16)).arg(dimension(20))));
            if (cycle == 1) gui.kill();
            QVERIFY2(gui.waitForFinished(5000), qPrintable(gui.readAllStandardError()));
            if (cycle != 1) QCOMPARE(gui.exitCode(), 0);
            QCOMPARE(engine_.state(), QProcess::Running);
            QVERIFY(state().value("ok").toBool());
            settle(*receiver_);
            const auto packets = receive(*receiver_, 200);
            QVERIFY(!packets.isEmpty());
            QVERIFY(std::any_of(packets.begin(), packets.end(), [](const Scalar& item) {
                return item.address.endsWith("/palm/ty") && item.value == 217.f;
            }));
        }
    }

    void exportedDiagnosticsAreValidJsonAndRedactSerials() {
        const auto reply = client_->request("export_diagnostics");
        QVERIFY2(reply.value("ok").toBool(), qPrintable(QJsonDocument(reply).toJson()));
        QFile exported(reply.value("path").toString());
        QVERIFY(exported.open(QIODevice::ReadOnly));
        const auto bytes = exported.readAll();
        QJsonParseError error;
        QVERIFY(QJsonDocument::fromJson(bytes, &error).isObject());
        QCOMPARE(error.error, QJsonParseError::NoError);
        QVERIFY(!bytes.contains("SIM-A"));
        QVERIFY(!bytes.contains("SIM-B"));
    }
};

QTEST_GUILESS_MAIN(ControlIntegration)
#include "test_ControlIntegration.moc"
