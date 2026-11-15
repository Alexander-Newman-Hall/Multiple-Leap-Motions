#pragma once
#include "ipc/Framing.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <stdexcept>
#include <utility>

namespace handbridge::engine {
// Called only on the IPC/file-I/O thread. Worker acknowledgements can arrive
// after a newer synchronous save, so persistence has its own monotonic revision.
class ConfigurationStore {
public:
    explicit ConfigurationStore(QString directory, quint64 savedRevision = 0)
        : directory_(std::move(directory)), savedRevision_(savedRevision) {}
    quint64 savedRevision() const { return savedRevision_; }
    bool saveCurrent(const QJsonObject& configuration, quint64 revision) {
        if (revision < savedRevision_) return false;
        write(QDir(directory_).filePath("current.json"), configuration);
        savedRevision_ = revision;
        return true;
    }
    static void write(const QString& path, const QJsonObject& configuration) {
        const auto bytes = QJsonDocument(configuration).toJson(QJsonDocument::Indented);
        if (bytes.size() > handbridge::ipc::MaximumPayload)
            throw std::runtime_error("Configuration exceeds file size limit");
        if (QFileInfo(path).isSymLink()) throw std::runtime_error("Refusing a symbolic-link configuration destination");
        if (QFile::exists(path)) {
            QFile previous(path);
            if (!previous.open(QIODevice::ReadOnly) || previous.size() > handbridge::ipc::MaximumPayload)
                throw std::runtime_error("Cannot back up previous configuration");
            const auto backup = previous.readAll();
            if (previous.error() != QFile::NoError) throw std::runtime_error("Cannot read previous configuration backup");
            QSaveFile backupFile(path + ".bak");
            if (!backupFile.open(QIODevice::WriteOnly) || backupFile.write(backup) != backup.size() || !backupFile.commit())
                throw std::runtime_error("Cannot commit configuration backup");
        }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
            throw std::runtime_error("Cannot atomically commit configuration");
    }
private:
    QString directory_;
    quint64 savedRevision_;
};
}
