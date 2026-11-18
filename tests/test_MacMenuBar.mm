#import <AppKit/AppKit.h>

#include "control/MacMenuBar.hpp"
#include <QGuiApplication>
#include <QTest>
#include <QWindow>

class MacMenuBarTest : public QObject {
    Q_OBJECT
private slots:
    void statusMenuReopensClosedAndMinimizedControls() {
        QWindow window;
        window.resize(320, 200);
        NSStatusItem* item = installMacMenuBar(&window);
        QVERIFY(item != nil);
        QVERIFY(item.button.image.isTemplate || item.button.title.length > 0);
        QVERIFY([item.button.accessibilityLabel isEqualToString:@"Hand Bridge"]);
        QVERIFY(!window.isVisible());
        QVERIFY(!QGuiApplication::quitOnLastWindowClosed());

        NSMenu* menu = item.menu;
        const NSInteger openIndex = [menu indexOfItemWithTitle:@"Open Hand Bridge"];
        QVERIFY(openIndex >= 0);
        QVERIFY([menu itemWithTitle:@"Quit Controller"] != nil);

        [menu performActionForItemAtIndex:openIndex];
        QTRY_VERIFY(window.isVisible());
        window.close();
        QTRY_VERIFY(!window.isVisible());
        [menu performActionForItemAtIndex:openIndex];
        QTRY_VERIFY(window.isVisible());

        window.showMinimized();
        QTRY_COMPARE(window.visibility(), QWindow::Minimized);
        [menu performActionForItemAtIndex:openIndex];
        QTRY_VERIFY(window.isVisible() && window.visibility() != QWindow::Minimized);
    }
};

int main(int argc, char** argv) {
    QGuiApplication application(argc, argv);
    if (application.platformName() != QStringLiteral("cocoa")) {
        qInfo("A native Cocoa session is required for the menu bar test.");
        return 77;
    }
    MacMenuBarTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_MacMenuBar.moc"
