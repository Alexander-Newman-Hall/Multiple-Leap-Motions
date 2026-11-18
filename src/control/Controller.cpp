#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "Controller.hpp"
#include "ipc/Endpoint.hpp"
#include "ipc/Framing.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <algorithm>
#include <utility>

namespace {
QJsonValue replacePath(const QJsonValue& current, const QStringList& path, int index,
                       const QJsonValue& replacement, bool& valid) {
    if (index == path.size()) { valid = true; return replacement; }
    if (current.isObject()) {
        auto object = current.toObject();
        if (!object.contains(path[index])) return current;
        object[path[index]] = replacePath(object.value(path[index]), path, index + 1, replacement, valid);
        return object;
    }
    if (current.isArray()) {
        bool numeric = false;
        const auto position = path[index].toInt(&numeric);
        auto array = current.toArray();
        if (!numeric || position < 0 || position >= array.size()) return current;
        array[position] = replacePath(array.at(position), path, index + 1, replacement, valid);
        return array;
    }
    return current;
}
QStringList stringList(const QJsonValue& value) {
    QStringList result;
    for (const auto& item : value.toArray()) {
        if (item.isString()) result.push_back(item.toString());
        else if (item.isObject()) result.push_back(QString::fromUtf8(QJsonDocument(item.toObject()).toJson(QJsonDocument::Compact)));
    }
    return result;
}
}

Controller::Controller(QString endpoint, QObject* parent, Startup startup)
    : QObject(parent), endpoint_(std::move(endpoint)), startup_(startup) {
    clock_.start();
    socket_.setReadBufferSize(256 * 1024);
    reconnectTimer_.setInterval(1000);
    timeoutTimer_.setInterval(250);
    connect(&socket_, &QLocalSocket::connected, this, [this] {
        startup_ = Startup::AttachOnly;
        connecting_ = false;
        errorMessage_.clear();
        if (!dirty()) rejected_ = false;
        request(QStringLiteral("get_state"));
        request(QStringLiteral("list_profiles"));
        emit uiChanged();
    });
    connect(&socket_, &QLocalSocket::readyRead, this, &Controller::receive);
    connect(&socket_, &QLocalSocket::disconnected, this, &Controller::disconnected);
    connect(&socket_, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError error) {
        connecting_ = false;
        if (connected_) disconnected();
        if (startup_ != Startup::AttachOnly &&
            (error == QLocalSocket::ServerNotFoundError || error == QLocalSocket::ConnectionRefusedError)) {
            startEngine(startup_ == Startup::Simulator);
        }
        emit connectionChanged();
        emit uiChanged();
    });
    connect(&reconnectTimer_, &QTimer::timeout, this, &Controller::connectToEngine);
    connect(&timeoutTimer_, &QTimer::timeout, this, &Controller::checkTimeouts);
    reconnectTimer_.start();
    timeoutTimer_.start();
    QTimer::singleShot(0, this, &Controller::connectToEngine);
}
Controller::~Controller() {
    reconnectTimer_.stop();
    timeoutTimer_.stop();
    socket_.disconnect(this);
    socket_.abort(); // Close this IPC client only; the detached engine survives.
    if (launchProbe_) { launchProbe_->disconnect(this); launchProbe_->kill(); }
}

bool Controller::isMutation(const QString& command) {
    return command != QStringLiteral("get_state") && command != QStringLiteral("list_profiles");
}
bool Controller::busy() const {
    if (launchProbe_) return true;
    for (const auto& item : pending_) if (isMutation(item.command)) return true;
    return false;
}
QString Controller::statusText() const {
    if (launchProbe_) return QStringLiteral("Checking Ultraleap runtime");
    if (!connected_) return connecting_ ? QStringLiteral("Connecting") : QStringLiteral("Engine offline");
    if (busy()) {
        for (const auto& item : pending_) {
            if (item.command == QStringLiteral("apply_config")) return QStringLiteral("Applying");
            if (item.command == QStringLiteral("save_profile")) return QStringLiteral("Saving");
            if (item.command == QStringLiteral("load_profile")) return QStringLiteral("Loading");
        }
        return QStringLiteral("Working");
    }
    if (rejected_) return QStringLiteral("Rejected");
    if (dirty()) return QStringLiteral("Editing · not applied");
    return revision_ > 0 && savedRevision_ == revision_ ? QStringLiteral("Applied · saved") : QStringLiteral("Applied · unsaved");
}

