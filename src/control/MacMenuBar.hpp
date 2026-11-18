#pragma once

#include <QtCore/qdarwinhelpers.h>

Q_FORWARD_DECLARE_OBJC_CLASS(NSStatusItem);
class QWindow;

// The status item and its actions live as long as the controller window.
NSStatusItem* installMacMenuBar(QWindow* window);
void showMacControllerWindow(QWindow* window);
