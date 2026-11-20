#include "control/Controller.hpp"
#include "engine/Preview.hpp"
#include <QGuiApplication>
#include <QImage>
#include <QJsonObject>
#include <QMetaProperty>
#include <QNetworkDatagram>
#include <QProcess>
#include <QPointer>
#include <QSignalSpy>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QQuickView>
#include <QQmlComponent>
#include <QQmlExpression>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QUdpSocket>
#include <QUuid>
#include <functional>

// Exercise the actual QML controls against the separate native engine and a
// receiver. A changed editor or acknowledgement alone cannot pass this test.
class ControlUi final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QQuickStyle::setStyle(QStringLiteral("Basic")); }

    void addingAndRemovingAHandUpdatesOnlyItsCameraPane_data() {
        QTest::addColumn<bool>("rightFirst");
        QTest::addColumn<bool>("processed");
        QTest::newRow("left-first-raw") << false << false;
        QTest::newRow("right-first-raw") << true << false;
        QTest::newRow("left-first-processed") << false << true;
        QTest::newRow("right-first-processed") << true << true;
    }

    void addingAndRemovingAHandUpdatesOnlyItsCameraPane() {
        QFETCH(bool, rightFirst);
        QFETCH(bool, processed);
        QQuickView view;
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        const auto directory = QFileInfo(QString::fromUtf8(TEST_QML_PATH)).absoluteDir();
        view.setSource(QUrl::fromLocalFile(directory.filePath("PreviewCanvas.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.resize(800, 376); // Preview projects one millimeter to one logical pixel.
        view.rootObject()->setProperty("processed", processed);
        view.show();

        HandData left, right;
        left.handType = "left";
        right.handType = "right";
        left.palm.position.x = -60.f;
        right.palm.position.x = 60.f;
        const auto points = [](const std::vector<HandData>& hands) {
            return handbridge::engine::previewHands(FrameData{"", 1, hands}).toVariantList();
        };
        const auto device = [&](const char* serial, const std::vector<HandData>& hands) {
            const auto observations = points(hands);
            return QVariantMap{{"serial", serial}, {"alias", serial}, {"frames_fresh", true},
                {"processed_hands", observations}, {"raw_hands", observations}};
        };
        const QColor leftColor(processed ? "#bbaff3" : "#b1edbc");
        const QColor rightColor(processed ? "#f1b4d5" : "#9fcbdc");
        const auto paint = [&](const std::vector<HandData>& hands, bool secondCamera) {
            QVariantList devices{device("A", hands)};
            if (secondCamera) devices.push_back(device("B", {left}));
            view.rootObject()->setProperty("devices", devices);
        };
        const auto palms = [&](bool seeLeft, bool seeRight, bool seeOtherLeft, bool seeOtherRight = false) {
            const auto image = view.grabWindow();
            if (image.isNull()) return false;
            const auto pixel = [&](int x) {
                return image.pixelColor(x * image.width() / 800, 188 * image.height() / 376);
            };
            const int center = view.rootObject()->property("paneCount").toInt() == 2 ? 200 : 400;
            return (pixel(center - 60) == leftColor) == seeLeft && (pixel(center + 60) == rightColor) == seeRight &&
                (pixel(540) == leftColor) == seeOtherLeft && (pixel(660) == rightColor) == seeOtherRight;
        };
        const auto first = rightFirst ? right : left;
        const auto second = rightFirst ? left : right;
        paint({first}, false);
        QTRY_VERIFY(palms(!rightFirst, rightFirst, false));
        paint({first, second}, false);
        QTRY_VERIFY(palms(true, true, false));
        paint({second}, false);
        QTRY_VERIFY(palms(rightFirst, !rightFirst, false));
        paint({first, second}, true);
        QTRY_VERIFY(palms(true, true, true));
        paint({}, true);
        QTRY_VERIFY(palms(false, false, true));

        // Both cameras observe the same two hands. Only camera A is routed right-only.
        auto cameraA = device("A", {left, right});
        const auto cameraB = device("B", {left, right});
        cameraA["processed_hands"] = points({right});
        view.rootObject()->setProperty("devices", QVariantList{cameraA, cameraB});
        QTRY_VERIFY(palms(!processed, true, true, true));
        view.rootObject()->setProperty("deviceSettings", QVariantMap{{"A", QVariantMap{{"enabled", false}}}});
        QTRY_VERIFY(palms(!processed, !processed, true, true));
        view.rootObject()->setProperty("outputMessage", QStringLiteral("OSC MUTED"));
        QTRY_VERIFY(palms(!processed, !processed, !processed, !processed));
    }

    void largeTransportTotalsFitAndKeepExactDigits() {
        Controller offline("hb-counter-test-" + QUuid::createUuid().toString(QUuid::Id128));
        QVariantMap presentation;
        const auto& meta = Controller::staticMetaObject;
        for (int i = meta.propertyOffset(); i < meta.propertyCount(); ++i) {
            const auto property = meta.property(i);
            presentation.insert(QString::fromLatin1(property.name()), property.read(&offline));
        }
        presentation["connected"] = true;
        QQmlApplicationEngine view;
        auto publish = [&](QVariantMap output) {
            presentation["status"] = QVariantMap{{"output", output}};
            view.rootContext()->setContextProperty("controller", presentation);
        };
        // Also exercise the numeric fields supplied by earlier engines.
        publish({{"send_errors", 9999}, {"packets_sent", 10000}});
        view.load(QUrl::fromLocalFile(QString::fromUtf8(TEST_QML_PATH)));
        QVERIFY(!view.rootObjects().isEmpty());
        auto* window = qobject_cast<QQuickWindow*>(view.rootObjects().first());
        QVERIFY(window);
        window->resize(window->minimumSize());
        auto* overview = window->findChild<QQuickItem*>("overviewSendErrors");
        auto* errors = window->findChild<QQuickItem*>("transportSendErrors");
        auto* packets = window->findChild<QQuickItem*>("transportPacketsSent");
        QVERIFY(overview && errors && packets);
        QCOMPARE(overview->property("value").toString(), "9999");
        QCOMPARE(packets->property("value").toString(), "10K");

        QVariantMap output{{"send_errors", -1}, {"send_errors_exact", "18446744073709551614"},
                           {"packets_sent_exact", "12345678901"}, {"bytes_sent", 1234567890123456LL}};
        publish(output);
        QCOMPARE(overview->property("value").toString(), "1.8e19");
        QCOMPARE(errors->property("value"), overview->property("value"));
        QCOMPARE(packets->property("value").toString(), "12.3B");
        output["send_errors_exact"] = "18446744073709551615";
        publish(output);
        for (auto* cell : {overview, errors}) {
            // The compact label can stay the same while the exact, live tooltip changes.
            QCOMPARE(cell->property("value").toString(), "1.8e19");
            QQmlExpression tooltip(QQmlEngine::contextForObject(cell), cell, "ToolTip.text");
            QVERIFY(tooltip.evaluate().toString().contains("18446744073709551615"));
            QVERIFY(!tooltip.hasError());
        }
        window->setProperty("pageIndex", 3);
        QTRY_VERIFY(packets->width() > 0 && errors->isVisible());
        for (auto* cell : {packets, errors}) {
            const auto right = cell->mapToScene(QPointF(cell->width(), 0)).x();
            QVERIFY(right <= window->width() + 1);
        }
    }

    void tabletopPreviewOrientation() {
        QQmlEngine engine;
        const auto path = QFileInfo(QString::fromUtf8(TEST_QML_PATH)).dir().filePath("PreviewCanvas.qml");
        QQmlComponent component(&engine, QUrl::fromLocalFile(path));
        std::unique_ptr<QObject> view(component.create());
        QVERIFY2(view, qPrintable(component.errorString()));
        view->setProperty("width", 400);
        view->setProperty("height", 240);
        view->setProperty("devices", QVariantList{QVariantMap{{"serial", "A"}}, QVariantMap{{"serial", "B"}}});
        const auto project = [&](const QString& arguments) {
            QQmlExpression expression(engine.rootContext(), view.get(), "project(" + arguments + ")");
            return expression.evaluate().toPointF();
        };
        // Flat hand, fingers pointing away. Height above the table does not change the XZ projection.
        QCOMPARE(project("[0, 190, 0], 100"), QPointF(100, 120));
        QCOMPARE(project("[0, 160, 80], 100"), QPointF(100, 160));  // wrist
        QCOMPARE(project("[80, 160, -80], 100"), QPointF(140, 80)); // fingertip
        QCOMPARE(project("[80, 260, -80], 300"), QPointF(340, 80)); // second camera
        view->setProperty("processed", true);
        view->setProperty("units", "m");
        QCOMPARE(project("[0.08, 0.16, -0.08], 100"), QPointF(140, 80));
    }

    void qmlOutputSwitchChangesActualUdp() {
        QTemporaryDir state;
        QVERIFY(state.isValid());
        QUdpSocket receiver;
        QVERIFY(receiver.bind(QHostAddress(QHostAddress::LocalHost), quint16(0)));
        const auto endpoint = QStringLiteral("hb-ui-") + QUuid::createUuid().toString(QUuid::Id128);
        QProcess process;
        process.start(QString::fromUtf8(TEST_ENGINE_PATH),
            {QStringLiteral("--simulate"), QStringLiteral("--endpoint"), endpoint,
             QStringLiteral("--state-dir"), state.path(), QStringLiteral("--port"),
             QString::number(receiver.localPort()), QStringLiteral("--duration-ms"), QStringLiteral("15000")});
        QVERIFY(process.waitForStarted());
        Controller controller(endpoint);
        QTRY_VERIFY_WITH_TIMEOUT(controller.connected(), 7000);
        QQmlApplicationEngine view;
        view.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
        view.rootContext()->setContextProperty(QStringLiteral("initialPage"), QStringLiteral("overview"));
        view.load(QUrl::fromLocalFile(QString::fromUtf8(TEST_QML_PATH)));
        QVERIFY2(!view.rootObjects().isEmpty(), "Native QML view failed to load");
        auto window = qobject_cast<QQuickWindow*>(view.rootObjects().first());
        QVERIFY(window);
        QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(receiver.hasPendingDatagrams(), 2000);

        std::function<QQuickItem*(QQuickItem*, const QString&)> findVisual;
        findVisual = [&](QQuickItem* parent, const QString& name) -> QQuickItem* {
            if (parent->objectName() == name) return parent;
            for (auto child : parent->childItems())
                if (auto found = findVisual(child, name)) return found;
            return nullptr;
        };
        auto click = [&](const char* name) {
            auto item = findVisual(window->contentItem(), QString::fromLatin1(name));
            if (!item || !item->isVisible() || !item->isEnabled()) return false;
            const auto center = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center.toPoint());
            return true;
        };

        // Live preview metadata describes acknowledged engine values, never an
        // unapplied unit/bypass draft. Raw bypass remains millimetres even if a
        // saved inactive transform prefers metres.
        auto preview = findVisual(window->contentItem(), QStringLiteral("coordinatePreview"));
        QVERIFY(preview);
        QVERIFY(preview->property("processed").toBool());
        QVERIFY(click("rawPreviewButton"));
        QVERIFY(!preview->property("processed").toBool());
        QVERIFY(click("outputPreviewButton"));
        QVERIFY(preview->property("processed").toBool());
        QCOMPARE(preview->property("units").toString(), QStringLiteral("mm"));
        controller.setSetting(QStringLiteral("processing.bypass"), false);
        controller.setSetting(QStringLiteral("processing.output_units"), QStringLiteral("m"));
        QCOMPARE(preview->property("units").toString(), QStringLiteral("mm"));
        QVERIFY(click("applyButton"));
        QTRY_COMPARE_WITH_TIMEOUT(preview->property("units").toString(), QStringLiteral("m"), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        controller.setSetting(QStringLiteral("processing.bypass"), true);
        QCOMPARE(preview->property("units").toString(), QStringLiteral("m"));
        QVERIFY(click("applyButton"));
        QTRY_COMPARE_WITH_TIMEOUT(preview->property("units").toString(), QStringLiteral("mm"), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);

        QSignalSpy samples(&controller, &Controller::previewChanged);
        QTRY_VERIFY_WITH_TIMEOUT(samples.count() >= 3, 2000);
        QVERIFY(click("navDevices"));
        const auto samplesAtNavigation = samples.count();
        QSignalSpy reports(&controller, &Controller::statusChanged);
        QPointer<QQuickItem> alias = findVisual(window->contentItem(), QStringLiteral("alias_SIM-A"));
        QVERIFY(alias && click("alias_SIM-A"));
        QTRY_VERIFY_WITH_TIMEOUT(reports.count() >= 3, 2000);
        QVERIFY(alias && alias->hasActiveFocus());
        QVERIFY(samples.count() <= samplesAtNavigation + 1);
        QTRY_VERIFY_WITH_TIMEOUT(click("navOsc"), 3000);
        QTest::qWait(100);

        // A user must be able to type a destination and click Apply directly,
        // without needing a hidden Tab/Enter step to commit the editor first.
        QUdpSocket movedReceiver;
        QVERIFY(movedReceiver.bind(QHostAddress(QHostAddress::LocalHost), quint16(0)));
        QVERIFY(click("oscPort"));
        QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
        for (const auto digit : QString::number(movedReceiver.localPort()))
            QTest::keyClick(window, digit.toLatin1());
        QVERIFY2(click("applyButton"), "Apply must accept the active text editor's change");
        QTRY_COMPARE_WITH_TIMEOUT(controller.config().value(QStringLiteral("osc")).toMap()
                                 .value(QStringLiteral("port")).toInt(), int(movedReceiver.localPort()), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(movedReceiver.hasPendingDatagrams(), 2000);

        const auto skeletonEnabled = [&] {
            return controller.config().value("channels").toMap().value("sendFullSkeleton").toBool();
        };
        QVERIFY(!skeletonEnabled());
        QVERIFY(click("channel_sendFullSkeleton"));
        QVERIFY(click("applyButton"));
        QTRY_VERIFY_WITH_TIMEOUT(skeletonEnabled() && !controller.busy(), 5000);
        bool sawSkeleton = false;
        const auto receivedSkeleton = [&] {
            while (!sawSkeleton && movedReceiver.hasPendingDatagrams()) {
                const auto packet = movedReceiver.receiveDatagram().data();
                const auto address = packet.left(packet.indexOf('\0'));
                sawSkeleton = address.startsWith("/handbridge/v2/") && address.endsWith("/skeleton");
            }
            return sawSkeleton;
        };
        QTRY_VERIFY_WITH_TIMEOUT(receivedSkeleton(), 2000);

        QVERIFY2(click("outputEnabled"), "The real OSC enable control must be clickable");
        QTRY_VERIFY(!controller.draft().value(QStringLiteral("osc")).toMap().value(QStringLiteral("enabled")).toBool());
        QVERIFY(preview->property("outputMessage").toString().isEmpty());
        const auto previousRevision = controller.revision();
        QVERIFY(click("applyButton"));
        QTRY_VERIFY_WITH_TIMEOUT(controller.revision() > previousRevision && !controller.busy(), 5000);
        QVERIFY(!controller.config().value(QStringLiteral("osc")).toMap().value(QStringLiteral("enabled")).toBool());
        QCOMPARE(preview->property("outputMessage").toString(), QStringLiteral("OSC MUTED"));
        QTest::qWait(100);
        while (movedReceiver.hasPendingDatagrams()) movedReceiver.readDatagram(nullptr, 0);
        QTest::qWait(200);
        QVERIFY2(!movedReceiver.hasPendingDatagrams(), "Disabling output in QML must stop actual UDP packets");

        QVERIFY(click("outputEnabled"));
        QTRY_VERIFY(controller.dirty());
        QVERIFY(click("applyButton"));
        QTRY_VERIFY_WITH_TIMEOUT(controller.config().value(QStringLiteral("osc")).toMap().value(QStringLiteral("enabled")).toBool()
                                && !controller.busy(), 5000);
        QVERIFY(preview->property("outputMessage").toString().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(movedReceiver.hasPendingDatagrams(), 2000);
        const auto channelSettings = controller.config().value("channels").toMap();
        for (const auto& key : channelSettings.keys())
            controller.setSetting("channels." + key, false);
        QVERIFY(click("applyButton"));
        QTRY_COMPARE(preview->property("outputMessage").toString(), QStringLiteral("NO OSC CHANNELS ENABLED"));
        controller.setSetting("channels.sendPalm", true);
        QVERIFY(click("applyButton"));
        QTRY_VERIFY(preview->property("outputMessage").toString().isEmpty());
        QVERIFY2(!window->grabWindow().isNull(), "The native Quick window must render");

        // Closing the native controller window must leave the owned test engine
        // running and the receiver active. The process test also covers kills.
        window->close();
        QTest::qWait(100);
        while (movedReceiver.hasPendingDatagrams()) movedReceiver.readDatagram(nullptr, 0);
        QTRY_VERIFY_WITH_TIMEOUT(movedReceiver.hasPendingDatagrams(), 2000);
        QCOMPARE(process.state(), QProcess::Running);
        process.terminate();
        if (!process.waitForFinished(3000)) { process.kill(); process.waitForFinished(); }
    }
};
QTEST_MAIN(ControlUi)
#include "test_ControlUi.moc"