void Controller::connectToEngine() {
    if (socket_.state() != QLocalSocket::UnconnectedState) return;
    received_.clear();
    const auto name = handbridge::ipc::socketName(endpoint_);
    if (name.isEmpty()) {
        setError(QStringLiteral("Cannot open the private control socket directory."));
        return;
    }
    connecting_ = true;
    socket_.connectToServer(name, QIODevice::ReadWrite);
    emit connectionChanged();
    emit uiChanged();
}
void Controller::disconnected() {
    const bool wasConnected = connected_;
    connected_ = connecting_ = false;
    received_.clear();
    pending_.clear();
    if (wasConnected && errorMessage_.isEmpty())
        errorMessage_ = QStringLiteral("Engine connection lost. Reconnecting; unapplied edits are retained.");
    emit connectionChanged();
    emit busyChanged();
    emit uiChanged();
}
void Controller::setError(const QString& message) {
    errorMessage_ = message;
    rejected_ = true;
    emit uiChanged();
}
bool Controller::request(const QString& command, QJsonObject fields) {
    if (socket_.state() != QLocalSocket::ConnectedState) {
        setError(QStringLiteral("Connect to an engine before sending commands."));
        return false;
    }
    if (pending_.size() >= 16 || socket_.bytesToWrite() > 192 * 1024) {
        setError(QStringLiteral("The engine is busy. Wait for the current request to finish."));
        return false;
    }
    const auto id = nextId_++;
    fields[QStringLiteral("version")] = 1;
    fields[QStringLiteral("id")] = id;
    fields[QStringLiteral("command")] = command;
    const auto bytes = handbridge::ipc::encode(fields);
    if (bytes.isEmpty() || bytes.size() > 64 * 1024 + 4) {
        setError(QStringLiteral("The request exceeds the local protocol size limit."));
        return false;
    }
    Pending pending{command, clock_.elapsed(), {}};
    if (command == QStringLiteral("apply_config")) pending.submittedConfig = fields.value(QStringLiteral("config")).toObject();
    pending_.insert(id, pending);
    if (socket_.write(bytes) != bytes.size()) {
        pending_.remove(id);
        setError(QStringLiteral("The local engine request could not be queued."));
        socket_.abort();
        return false;
    }
    emit busyChanged();
    emit uiChanged();
    return true;
}

