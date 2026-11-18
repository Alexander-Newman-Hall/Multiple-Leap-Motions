#include "MacMenuBar.hpp"

#include <QGuiApplication>
#include <QWindow>
#import <AppKit/AppKit.h>

void showMacControllerWindow(QWindow* window) {
    window->setWindowStates(window->windowStates() & ~Qt::WindowMinimized);
    window->show();
    if (@available(macOS 14.0, *)) {
        [NSApp activate];
    } else {
        [NSApp activateIgnoringOtherApps:YES];
    }
    window->raise();
    window->requestActivate();
}

@interface HBMenuActions : NSObject
@property(nonatomic, assign) QWindow* window;
- (void)openControls:(id)sender;
- (void)quitController:(id)sender;
- (void)installReopenHandler:(NSNotification*)notification;
@end

@implementation HBMenuActions
- (void)openControls:(id)sender {
    showMacControllerWindow(self.window);
}

- (void)quitController:(id)sender {
    QCoreApplication::quit();
}

- (void)reopen:(NSAppleEventDescriptor*)event reply:(NSAppleEventDescriptor*)reply {
    [self openControls:nil];
}

- (void)installReopenHandler:(NSNotification*)notification {
    [NSAppleEventManager.sharedAppleEventManager setEventHandler:self
        andSelector:@selector(reopen:reply:) forEventClass:kCoreEventClass andEventID:kAEReopenApplication];
}
@end

NSStatusItem* installMacMenuBar(QWindow* window) {
    // Direct command-line launches have no bundle LSUIElement activation policy.
    if (NSApp.activationPolicy == NSApplicationActivationPolicyProhibited)
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    NSStatusItem* item = [NSStatusBar.systemStatusBar statusItemWithLength:NSVariableStatusItemLength];
    if (!item.button) {
        if (item) [NSStatusBar.systemStatusBar removeStatusItem:item];
        return nil;
    }
    HBMenuActions* actions = [HBMenuActions new];
    actions.window = window;
    NSImage* hand = [NSImage imageWithSystemSymbolName:@"hand.raised" accessibilityDescription:@"Hand Bridge"];
    if (hand) {
        [hand setTemplate:YES];
        item.button.image = hand;
    } else {
        item.button.title = @"HB";
    }
    item.button.toolTip = @"Hand Bridge";
    [item.button setAccessibilityLabel:@"Hand Bridge"];
    NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Hand Bridge"];
    [menu addItemWithTitle:@"Open Hand Bridge" action:@selector(openControls:) keyEquivalent:@""].target = actions;
    [menu addItem:NSMenuItem.separatorItem];
    [menu addItemWithTitle:@"Quit Controller" action:@selector(quitController:) keyEquivalent:@""].target = actions;
    item.menu = menu;

    // Qt can have briefly run AppKit during QML setup. Install now and again
    // if AppKit's launch notification is still pending.
    [actions installReopenHandler:nil];
    if (!NSApp.running) {
        [NSNotificationCenter.defaultCenter addObserver:actions selector:@selector(installReopenHandler:)
            name:NSApplicationWillFinishLaunchingNotification object:NSApp];
    }
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QObject::connect(window, &QObject::destroyed, qGuiApp, [item, actions] {
        [NSNotificationCenter.defaultCenter removeObserver:actions];
        [NSAppleEventManager.sharedAppleEventManager removeEventHandlerForEventClass:kCoreEventClass
            andEventID:kAEReopenApplication];
        [NSStatusBar.systemStatusBar removeStatusItem:item];
    });
    return item;
}
