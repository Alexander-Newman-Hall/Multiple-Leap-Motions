#pragma once
#include <QString>

#ifdef Q_OS_MACOS
#include <QCryptographicHash>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace handbridge::ipc {
inline QString socketName(const QString& token) {
#ifdef Q_OS_MACOS
    // Darwin does not enforce QLocalServer::UserAccessOption. Its Unix socket
    // path is short, so keep both the directory and the hashed token bounded.
    const auto uid = geteuid();
    const QByteArray directory = "/private/tmp/handbridge-" + QByteArray::number(uid);
    if (::mkdir(directory.constData(), 0700) != 0 && errno != EEXIST) return {};
    struct stat info{};
    if (::lstat(directory.constData(), &info) != 0 || !S_ISDIR(info.st_mode) ||
        info.st_uid != uid || (info.st_mode & 0777) != 0700) return {};
    const auto digest = QCryptographicHash::hash(token.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QString::fromLatin1(directory + '/' + digest.left(32));
#else
    return token;
#endif
}
}