void Controller::receive() {
    received_ += socket_.readAll();
    QList<QJsonObject> messages;
    QString error;
    if (!handbridge::ipc::takeFrames(received_, messages, error)) {
        setError(QStringLiteral("Invalid engine response: ") + error);
        socket_.abort();
        return;
    }
    for (const auto& item : messages) message(item);
    // The parser limits work per event-loop turn. A coalesced socket read can
    // contain more complete frames without another readyRead notification.
    if (messages.size() == 16 && received_.size() >= 4)
        QTimer::singleShot(0, this, &Controller::receive);
}
void Controller::adoptState(const QJsonObject& response, bool replaceDraft) {
    const bool wasDirty = dirty();
    const auto incomingRevision = response.value(QStringLiteral("revision")).toInteger(revision_);
    // Socket reconnect starts a new session, whose revision can legitimately be
    // lower after an engine restart. Within a session, delayed replies must not
    // overwrite newer telemetry from another controller or queued command.
    const bool stale = connected_ && response.contains(QStringLiteral("revision")) && incomingRevision < revision_;
    if (stale) {
        if (response.contains(QStringLiteral("saved_revision")))
            savedRevision_ = std::max(savedRevision_, response.value(QStringLiteral("saved_revision")).toInteger());
        emit uiChanged();
        return;
    }
    const bool initialState = !connected_;
    const bool retainDraft = !replaceDraft && dirty();
    if (response.value(QStringLiteral("config")).isObject()) {
        const auto next = response.value(QStringLiteral("config")).toObject();
        const auto nextRevision = incomingRevision;
        const bool configChanged = config_ != next || revision_ != nextRevision;
        config_ = next;
        revision_ = nextRevision;
        if (!retainDraft) {
            const bool editorChanged = draft_ != config_;
            draft_ = config_;
            draftRevision_ = revision_;
            if (editorChanged) emit draftChanged();
        }
        if (configChanged) emit this->configChanged();
        if (!connected_) {
            connected_ = true;
            connecting_ = false;
            emit connectionChanged();
        }
    }
    if (response.contains(QStringLiteral("saved_revision"))) {
        const auto saved = response.value(QStringLiteral("saved_revision")).toInteger();
        savedRevision_ = initialState ? saved : std::max(savedRevision_, saved);
    }
    if (response.value(QStringLiteral("state")).isObject()) {
        status_ = response.value(QStringLiteral("state")).toObject();
        emit statusChanged();
    }
    if (response.value(QStringLiteral("logs")).isArray()) {
        const auto next = stringList(response.value(QStringLiteral("logs")));
        if (next != logs_) { logs_ = next; emit logsChanged(); }
    }
    if (response.value(QStringLiteral("profiles")).isArray()) {
        const auto next = stringList(response.value(QStringLiteral("profiles")));
        if (next != profiles_) { profiles_ = next; emit profilesChanged(); }
    }
    if (wasDirty != dirty()) emit dirtyChanged();
    emit uiChanged();
}
void Controller::message(const QJsonObject& response) {
    if (response.value(QStringLiteral("version")).toInt() != 1) {
        setError(QStringLiteral("The engine uses an unsupported control protocol."));
        socket_.abort();
        return;
    }
    if (response.value(QStringLiteral("event")).toString() == QStringLiteral("state")) {
        adoptState(response);
        return;
    }
    const auto id = response.value(QStringLiteral("id")).toInteger(-1);
    if (!pending_.contains(id)) return;
    const auto pending = pending_.take(id);
    const bool success = response.value(QStringLiteral("ok")).toBool(false);
    if (!success) {
        QString error = response.value(QStringLiteral("error")).toString();
        if (error.isEmpty()) error = QStringLiteral("The engine rejected the request.");
        setError(error);
        adoptState(response); // Show engine truth, retain unapplied editor contents.
    } else {
        bool replaceDraft = pending.command == QStringLiteral("load_profile");
        if (pending.command == QStringLiteral("apply_config")) {
            // Do not discard an edit made while a request was in flight.
            replaceDraft = draft_ == pending.submittedConfig;
            rejected_ = false;
            errorMessage_.clear();
            notice_ = QStringLiteral("Settings applied by the engine.");
        } else if (pending.command == QStringLiteral("save_profile")) {
            rejected_ = false;
            errorMessage_.clear();
            notice_ = QStringLiteral("Profile saved atomically by the engine.");
        } else if (pending.command == QStringLiteral("load_profile")) {
            rejected_ = false;
            errorMessage_.clear();
            notice_ = QStringLiteral("Profile loaded and applied by the engine.");
        } else if (pending.command == QStringLiteral("export_diagnostics")) {
            notice_ = QStringLiteral("Diagnostics exported: ") + response.value(QStringLiteral("path")).toString();
            errorMessage_.clear();
        } else if (pending.command == QStringLiteral("restart_device")) {
            notice_ = QStringLiteral("Device restart acknowledged by the engine.");
            errorMessage_.clear();
        } else if (pending.command == QStringLiteral("reset_output")) {
            notice_ = QStringLiteral("OSC output reset by the engine.");
            errorMessage_.clear();
        }
        adoptState(response, replaceDraft);
        if (pending.command == QStringLiteral("apply_config")) {
            draftRevision_ = response.value(QStringLiteral("revision")).toInteger(revision_);
            if (draftRevision_ < revision_)
                notice_ = QStringLiteral("Settings were applied, then a newer engine revision arrived. Revert to refresh before editing further.");
        }
    }
    emit busyChanged();
    emit uiChanged();
}
void Controller::checkTimeouts() {
    QStringList expired;
    for (auto it = pending_.begin(); it != pending_.end();) {
        if (clock_.elapsed() - it->startedMs > 5000) {
            expired.push_back(it->command);
            it = pending_.erase(it);
        } else ++it;
    }
    if (!expired.isEmpty()) {
        setError(QStringLiteral("Engine acknowledgement timed out. Applied state is unknown; reconnecting to refresh it."));
        socket_.abort();
        emit busyChanged();
    }
}

