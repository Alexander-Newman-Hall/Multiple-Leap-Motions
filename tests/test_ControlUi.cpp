#include "control/Controller.hpp"
#include <QGuiApplication>
#include <QImage>
#include <QJsonObject>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
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
        QQuickStyle::setStyle(QStringLiteral("Basic"));
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
        QTRY_VERIFY_WITH_TIMEOUT(movedReceiver.hasPendingDatagrams(), 2000);

        QVERIFY2(click("outputEnabled"), "The real OSC enable control must be clickable");
        QTRY_VERIFY(!controller.draft().value(QStringLiteral("osc")).toMap().value(QStringLiteral("enabled")).toBool());
        const auto previousRevision = controller.revision();
        QVERIFY(click("applyButton"));
        QTRY_VERIFY_WITH_TIMEOUT(controller.revision() > previousRevision && !controller.busy(), 5000);
        QVERIFY(!controller.config().value(QStringLiteral("osc")).toMap().value(QStringLiteral("enabled")).toBool());
        QTest::qWait(100);
        while (movedReceiver.hasPendingDatagrams()) movedReceiver.readDatagram(nullptr, 0);
        QTest::qWait(200);
        QVERIFY2(!movedReceiver.hasPendingDatagrams(), "Disabling output in QML must stop actual UDP packets");

        QVERIFY(click("outputEnabled"));
        QTRY_VERIFY(controller.dirty());
        QVERIFY(click("applyButton"));
        QTRY_VERIFY_WITH_TIMEOUT(controller.config().value(QStringLiteral("osc")).toMap().value(QStringLiteral("enabled")).toBool()
                                && !controller.busy(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(movedReceiver.hasPendingDatagrams(), 2000);
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
