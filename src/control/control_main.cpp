#include "Controller.hpp"
#include "ipc/Framing.hpp"

#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QOperatingSystemVersion>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTimer>
#include <memory>
#ifdef Q_OS_MACOS
#include "MacMenuBar.hpp"
#endif

int main(int argc, char** argv) {
#ifdef Q_OS_MACOS
    // Qt's Cocoa plugin otherwise activates the app even with LSUIElement set.
    qputenv("QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM", "1");
#endif
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("HandBridgeControl"));
    QCoreApplication::setOrganizationName(QStringLiteral("HandBridge"));
    QCoreApplication::setApplicationVersion(QStringLiteral("2.0.7"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QFont font;
    font.setFamily(QFontDatabase::families().contains(QStringLiteral("Segoe UI")) ? QStringLiteral("Segoe UI") : QStringLiteral("DejaVu Sans"));
    font.setPixelSize(13);
    app.setFont(font);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Hand Bridge native controller. Tracking/OSC run in a separate engine process."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("endpoint"), QStringLiteral("Local control endpoint token."), QStringLiteral("token"), handbridge::ipc::defaultEndpoint()});
    parser.addOption({QStringLiteral("start-engine"), QStringLiteral("Start a detached hardware engine if none is connected.")});
    parser.addOption({QStringLiteral("attach-only"), QStringLiteral("Connect to a running engine without starting one automatically.")});
    parser.addOption({QStringLiteral("demo"), QStringLiteral("Start a detached simulated engine if none is connected. No real camera support is implied.")});
    parser.addOption({QStringLiteral("exit-after-ms"), QStringLiteral("Close only the controller after N milliseconds; useful for automated lifecycle tests."), QStringLiteral("N"), QStringLiteral("0")});
    parser.addOption({QStringLiteral("screenshot"), QStringLiteral("Capture the native window to this PNG path after connection/rendering."), QStringLiteral("path")});
    parser.addOption({QStringLiteral("page"), QStringLiteral("Initial page: overview, devices, processing, osc, profiles."), QStringLiteral("name"), QStringLiteral("overview")});
#ifdef Q_OS_MACOS
    parser.addOption({QStringLiteral("show-window"), QStringLiteral("Open the controller window immediately on macOS.")});
#endif
    parser.process(app);

    const auto endpoint = parser.value(QStringLiteral("endpoint"));
    if (!QRegularExpression(QStringLiteral("^[A-Za-z0-9_.-]{1,80}$")).match(endpoint).hasMatch())
        parser.showHelp(1);
    bool durationValid = false;
    const auto duration = parser.value(QStringLiteral("exit-after-ms")).toInt(&durationValid);
    const QStringList pages = {QStringLiteral("overview"), QStringLiteral("devices"), QStringLiteral("processing"), QStringLiteral("osc"), QStringLiteral("profiles")};
    if (!durationValid || duration < 0 || !pages.contains(parser.value(QStringLiteral("page")))) parser.showHelp(1);

    const bool simulate = parser.isSet(QStringLiteral("demo"));
    const bool hardware = parser.isSet(QStringLiteral("start-engine"));
    const bool attachOnly = parser.isSet(QStringLiteral("attach-only"));
    if ((simulate && hardware) || (attachOnly && (simulate || hardware))) parser.showHelp(1);
    auto startup = Controller::Startup::AttachOnly;
    if (simulate) startup = Controller::Startup::Simulator;
    else if (hardware || (!attachOnly && QOperatingSystemVersion::currentType() == QOperatingSystemVersion::MacOS))
        startup = Controller::Startup::Hardware;
    Controller controller(endpoint, nullptr, startup);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("initialPage"), parser.value(QStringLiteral("page")));
#ifdef Q_OS_MACOS
    const bool menuBarMode = app.platformName() == QStringLiteral("cocoa");
    const bool showAtLaunch = parser.isSet(QStringLiteral("show-window")) || parser.isSet(QStringLiteral("page"))
                             || parser.isSet(QStringLiteral("screenshot"));
    if (menuBarMode)
        engine.setInitialProperties({{QStringLiteral("visible"), false}});
#endif
    const QUrl url(QStringLiteral("qrc:/handbridge/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app, [url](QObject* object, const QUrl& source) {
        if (!object && source == url) QCoreApplication::exit(1);
    }, Qt::QueuedConnection);
    engine.load(url);
    if (engine.rootObjects().isEmpty()) return 1;
#ifdef Q_OS_MACOS
    if (menuBarMode) {
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (!window) return 1;
        if (!installMacMenuBar(window) || showAtLaunch) showMacControllerWindow(window);
    }
#endif
    if (duration > 0) QTimer::singleShot(duration, &app, &QCoreApplication::quit);
    if (parser.isSet(QStringLiteral("screenshot"))) {
        auto timer = new QTimer(&app);
        timer->setInterval(100);
        auto attempts = std::make_shared<int>(0);
        const auto path = parser.value(QStringLiteral("screenshot"));
        QObject::connect(timer, &QTimer::timeout, &app, [&engine, &controller, timer, attempts, path] {
            ++*attempts;
            if (*attempts < 15 || (!controller.connected() && *attempts < 100)) return;
            auto window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));
            if (window) {
                QDir().mkpath(QFileInfo(path).absolutePath());
                const auto image = window->grabWindow();
                if (image.isNull() || !image.save(path)) qWarning("Could not save the native controller screenshot.");
            }
            timer->stop();
            timer->deleteLater();
        });
        timer->start();
    }
    return app.exec();
}