void Controller::setSetting(const QString& path, const QVariant& value) {
    for (const auto& pending : pending_) {
        if (pending.command == QStringLiteral("load_profile")) {
            setError(QStringLiteral("Wait for the profile to finish loading before editing settings."));
            return;
        }
    }
    if (config_.isEmpty()) { setError(QStringLiteral("Connect to an engine before editing its settings.")); return; }
    auto parts = path.split(QLatin1Char('.'));
    // Device serials are opaque identifiers and may contain dots. Only the
    // final known setting name is a path component beneath devices.
    if (parts.size() >= 3 && parts.first() == QStringLiteral("devices") &&
        (parts.last() == QStringLiteral("alias") || parts.last() == QStringLiteral("enabled") ||
         parts.last() == QStringLiteral("hand_filter"))) {
        const auto field = parts.last();
        const auto serial = parts.mid(1, parts.size() - 2).join(QLatin1Char('.'));
        parts = {QStringLiteral("devices"), serial, field};
    }
    if (parts.isEmpty() || parts.contains(QString())) { setError(QStringLiteral("Unknown setting path.")); return; }
    bool valid = false;
    const auto next = replacePath(draft_, parts, 0, QJsonValue::fromVariant(value), valid).toObject();
    if (!valid) { setError(QStringLiteral("Unknown setting: ") + path); return; }
    if (next == draft_) return;
    const bool wasDirty = dirty();
    if (!wasDirty) draftRevision_ = revision_;
    draft_ = next;
    rejected_ = false;
    errorMessage_.clear();
    notice_.clear();
    emit draftChanged();
    if (wasDirty != dirty()) emit dirtyChanged();
    emit uiChanged();
}
void Controller::apply() {
    if (!connected_ || busy() || !dirty()) return;
    errorMessage_.clear();
    rejected_ = false;
    request(QStringLiteral("apply_config"), {{QStringLiteral("expected_revision"), draftRevision_}, {QStringLiteral("config"), draft_}});
}
void Controller::revert() {
    const bool wasDirty = dirty();
    draft_ = config_;
    draftRevision_ = revision_;
    rejected_ = false;
    errorMessage_.clear();
    notice_ = QStringLiteral("Editor restored to the current engine settings.");
    emit draftChanged();
    if (wasDirty != dirty()) emit dirtyChanged();
    emit uiChanged();
}
void Controller::startSimulator() { startEngine(true); }
void Controller::startHardwareEngine() { startEngine(false); }
void Controller::startEngine(bool simulate) {
    if (connected_ || busy()) return;
    startup_ = Startup::AttachOnly;
    if (connecting_) { socket_.abort(); connecting_ = false; }
    const auto executable = QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
        QStringLiteral("handbridge-engine.exe")
#else
        QStringLiteral("handbridge-engine")
#endif
    );
    if (!QFileInfo::exists(executable)) {
        setError(QStringLiteral("Cannot find handbridge-engine beside the controller. Build both native targets."));
        return;
    }
    if (simulate) {
        launchDetachedEngine(executable, true);
        return;
    }
    // Checking the installed runtime is asynchronous and does not open a LeapC
    // connection. The actual tracking engine is detached only after this succeeds.
    auto* probe = new QProcess(this);
    launchProbe_ = probe;
    probe->setProgram(executable);
    probe->setArguments({QStringLiteral("--check-leapc")});
    probe->setWorkingDirectory(QCoreApplication::applicationDirPath());
    errorMessage_.clear();
    rejected_ = false;
    notice_ = QStringLiteral("Checking the installed Ultraleap runtime before starting tracking…");
    emit busyChanged();
    emit uiChanged();
    connect(probe, &QProcess::errorOccurred, this, [this, probe](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || launchProbe_ != probe) return;
        launchProbe_ = nullptr;
        notice_.clear();
        setError(QStringLiteral("Cannot run the bundled engine's Ultraleap check: ") + probe->errorString());
        emit busyChanged();
        probe->deleteLater();
    });
    connect(probe, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
        [this, probe, executable](int exitCode, QProcess::ExitStatus exitStatus) {
            if (launchProbe_ != probe) return;
            launchProbe_ = nullptr;
            emit busyChanged();
            const auto report = QJsonDocument::fromJson(probe->readAllStandardOutput()).object();
            const auto details = QString::fromUtf8(probe->readAllStandardError()).simplified().left(768);
            const bool timedOut = probe->property("handbridge_probe_timed_out").toBool();
            probe->deleteLater();
            if (connected_) {
                notice_ = QStringLiteral("Connected to the running engine.");
                emit uiChanged();
                return;
            }
            if (exitStatus != QProcess::NormalExit || exitCode != 0 || report.value(QStringLiteral("provider")).toString() != QStringLiteral("leapc")) {
                notice_.clear();
                setError(timedOut ? QStringLiteral("The Ultraleap runtime check timed out. Check the installed Hyperion runtime.") :
                    QStringLiteral("Ultraleap runtime unavailable. ") + (details.isEmpty() ?
                        QStringLiteral("Install Hyperion and use a build with LeapC support.") : details));
                return;
            }
            launchDetachedEngine(executable, false);
        });
    QTimer::singleShot(5000, probe, [this, probe] {
        if (launchProbe_ == probe && probe->state() != QProcess::NotRunning) {
            probe->setProperty("handbridge_probe_timed_out", true);
            probe->kill();
        }
    });
    probe->start();
}
void Controller::launchDetachedEngine(const QString& executable, bool simulate) {
    if (connected_) return;
    qint64 pid = 0;
    QProcess process;
    process.setProgram(executable);
    QStringList arguments{QStringLiteral("--endpoint"), endpoint_};
    if (simulate) arguments.prepend(QStringLiteral("--simulate"));
    process.setArguments(arguments);
    process.setWorkingDirectory(QCoreApplication::applicationDirPath());
#ifdef _WIN32
    // startDetached deliberately leaves no owned process to terminate when this
    // controller closes. Hide the console without making tracking a GUI thread.
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* arguments) {
        arguments->flags |= CREATE_NO_WINDOW;
        arguments->flags &= ~static_cast<DWORD>(CREATE_NEW_CONSOLE);
    });
