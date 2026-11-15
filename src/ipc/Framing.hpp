#pragma once
#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QList>
#include <QString>
#include <QtEndian>

namespace handbridge::ipc {
inline constexpr quint32 MaximumPayload = 64 * 1024;
inline constexpr qsizetype MaximumBuffered = 256 * 1024;
inline QString defaultEndpoint() {
    return QStringLiteral("handbridge-%1-v1").arg(QString::fromLatin1(
        QCryptographicHash::hash(QDir::homePath().toUtf8(), QCryptographicHash::Sha256).toHex().left(16)));
}
inline QByteArray encode(const QJsonObject& object) {
    const auto payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    if (payload.isEmpty() || payload.size() > MaximumPayload) return {};
    QByteArray result(4, Qt::Uninitialized);
    qToBigEndian<quint32>(static_cast<quint32>(payload.size()), result.data());
    result += payload;
    return result;
}
// Retain an incomplete tail. Return false on malformed/oversized frames.
inline bool takeFrames(QByteArray& buffer, QList<QJsonObject>& result, QString& error) {
    if (buffer.size() > MaximumBuffered) { error = QStringLiteral("IPC buffer limit exceeded"); return false; }
    while (buffer.size() >= 4 && result.size() < 16) {
        const auto size = qFromBigEndian<quint32>(buffer.constData());
        if (size == 0 || size > MaximumPayload) { error = QStringLiteral("Invalid IPC payload size"); return false; }
        if (buffer.size() < static_cast<qsizetype>(size) + 4) break;
        QJsonParseError parse;
        const auto document = QJsonDocument::fromJson(buffer.mid(4, size), &parse);
        if (parse.error != QJsonParseError::NoError || !document.isObject()) {
            error = QStringLiteral("IPC requires a UTF-8 JSON object"); return false;
        }
        buffer.remove(0, size + 4);
        result.push_back(document.object());
    }
    return true;
}
}
