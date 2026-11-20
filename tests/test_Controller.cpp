#include "ipc/Endpoint.hpp"
#include "control/Controller.hpp"
#include "ipc/Framing.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QLocalServer>
#include <QProcess>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>
#include <memory>

// Test-only peer verifies the presentation contract. Actual engine/process/UDP
// behavior is covered independently by test_ControlIntegration.
class ControllerTest final : public QObject {
    Q_OBJECT
private:
    QLocalServer server_;
    QLocalSocket* peer_ = nullptr;
    QByteArray bytes_;
    QList<QJsonObject> requests_;
    std::unique_ptr<Controller> controller_;
    QString endpoint_;

    static QJsonObject config(int port = 9000) {
        return {{"schema_version", 2}, {"osc", QJsonObject{{"host", "127.0.0.1"}, {"port", port}, {"enabled", true}, {"rate_hz", 0}}},
                {"channels", QJsonObject{{"sendPalm", true}}},
                {"processing", QJsonObject{{"scale", QJsonArray{1, 1, 1}}}},
                {"devices", QJsonObject{{"SIM-A", QJsonObject{{"alias", "dev1"}, {"enabled", true}, {"hand_filter", "both"}}}}}};
    }
    QJsonObject state(const QJsonObject& settings, qint64 revision, qint64 saved = 0) const {
        return {{"version", 1}, {"config", settings}, {"revision", revision}, {"saved_revision", saved},
                {"state", QJsonObject{{"simulated", true}, {"devices", QJsonArray{QJsonObject{{"serial", "SIM-A"}, {"alias", "dev1"}}}}}},
                {"profiles", QJsonArray{"Default"}}, {"logs", QJsonArray{"engine connected"}}};
    }
    void send(QJsonObject response) { peer_->write(handbridge::ipc::encode(response)); }
    void reply(const QJsonObject& request, QJsonObject response) {
        response["version"] = 1; response["id"] = request.value("id");
        if (!response.contains("ok")) response["ok"] = true;
        send(response);
    }
    void telemetry(QJsonObject response) { response["event"] = "state"; send(response); }
    void receive() {
        bytes_ += peer_->readAll();
        QList<QJsonObject> result;
        QString error;
        QVERIFY2(handbridge::ipc::takeFrames(bytes_, result, error), qPrintable(error));
        requests_ += result;
    }
    int count(const QString& command) const {
        int result = 0;
        for (const auto& item : requests_) if (item.value("command").toString() == command) ++result;
        return result;
    }
    QJsonObject last(const QString& command) const {
        for (auto it = requests_.crbegin(); it != requests_.crend(); ++it)
            if (it->value("command").toString() == command) return *it;
        return {};
    }
    int draftPort() const { return controller_->draft().value("osc").toMap().value("port").toInt(); }
    int appliedPort() const { return controller_->config().value("osc").toMap().value("port").toInt(); }

private slots:
    void init() {
        endpoint_ = "handbridge-controller-test-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        QVERIFY(server_.listen(handbridge::ipc::socketName(endpoint_)));
        connect(&server_, &QLocalServer::newConnection, this, [this] {
            peer_ = server_.nextPendingConnection();
            connect(peer_, &QLocalSocket::readyRead, this, &ControllerTest::receive);
        });
        controller_ = std::make_unique<Controller>(endpoint_);
        QTRY_VERIFY(peer_ != nullptr);
        QTRY_COMPARE(count("get_state"), 1);
        reply(last("get_state"), state(config(), 1));
        QTRY_VERIFY(controller_->connected());
        QTRY_COMPARE(count("list_profiles"), 1);
        reply(last("list_profiles"), {{"profiles", QJsonArray{"Default"}}});
        QTRY_VERIFY(!controller_->busy());
        requests_.clear();
    }
    void cleanup() {
        controller_.reset();
        if (peer_) { peer_->abort(); delete peer_; peer_ = nullptr; }
        server_.close();
        server_.disconnect(this);
        QLocalServer::removeServer(handbridge::ipc::socketName(endpoint_));
        bytes_.clear(); requests_.clear();
    }
    void editorIsNotAppliedUntilAcknowledged() {
        controller_->setSetting("osc.port", 9010);
        QVERIFY(controller_->dirty());
        QCOMPARE(appliedPort(), 9000);
        controller_->apply();
        QTRY_COMPARE(count("apply_config"), 1);
        const auto request = last("apply_config");
        QCOMPARE(request.value("expected_revision").toInteger(), 1);
        QVERIFY(controller_->busy());
        QCOMPARE(controller_->statusText(), "Applying");
        telemetry(state(config(), 1));
        QTRY_COMPARE(controller_->revision(), 1);
        QCOMPARE(draftPort(), 9010);
        QCOMPARE(appliedPort(), 9000);
        reply(request, state(config(9010), 2));
        QTRY_VERIFY(!controller_->busy());
        QCOMPARE(appliedPort(), 9010);
        QVERIFY(!controller_->dirty());
        QCOMPARE(controller_->statusText(), "Applied · unsaved");
    }
    void rejectedRevisionRetainsEditsAndRefreshesEngineTruth() {
        controller_->setSetting("osc.port", 9010);
        telemetry(state(config(9020), 2));
        QTRY_COMPARE(controller_->revision(), 2);
        QCOMPARE(draftPort(), 9010);
        controller_->apply();
        QTRY_COMPARE(count("apply_config"), 1);
        QCOMPARE(last("apply_config").value("expected_revision").toInteger(), 1);
        auto rejection = state(config(9020), 2);
        rejection["ok"] = false; rejection["error"] = "Revision conflict; refresh before applying.";
        reply(last("apply_config"), rejection);
        QTRY_VERIFY(!controller_->busy());
        QVERIFY(controller_->dirty());
        QCOMPARE(draftPort(), 9010);
        QCOMPARE(appliedPort(), 9020);
        QCOMPARE(controller_->statusText(), "Rejected");
        QVERIFY(controller_->errorMessage().contains("Revision conflict"));
        controller_->revert();
        QCOMPARE(draftPort(), 9020);
        QVERIFY(!controller_->dirty());
    }
    void inFlightEditsRemainAndUseAcknowledgedRevisionForNextApply() {
        controller_->setSetting("osc.port", 9010);
        controller_->apply();
        QTRY_COMPARE(count("apply_config"), 1);
        const auto first = last("apply_config");
        controller_->setSetting("osc.port", 9011);
        reply(first, state(config(9010), 2));
        QTRY_VERIFY(!controller_->busy());
        QCOMPARE(appliedPort(), 9010);
        QCOMPARE(draftPort(), 9011);
        QVERIFY(controller_->dirty());
        controller_->apply();
        QTRY_COMPARE(count("apply_config"), 2);
        QCOMPARE(last("apply_config").value("expected_revision").toInteger(), 2);
    }
    void externalStateMatchingDraftNotifiesDirtyChange() {
        QSignalSpy dirty(controller_.get(), &Controller::dirtyChanged);
        controller_->setSetting("osc.port", 9010);
        QCOMPARE(dirty.count(), 1);
        telemetry(state(config(9010), 2));
        QTRY_VERIFY(!controller_->dirty());
        QCOMPARE(dirty.count(), 2);
    }
    void delayedAcknowledgementCannotRegressNewerTelemetry() {
        controller_->setSetting("osc.port", 9010);
        controller_->apply();
        QTRY_COMPARE(count("apply_config"), 1);
        const auto pending = last("apply_config");
        telemetry(state(config(9030), 3));
        QTRY_COMPARE(controller_->revision(), 3);
        reply(pending, state(config(9010), 2));
        QTRY_VERIFY(!controller_->busy());
        QCOMPARE(controller_->revision(), 3);
        QCOMPARE(appliedPort(), 9030);
        QCOMPARE(draftPort(), 9010);
        QVERIFY(controller_->dirty());
        controller_->apply();
        QTRY_COMPARE(count("apply_config"), 2);
        QCOMPARE(last("apply_config").value("expected_revision").toInteger(), 2);
    }
    void unchangedTelemetryDoesNotResetEditorBindings() {
        QSignalSpy editor(controller_.get(), &Controller::draftChanged);
        for (int index = 0; index < 20; ++index) {
            auto update = state(config(), 1);
            update["logs"] = QJsonArray{QString::number(index)};
            telemetry(update);
        }
        QTRY_COMPARE(controller_->logEntries(), QStringList{QStringLiteral("19")});
        QCOMPARE(editor.count(), 0);
        QVERIFY(controller_->connected());
    }
    void savesOnlyAfterEngineAcknowledgementAndRejectsDirtyProfileLoad() {
        controller_->saveProfile("Default");
        QTRY_COMPARE(count("save_profile"), 1);
        QCOMPARE(controller_->savedRevision(), 0);
        reply(last("save_profile"), state(config(), 1, 1));
        QTRY_VERIFY(!controller_->busy());
        QCOMPARE(controller_->statusText(), "Applied · saved");
        controller_->setSetting("osc.port", 9010);
        controller_->loadProfile("Default");
        QCOMPARE(count("load_profile"), 0);
        QVERIFY(controller_->dirty());
        QVERIFY(controller_->errorMessage().contains("Apply or revert"));
        controller_->revert();
        controller_->loadProfile("Default");
        QTRY_COMPARE(count("load_profile"), 1);
        controller_->setSetting("osc.port", 9011);
        QCOMPARE(draftPort(), 9000);
        QVERIFY(!controller_->dirty());
        QVERIFY(controller_->errorMessage().contains("finish loading"));
        reply(last("load_profile"), state(config(9020), 2, 2));
        QTRY_VERIFY(!controller_->busy());
        QCOMPARE(draftPort(), 9020);
        QCOMPARE(appliedPort(), 9020);
        QVERIFY(!controller_->dirty());
        QVERIFY(controller_->errorMessage().isEmpty());
    }
    void nestedArrayAndDeviceEditsAreSentWithoutMutatingAppliedConfig() {
        auto withDottedSerial = config();
        auto deviceMap = withDottedSerial.value("devices").toObject();
        deviceMap["SERIAL.WITH.DOT"] = QJsonObject{{"alias", "dev2"}, {"enabled", true}, {"hand_filter", "both"}};
        withDottedSerial["devices"] = deviceMap;
        telemetry(state(withDottedSerial, 2));
        QTRY_COMPARE(controller_->revision(), 2);
        controller_->setSetting("processing.scale.1", 2.5);
        controller_->setSetting("devices.SIM-A.alias", "dev7");
        controller_->setSetting("devices.SERIAL.WITH.DOT.alias", "dev8");
        controller_->setSetting("devices.SERIAL.WITH.DOT.enabled", false);
        controller_->setSetting("devices.SERIAL.WITH.DOT.hand_filter", "right");
        QCOMPARE(controller_->draft().value("processing").toMap().value("scale").toList().at(1).toDouble(), 2.5);
        QCOMPARE(controller_->config().value("devices").toMap().value("SIM-A").toMap().value("alias").toString(), "dev1");
        controller_->setSetting("processing.unknown", 8);
        QVERIFY(controller_->errorMessage().contains("Unknown setting"));
        controller_->apply();
        QTRY_COMPARE(count("apply_config"), 1);
        QCOMPARE(last("apply_config").value("config").toObject().value("devices").toObject().value("SIM-A").toObject().value("alias").toString(), "dev7");
        const auto dotted = last("apply_config").value("config").toObject().value("devices").toObject().value("SERIAL.WITH.DOT").toObject();
        QCOMPARE(dotted.value("alias").toString(), "dev8");
        QCOMPARE(dotted.value("enabled").toBool(), false);
        QCOMPARE(dotted.value("hand_filter").toString(), "right");
    }
    void disconnectPreservesUnappliedDraftAndCannotSaveOffline() {
        controller_->setSetting("osc.port", 9010);
        QVERIFY(!controller_->devices().isEmpty());
        QSignalSpy updates(controller_.get(), &Controller::statusChanged);
        peer_->abort();
        QTRY_VERIFY(!controller_->connected());
        QVERIFY(!updates.isEmpty());
        QVERIFY(controller_->devices().isEmpty());
        QVERIFY(controller_->status().isEmpty());
        QCOMPARE(draftPort(), 9010);
        QVERIFY(controller_->dirty());
        QVERIFY(!controller_->busy());
        controller_->saveProfile("Default");
        QCOMPARE(count("save_profile"), 0);
    }
    void previewRequestsStayBoundedAndSeparateFromDiagnostics() {
        controller_->refreshPreview();
        QCOMPARE(count("get_preview"), 0);
        auto modern = state(config(), 1);
        modern["preview_available"] = true;
        telemetry(modern);
        QTRY_VERIFY(controller_->previewAvailable());
        QSignalSpy status(controller_.get(), &Controller::statusChanged);
        QSignalSpy received(controller_.get(), &Controller::previewChanged);
        controller_->refreshPreview();
        controller_->refreshPreview();
        QTRY_COMPARE(count("get_preview"), 1);
        QVERIFY(!controller_->busy());
        QJsonArray devices{QJsonObject{{"serial", "SIM-A"}, {"frames_fresh", true}}};
        reply(last("get_preview"), {{"revision", 1}, {"devices", devices}});
        QTRY_COMPARE(received.count(), 1);
        QCOMPARE(controller_->previewDevices(), devices.toVariantList());
        QCOMPARE(status.count(), 0);
        peer_->abort();
        QTRY_VERIFY(!controller_->connected());
        QVERIFY(controller_->previewDevices().isEmpty());
        QVERIFY(!controller_->previewAvailable());
    }
    void acknowledgedHandFilterReplacesPreviewBeforeNextFastReply() {
        const QJsonObject left{{"hand", "left"}}, right{{"hand", "right"}};
        const QJsonArray both{left, right};
        const auto camera = [&](const QJsonArray& routed) {
            return QJsonArray{QJsonObject{{"serial", "SIM-A"}, {"raw_hands", both}, {"processed_hands", routed}}};
        };
        auto report = state(config(), 2);
        report["preview_available"] = true;
        report["state"] = QJsonObject{{"devices", camera(both)}};
        telemetry(report);
        QTRY_COMPARE(controller_->previewDevices(), camera(both).toVariantList());
        controller_->refreshPreview();
        QTRY_COMPARE(count("get_preview"), 1);
        const auto olderRequest = last("get_preview");
        controller_->setSetting("osc.port", 9010);
        controller_->applyHandFilter("SIM-A", "right");
        QTRY_COMPARE(count("apply_config"), 1);
        const auto apply = last("apply_config");
        QCOMPARE(apply.value("config").toObject().value("osc").toObject().value("port").toInt(), 9000);
        const auto routed = camera(QJsonArray{right});
        auto acknowledged = state(apply.value("config").toObject(), 3);
        acknowledged["preview_available"] = true;
        acknowledged["state"] = QJsonObject{{"devices", routed}};
        reply(apply, acknowledged);
        QTRY_COMPARE(controller_->previewDevices(), routed.toVariantList());
        QCOMPARE(draftPort(), 9010);
        QCOMPARE(controller_->draft().value("devices").toMap().value("SIM-A").toMap().value("hand_filter").toString(), "right");

        QSignalSpy samples(controller_.get(), &Controller::previewChanged);
        QSignalSpy reports(controller_.get(), &Controller::statusChanged);
        reply(olderRequest, {{"revision", 2}, {"devices", camera(both)}});
        telemetry(acknowledged); // This later frame confirms the delayed reply was consumed.
        QTRY_COMPARE(reports.count(), 1);
        QCOMPARE(samples.count(), 0);
        QCOMPARE(controller_->previewDevices(), routed.toVariantList());
        controller_->apply();
        QTRY_COMPARE(count("apply_config"), 2);
        QCOMPARE(last("apply_config").value("expected_revision").toInteger(), 3);
    }
    void liveHandFilterKeepsRevisionProtectionForStaleEdits() {
        controller_->setSetting("osc.port", 9010);
        telemetry(state(config(9030), 2));
        QTRY_COMPARE(controller_->revision(), 2);
        controller_->applyHandFilter("SIM-A", "left");
        QTRY_COMPARE(count("apply_config"), 1);
        const auto live = last("apply_config");
        QCOMPARE(live.value("expected_revision").toInteger(), 2);
        QCOMPARE(live.value("config").toObject().value("osc").toObject().value("port").toInt(), 9030);
        reply(live, state(live.value("config").toObject(), 3));
        QTRY_VERIFY(!controller_->busy());
        QCOMPARE(draftPort(), 9010);
        controller_->apply();
        QTRY_COMPARE(count("apply_config"), 2);
        QCOMPARE(last("apply_config").value("expected_revision").toInteger(), 1);
    }
    void rejectedLiveHandFilterKeepsTheAppliedSelection() {
        controller_->applyHandFilter("SIM-A", "right");
        QTRY_COMPARE(count("apply_config"), 1);
        auto rejected = state(config(), 2);
        rejected["ok"] = false;
        rejected["error"] = "Revision conflict";
        reply(last("apply_config"), rejected);
        QTRY_VERIFY(!controller_->busy());
        QCOMPARE(controller_->config().value("devices").toMap().value("SIM-A").toMap().value("hand_filter").toString(), "both");
        QVERIFY(!controller_->dirty());
        QCOMPARE(controller_->errorMessage(), "Revision conflict");
    }
    void missingHardwareRuntimePreflightIsAsynchronousVisibleAndRetryable() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto variable = "HANDBRIDGE_LEAPC_LIBRARY";
        const bool previouslySet = qEnvironmentVariableIsSet(variable);
        const auto previous = qgetenv(variable);
        const auto restore = qScopeGuard([&] {
            if (previouslySet) qputenv(variable, previous);
            else qunsetenv(variable);
        });
        const auto missing = QDir(directory.path()).absoluteFilePath("missing-LeapC.dll");
        qputenv(variable, missing.toUtf8());

        // This controller has no server. It invokes the actual sibling engine's
        // nontracking --check-leapc path; no SDK connection/device is opened.
        Controller offline("handbridge-missing-runtime-" + QUuid::createUuid().toString(QUuid::Id128));
        QSignalSpy changes(&offline, &Controller::busyChanged);
        for (int attempt = 0; attempt < 2; ++attempt) {
            const auto before = changes.count();
            offline.startHardwareEngine();
            QVERIFY(offline.busy());
            QCOMPARE(offline.statusText(), "Checking Ultraleap runtime");
            QTRY_VERIFY_WITH_TIMEOUT(!offline.busy(), 5000);
            QVERIFY(!offline.connected());
            QVERIFY2(offline.errorMessage().contains("Ultraleap runtime unavailable"), qPrintable(offline.errorMessage()));
            QVERIFY(changes.count() >= before + 2);
            QTRY_VERIFY(offline.findChildren<QProcess*>().isEmpty());
        }
        QCOMPARE(count("apply_config"), 0); // A failed launch does not send settings to another engine.
        QVERIFY(controller_->connected()); // Existing connected controllers remain usable.
    }
};

QTEST_GUILESS_MAIN(ControllerTest)
#include "test_Controller.moc"