#endif
    if (!process.startDetached(&pid)) {
        setError(simulate ? QStringLiteral("The simulated engine could not be started.") :
            QStringLiteral("The LeapC tracking engine could not be started."));
        return;
    }
    rejected_ = false;
    errorMessage_.clear();
    notice_ = simulate ? QStringLiteral("Starting a simulated engine. It keeps running when this window closes.") :
        QStringLiteral("Starting the LeapC engine. Keep Hyperion running; this window can close without stopping tracking.");
    QTimer::singleShot(300, this, &Controller::connectToEngine);
    emit uiChanged();
}
void Controller::saveProfile(const QString& name) {
    if (!connected_ || busy()) return;
    if (dirty()) { setError(QStringLiteral("Apply or revert your edits before saving a profile.")); return; }
    request(QStringLiteral("save_profile"), {{QStringLiteral("name"), name.trimmed()}, {QStringLiteral("expected_revision"), revision_}});
}
void Controller::loadProfile(const QString& name) {
    if (!connected_ || busy()) return;
    if (dirty()) { setError(QStringLiteral("Apply or revert your edits before loading a profile.")); return; }
    request(QStringLiteral("load_profile"), {{QStringLiteral("name"), name.trimmed()}, {QStringLiteral("expected_revision"), revision_}});
}
void Controller::restartDevice(const QString& serial) {
    if (connected_ && !busy()) request(QStringLiteral("restart_device"), {{QStringLiteral("serial"), serial}});
}
void Controller::resetOutput() {
    if (connected_ && !busy()) request(QStringLiteral("reset_output"));
}
void Controller::exportDiagnostics() {
    if (connected_ && !busy()) request(QStringLiteral("export_diagnostics"));
}
