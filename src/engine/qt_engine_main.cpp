#include "ipc/Endpoint.hpp"
#include "ipc/Framing.hpp"
#include "ConfigurationStore.hpp"
#include "SnapshotMailbox.hpp"
#include "runtime/ControlSettings.hpp"
#include "runtime/EngineRuntime.hpp"
#include "tracking/TrackingSourceFactory.hpp"
#include "pipeline/04_OscSender.hpp"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QHash>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <deque>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

using namespace handbridge;
namespace {
volatile std::sig_atomic_t interrupted = 0;
void interrupt(int) { interrupted = 1; }
QJsonObject object(const nlohmann::json& value) {
    return QJsonDocument::fromJson(QByteArray::fromStdString(value.dump())).object();
}
nlohmann::json native(const QJsonObject& value) {
    return nlohmann::json::parse(QJsonDocument(value).toJson(QJsonDocument::Compact).toStdString());
}
bool integer(const QJsonValue& value) {
    return value.isDouble() && value.toDouble() >= 0 && value.toDouble() <= 9007199254740991.0 &&
        value.toDouble() == static_cast<double>(value.toInteger(-1));
}
quint64 unsignedArgument(const QString& input, const char* name) {
    bool valid = false;
    const auto value = input.toULongLong(&valid);
    if (!valid || input.isEmpty() || !QRegularExpression(QStringLiteral("^[0-9]+$")).match(input).hasMatch())
        throw std::invalid_argument(std::string("Invalid unsigned integer for ") + name);
    return value;
}
QString privateDirectory(const QString& requested) {
    const auto path = requested.isEmpty() ?
        QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)).filePath("HandBridge") :
        QDir(requested).absolutePath();
    const QFileInfo before(path);
    if (before.isSymLink()) throw std::runtime_error("Engine state directory must not be a symbolic link");
    if (!QDir().mkpath(path)) throw std::runtime_error("Cannot create private engine state directory");
#ifndef _WIN32
    const auto shared = QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup |
        QFile::ReadOther | QFile::WriteOther | QFile::ExeOther;
    if (before.exists() && (before.permissions() & shared))
        throw std::runtime_error("Existing state directory must be private to the current user");
    if (!before.exists() && !QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner))
        throw std::runtime_error("Cannot restrict engine state directory permissions");
#endif
    return path;
}
ControlSettings readSettings(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > handbridge::ipc::MaximumPayload)
        throw std::runtime_error("Cannot read configuration (missing, inaccessible or oversized)");
    const auto document = nlohmann::json::parse(file.readAll().toStdString());
    if (document.contains("schema_version")) return ControlSettings::fromJson(document);
    ConfigManager legacy;
    if (!legacy.loadConfig(path.toStdString())) throw std::runtime_error("Invalid legacy configuration");
    return ControlSettings::fromLegacy(legacy);
}
QJsonArray vector(const Vector3& value) { return {value.x, value.y, value.z}; }
QJsonValue preview(const FrameData& frame) {
    if (frame.hands.empty()) return QJsonValue::Null;
    const auto& hand = frame.hands.front();
    QJsonArray tips;
    for (const auto& finger : hand.fingers) {
        if (finger.isValid() && finger.bones.size() > 3 && finger.bones[3].isValid())
            tips.push_back(vector(finger.bones[3].nextJoint));
    }
    return QJsonObject{{"palm", vector(hand.palm.position)},
        {"wrist", hand.arm.isValid() ? QJsonValue(vector(hand.arm.wristPosition)) : QJsonValue(QJsonValue::Null)},
        {"tips", tips}, {"hand", QString::fromStdString(hand.handType)},
        {"pinch", hand.pinchStrength}, {"grab", hand.grabStrength}};
}
struct Snapshot {
    QJsonObject config, state;
    QJsonArray logs;
    quint64 revision = 1;
};
struct Job {
    enum Kind { Apply, RestartDevice, ResetOutput } kind = Apply;
    ControlSettings settings;
    quint64 expectedRevision = 0;
    std::string serial;
    std::function<void(QJsonObject)> done;
};

// Only this worker owns the source, processor and UDP sender. GUI IPC and file
// I/O never run on it. Telemetry copies are bounded immutable shared snapshots.
class EngineWorker {
public:
    EngineWorker(ControlSettings settings, bool simulate, bool stall, std::string leapcLibrary, QObject* callbackContext)
        : context_(callbackContext) {
        std::promise<void> ready;
        auto readyFuture = ready.get_future();
        thread_ = std::thread([this, settings = std::move(settings), simulate, stall,
                                leapcLibrary = std::move(leapcLibrary), ready = std::move(ready)]() mutable {
            try {
                ConfigManager config;
                auto source = createTrackingSource(simulate, stall, leapcLibrary);
                if (!source) throw std::runtime_error("Tracking provider could not be created");
                auto sender = std::make_unique<OscSender>(settings.osc.host, settings.osc.port);
                if (!sender->isInitialized()) throw std::runtime_error("Cannot initialize OSC socket");
                std::deque<QString> logs;
                auto logger = [&](const std::string& text) {
                    const auto line = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) + " " + QString::fromStdString(text).left(256);
                    logs.push_back(line);
                    if (logs.size() > 128) logs.pop_front();
                    std::cerr << text << '\n';
                };
                EngineRuntime runtime(*source, config,
                    [&](const OscMessage& message) { return sender->sendOscMessageChecked(message); }, {}, logger);
                runtime.applySettings(settings);
                runtime.start(0);
                const auto started = std::chrono::steady_clock::now();
                quint64 revision = 1;
                Millis now = 0, lastSnapshot = 0;
                std::uint64_t previousPackets = 0, packetOffset = 0, byteOffset = 0, errorOffset = 0, socketGeneration = 1;
                QString retainedError;
                std::string lastTrackingError;
                std::map<std::string, std::uint64_t> previousFrames;
                auto replaceSender = [&](std::unique_ptr<OscSender>& candidate) {
                    const auto old = sender->getStats();
                    packetOffset += old.packetsSent; byteOffset += old.bytesSent; errorOffset += old.sendErrors;
                    if (!old.lastError.empty()) retainedError = QString::fromStdString(old.lastError);
                    sender.swap(candidate); ++socketGeneration;
                };
                auto publish = [&]() {
                    auto next = std::make_shared<Snapshot>();
                    next->config = object(runtime.settings().toJson());
                    const auto before = snapshot_.load();
                    // Discovery is a real canonical configuration mutation too.
                    if (before && before->revision == revision && before->config != next->config) ++revision;
                    next->revision = revision;
                    const auto span = now > lastSnapshot ? now - lastSnapshot : 0;
                    QJsonArray devices;
                    for (const auto& [serial, status] : runtime.devices()) {
                        QJsonObject device{{"serial", QString::fromStdString(serial)}, {"alias", QString::fromStdString(status.alias)},
                            {"state", stateName(status.state)}, {"present", status.present}, {"subscribed", status.subscribed},
                            {"frames_fresh", status.framesFresh}, {"hands_visible", status.handsVisible},
                            {"frames_in", static_cast<qint64>(status.framesIn)}, {"frames_processed", static_cast<qint64>(status.framesProcessed)},
                            {"queue_drops", static_cast<qint64>(status.queueDrops)}, {"reconnect_attempts", static_cast<qint64>(status.reconnectAttempts)},
                            {"reason", QString::fromStdString(status.reason)},
                            {"input_fps", span ? double(status.framesIn - previousFrames[serial]) * 1000.0 / double(span) : 0.0},
                            {"last_frame_age_ms", status.lastFrameMs ? QJsonValue(static_cast<qint64>(now - *status.lastFrameMs)) : QJsonValue(QJsonValue::Null)},
                            {"raw", QJsonValue::Null}, {"processed", QJsonValue::Null}};
                        previousFrames[serial] = status.framesIn;
                        const auto found = runtime.previews().find(serial);
                        if (status.framesFresh && found != runtime.previews().end()) {
                            device["raw"] = preview(found->second.raw);
                            device["processed"] = preview(found->second.processed);
                        }
                        devices.push_back(device);
                    }
                    const auto stats = sender->getStats();
                    const auto packets = packetOffset + stats.packetsSent;
                    const auto sendErrors = errorOffset + stats.sendErrors;
                    const auto packetDelta = packets >= previousPackets ? packets - previousPackets : 0;
                    const auto& output = runtime.outputStatus();
                    const auto tracking = source->diagnostics();
                    if (tracking.lastError != lastTrackingError) {
                        logger(tracking.lastError.empty() ? "tracking diagnostic cleared" : "tracking error=" + tracking.lastError);
                        lastTrackingError = tracking.lastError;
                    }
                    next->state = {{"source", trackingSourceName(simulate)}, {"simulated", simulate}, {"hardware_verified", false},
                        {"uptime_ms", static_cast<qint64>(now)}, {"service_connected", runtime.serviceConnected()}, {"devices", devices},
                        {"tracking", QJsonObject{{"last_error", QString::fromStdString(tracking.lastError)},
                            {"library_path", QString::fromStdString(tracking.libraryPath)}, {"library_version", QString::fromStdString(tracking.libraryVersion)},
                            {"service_version", QString::fromStdString(tracking.serviceVersion)},
                            {"connection_attempts", static_cast<qint64>(tracking.connectionAttempts)}, {"poll_errors", static_cast<qint64>(tracking.pollErrors)}}},
                        {"output", QJsonObject{{"enabled", runtime.settings().osc.enabled}, {"host", QString::fromStdString(runtime.settings().osc.host)},
                            {"port", runtime.settings().osc.port}, {"target", QString::fromStdString(runtime.settings().osc.host) + ":" + QString::number(runtime.settings().osc.port)},
                            {"socket_ready", sender->isInitialized()}, {"packets_sent", static_cast<qint64>(packets)},
                            {"send_errors", static_cast<qint64>(sendErrors)}, {"bytes_sent", static_cast<qint64>(byteOffset + stats.bytesSent)},
                            // QML numbers lose integer precision above 2^53.
                            {"packets_sent_exact", QString::number(packets)}, {"send_errors_exact", QString::number(sendErrors)},
                            {"socket_generation", static_cast<qint64>(socketGeneration)},
                            {"packet_fps", span ? double(packetDelta) * 1000.0 / double(span) : 0.0}, {"degraded", output.degraded},
                            {"attempts", static_cast<qint64>(output.attempts)}, {"successes", static_cast<qint64>(output.successes)},
                            {"errors", static_cast<qint64>(output.errors)},
                            {"last_error", stats.lastError.empty() ? retainedError : QString::fromStdString(stats.lastError)}}}};
                    for (const auto& line : logs) next->logs.push_back(line);
                    previousPackets = packets;
                    lastSnapshot = now;
                    snapshot_.store(std::move(next));
                };
                // Establish the startup configuration's revision before source
                // discovery can add persistent serial/alias bindings.
                publish();
                runtime.tick(0);
                publish();
                ready.set_value();
                while (!stopping_.load()) {
                    now = static_cast<Millis>(std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - started).count());
                    std::deque<Job> work;
                    {
                        std::lock_guard lock(mutex_);
                        work.swap(jobs_);
                    }
                    for (auto& job : work) {
                        if (const auto previous = snapshot_.load(); previous && previous->config != object(runtime.settings().toJson())) publish();
                        QJsonObject response;
                        try {
                            if (job.kind == Job::Apply) {
                                if (job.expectedRevision != revision) throw std::runtime_error("Configuration changed; refresh before applying");
                                job.settings.validate();
                                std::unique_ptr<OscSender> candidate;
                                if (job.settings.osc.host != runtime.settings().osc.host || job.settings.osc.port != runtime.settings().osc.port) {
                                    candidate = std::make_unique<OscSender>(job.settings.osc.host, job.settings.osc.port);
                                    if (!candidate->isInitialized()) throw std::runtime_error("New OSC socket could not be opened; previous configuration retained");
                                }
                                runtime.applySettings(job.settings);
                                if (candidate) replaceSender(candidate);
                                ++revision;
                                logger("configuration applied revision=" + std::to_string(revision));
                            } else if (job.kind == Job::RestartDevice) {
                                if (!runtime.restartDevice(job.serial, now)) throw std::runtime_error("Device restart was not accepted");
                            } else {
                                auto candidate = std::make_unique<OscSender>(runtime.settings().osc.host, runtime.settings().osc.port);
                                if (!candidate->isInitialized()) throw std::runtime_error("OSC socket reset failed");
                                replaceSender(candidate);
                                logger("output socket reset");
                            }
                            response["ok"] = true;
                        } catch (const std::exception& error) {
                            response = {{"ok", false}, {"error", QString::fromUtf8(error.what()).left(512)}};
                        }
                        // Apply and observe at this frame boundary before acknowledging.
                        runtime.tick(now);
                        publish();
                        const auto current = snapshot_.load();
                        response["revision"] = static_cast<qint64>(current->revision);
                        response["config"] = current->config;
                        response["state"] = current->state;
                        QMetaObject::invokeMethod(context_, [done = std::move(job.done), response]() { done(response); }, Qt::QueuedConnection);
                    }
                    runtime.tick(now);
                    if (now - lastSnapshot >= 100) publish();
                    std::unique_lock lock(mutex_);
                    wake_.wait_for(lock, std::chrono::milliseconds(2), [&]() { return stopping_.load() || !jobs_.empty(); });
                }
                // Keep last live device state for the CLI compatibility summary;
                // shutdown loss messages are still included in output counters.
                const auto live = snapshot_.load();
                runtime.stop();
                auto final = std::make_shared<Snapshot>(*live);
                auto output = final->state["output"].toObject();
                output["errors"] = static_cast<qint64>(runtime.outputStatus().errors);
                output["attempts"] = static_cast<qint64>(runtime.outputStatus().attempts);
                output["successes"] = static_cast<qint64>(runtime.outputStatus().successes);
                final->state["output"] = output;
                snapshot_.store(std::move(final));
            } catch (...) {
                try { ready.set_exception(std::current_exception()); } catch (...) {}
                failed_ = true;
            }
        });
        try {
            readyFuture.get();
        } catch (...) {
            // A throwing constructor does not run ~EngineWorker. Explicitly join
            // before unwinding the owned std::thread, including provider errors.
            stop();
            throw;
        }
    }
    ~EngineWorker() { stop(); }
    bool enqueue(Job job) {
        std::lock_guard lock(mutex_);
        if (jobs_.size() >= 32 || stopping_.load() || failed_) return false;
        jobs_.push_back(std::move(job));
        wake_.notify_one();
        return true;
    }
    std::shared_ptr<const Snapshot> snapshot() const { return snapshot_.load(); }
    bool failed() const { return failed_; }
    void stop() {
        // Called only by the control thread; repeated shutdown is harmless.
        {
            std::lock_guard lock(mutex_);
            stopping_.store(true);
        }
        wake_.notify_one();
        // Never hold the command/snapshot mutex while waiting for the owner.
        if (thread_.joinable()) thread_.join();
    }
private:
    QObject* context_;
    SnapshotMailbox<Snapshot> snapshot_;
    std::atomic<bool> failed_ = false;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Job> jobs_;
    std::atomic<bool> stopping_ = false;
    std::thread thread_;
};

class ControlServer final : public QObject {
public:
    ControlServer(const QString& endpoint, const QString& directory, const ControlSettings& settings,
                  bool simulate, bool stall, std::string leapcLibrary, bool restored, QObject* parent = nullptr)
        : QObject(parent), directory_(directory), endpoint_(endpoint), lock_(QDir(privateDirectory({})).filePath(
            "engine-" + QString::fromLatin1(QCryptographicHash::hash(endpoint.toUtf8(), QCryptographicHash::Sha256).toHex().left(16)) + ".lock")),
          store_(directory, restored ? 1 : 0) {
        lock_.setStaleLockTime(0);
        if (!lock_.tryLock()) throw std::runtime_error("An engine already owns this endpoint");
        server_.setSocketOptions(QLocalServer::UserAccessOption);
        server_.setMaxPendingConnections(8);
        const auto name = handbridge::ipc::socketName(endpoint);
        if (name.isEmpty()) throw std::runtime_error("Cannot open the private control socket directory");
        if (!server_.listen(name)) {
            QLocalSocket probe;
            probe.connectToServer(name);
            if (probe.waitForConnected(150)) throw std::runtime_error("Endpoint is already active");
#ifdef _WIN32
            throw std::runtime_error("Cannot create local-only control endpoint");
#else
            const bool staleEndpoint = server_.serverError() == QAbstractSocket::AddressInUseError &&
                (probe.error() == QLocalSocket::ServerNotFoundError || probe.error() == QLocalSocket::ConnectionRefusedError);
            if (!staleEndpoint || !QLocalServer::removeServer(name) || !server_.listen(name))
                throw std::runtime_error("Cannot create local-only control endpoint");
#endif
        }
        worker_ = std::make_unique<EngineWorker>(settings, simulate, stall, std::move(leapcLibrary), this);
        connect(&server_, &QLocalServer::newConnection, this, [this]() {
            while (server_.hasPendingConnections()) {
                auto* socket = server_.nextPendingConnection();
                if (clients_.size() >= 8) { socket->abort(); socket->deleteLater(); continue; }
                socket->setReadBufferSize(handbridge::ipc::MaximumBuffered);
                clients_.insert(socket, {});
                connect(socket, &QLocalSocket::readyRead, this, [this, socket]() { receive(socket); });
                connect(socket, &QLocalSocket::disconnected, this, [this, socket]() { clients_.remove(socket); socket->deleteLater(); });
                transmit(socket, stateMessage(true));
                if (socket->bytesAvailable() > 0) receive(socket);
            }
        });
        telemetry_.setInterval(100);
        connect(&telemetry_, &QTimer::timeout, this, [this]() {
            const auto message = stateMessage(true);
            const auto sockets = clients_.keys();
            for (auto* socket : sockets) transmit(socket, message);
        });
        telemetry_.start();
    }
    ~ControlServer() override { worker_->stop(); server_.close(); }
    void stop() { worker_->stop(); }
    bool failed() const { return worker_->failed(); }
    QJsonObject summary() const {
        const auto current = worker_->snapshot();
        QJsonObject devices;
        for (const auto& item : current->state["devices"].toArray()) {
            auto device = item.toObject();
            devices[device.take("serial").toString()] = device;
        }
        const auto osc = current->config["osc"].toObject();
        return {{"source", current->state["source"]}, {"simulated", current->state["simulated"]}, {"hardware_verified", false},
            {"service_connected", current->state["service_connected"]}, {"destination", osc["host"]}, {"port", osc["port"]},
            {"devices", devices}, {"tracking", current->state["tracking"]}, {"output", current->state["output"]}};
    }
private:
    QJsonArray profiles() const {
        QJsonArray result;
        const auto files = QDir(QDir(directory_).filePath("profiles")).entryInfoList({"*.json"}, QDir::Files | QDir::NoSymLinks, QDir::Name);
        for (const auto& file : files) result.push_back(file.completeBaseName());
        return result;
    }
    QJsonObject stateMessage(bool telemetry = false) const {
        const auto current = worker_->snapshot();
        QJsonObject result{{"version", 1}, {"revision", static_cast<qint64>(current->revision)},
            {"saved_revision", static_cast<qint64>(store_.savedRevision())}, {"config", current->config},
            {"state", current->state}, {"logs", current->logs}, {"profiles", profiles()}};
        if (telemetry) result["event"] = "state";
        return result;
    }
    void transmit(QLocalSocket* socket, const QJsonObject& object) {
        if (!clients_.contains(socket)) return;
        const auto bytes = handbridge::ipc::encode(object);
        if (bytes.isEmpty() || socket->bytesToWrite() + bytes.size() > handbridge::ipc::MaximumBuffered) {
            socket->abort(); return;
        }
        if (socket->write(bytes) < 0) socket->abort();
    }
    void reply(QLocalSocket* socket, const QJsonValue& id, QJsonObject response) {
        if (!clients_.contains(socket)) return;
        auto complete = stateMessage();
        for (auto it = response.begin(); it != response.end(); ++it) complete[it.key()] = it.value();
        complete["id"] = id;
        transmit(socket, complete);
    }
    QString profilePath(const QString& name) const {
        if (!QRegularExpression(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9 _-]{0,63}$")).match(name).hasMatch())
            throw std::invalid_argument("Profile names must use 1-64 letters, numbers, spaces, underscores or hyphens");
        const auto directory = QDir(directory_).filePath("profiles");
        if (!QDir().mkpath(directory)) throw std::runtime_error("Cannot create profile directory");
        return QDir(directory).filePath(name + ".json");
    }
    void dispatch(QLocalSocket* socket, const QJsonObject& request) {
        const auto id = request["id"];
        try {
            if (!integer(id) || request["version"] != 1 || !request["command"].isString())
                throw std::invalid_argument("Expected version1, nonnegative integer id and command");
            const auto command = request["command"].toString();
            if (command == "get_state" || command == "list_profiles") {
                reply(socket, id, {{"ok", true}}); return;
            }
            if (command == "save_profile") {
                const auto current = worker_->snapshot();
                if (request.contains("expected_revision") &&
                    (!integer(request["expected_revision"]) || request["expected_revision"].toInteger() != static_cast<qint64>(current->revision)))
                    throw std::runtime_error("Configuration changed; refresh before saving");
                const auto path = profilePath(request["name"].toString());
                handbridge::engine::ConfigurationStore::write(path, current->config);
                store_.saveCurrent(current->config, current->revision);
                reply(socket, id, {{"ok", true}, {"name", request["name"]}}); return;
            }
            if (command == "export_diagnostics") {
                auto diagnostic = stateMessage();
                auto config = diagnostic["config"].toObject();
                auto deviceSettings = config["devices"].toObject();
                QJsonObject redacted;
                QStringList serials;
                for (auto it = deviceSettings.begin(); it != deviceSettings.end(); ++it) {
                    serials.push_back(it.key());
                    redacted["redacted-" + it.value().toObject()["alias"].toString()] = it.value();
                }
                config["devices"] = redacted;
                diagnostic["config"] = config;
                auto state = diagnostic["state"].toObject();
                auto devices = state["devices"].toArray();
                for (qsizetype index = 0; index < devices.size(); ++index) {
                    auto device = devices[index].toObject(); device["serial"] = "<redacted>"; devices[index] = device;
                }
                state["devices"] = devices;
                auto tracking = state["tracking"].toObject();
                if (!tracking.value("library_path").toString().isEmpty()) tracking["library_path"] = "<redacted>";
                state["tracking"] = tracking;
                diagnostic["state"] = state;
                auto bytes = QJsonDocument(diagnostic).toJson(QJsonDocument::Indented);
                for (const auto& serial : serials) bytes.replace(serial.toUtf8(), "<redacted>");
                const auto folder = QDir(directory_).filePath("diagnostics");
                if (!QDir().mkpath(folder)) throw std::runtime_error("Cannot create diagnostic directory");
                const auto path = QDir(folder).filePath("handbridge-" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz") + ".json");
                QSaveFile file(path);
                if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
                    throw std::runtime_error("Cannot save diagnostic export");
                reply(socket, id, {{"ok", true}, {"path", path}}); return;
            }
            Job job;
            if (command == "apply_config" || command == "load_profile") {
                if (!integer(request["expected_revision"])) throw std::invalid_argument("expected_revision is required");
                job.expectedRevision = request["expected_revision"].toInteger();
                job.settings = command == "apply_config" ? ControlSettings::fromJson(native(request["config"].toObject())) :
                    readSettings(profilePath(request["name"].toString()));
            } else if (command == "restart_device") {
                job.kind = Job::RestartDevice;
                job.serial = request["serial"].toString().toStdString();
            } else if (command == "reset_output") job.kind = Job::ResetOutput;
            else throw std::invalid_argument("Unknown control command");
            const QPointer<QLocalSocket> guarded(socket);
            const bool loading = command == "load_profile";
            job.done = [this, guarded, id, loading](QJsonObject response) {
                const auto appliedRevision = static_cast<quint64>(response["revision"].toInteger());
                // Worker completion is queued onto this thread. Another client
                // may already have saved a later revision before this older
                // load callback runs; never roll startup restore backwards.
                if (loading && response["ok"].toBool()) {
                    try {
                        store_.saveCurrent(response["config"].toObject(), appliedRevision);
                    } catch (const std::exception& error) {
                        response["ok"] = false;
                        response["error"] = QString("Settings applied, but startup restore could not be saved: ") + error.what();
                    }
                }
                if (guarded) reply(guarded, id, response);
            };
            if (!worker_->enqueue(std::move(job))) throw std::runtime_error("Engine command queue is busy; retry later");
        } catch (const std::exception& error) {
            reply(socket, id, {{"ok", false}, {"error", QString::fromUtf8(error.what()).left(512)}});
        }
    }
    void receive(QLocalSocket* socket) {
        if (!clients_.contains(socket)) return;
        auto& buffer = clients_[socket];
        buffer += socket->readAll();
        QList<QJsonObject> messages;
        QString error;
        if (!handbridge::ipc::takeFrames(buffer, messages, error)) { socket->abort(); return; }
        for (const auto& message : messages) {
            if (!clients_.contains(socket)) break;
            dispatch(socket, message);
        }
        // Yield the IPC event loop between bounded batches. Tracking is on its
        // own worker even when a local client continually submits commands.
        if (clients_.contains(socket) && clients_[socket].size() >= 4 &&
            clients_[socket].size() >= 4 + static_cast<qsizetype>(qFromBigEndian<quint32>(clients_[socket].constData()))) {
            const QPointer<QLocalSocket> guarded(socket);
            QTimer::singleShot(0, this, [this, guarded]() { if (guarded) receive(guarded); });
        }
    }
    QString directory_, endpoint_;
    QLockFile lock_;
    handbridge::engine::ConfigurationStore store_;
    QLocalServer server_;
    QTimer telemetry_;
    QHash<QLocalSocket*, QByteArray> clients_;
    std::unique_ptr<EngineWorker> worker_;
};
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("HandBridge");
    QCoreApplication::setApplicationVersion("2.0.3");
    try {
        QCommandLineParser parser;
        parser.setApplicationDescription("Hand Bridge native headless engine. LeapC tracking by default; --simulate selects mock cameras. UNTESTED ON HARDWARE.");
        parser.addHelpOption(); parser.addVersionOption();
        parser.addOptions({QCommandLineOption(QStringLiteral("simulate"), "Use deterministic simulated cameras instead of LeapC."),
            QCommandLineOption(QStringLiteral("headless"), "Run without a GUI (always true)."),
            QCommandLineOption(QStringLiteral("config"), "Explicit legacy or schema2 configuration.", "path"),
            QCommandLineOption(QStringLiteral("host"), "Numeric IPv4 OSC destination.", "host"),
            QCommandLineOption(QStringLiteral("port"), "UDP port1-65535.", "port"),
            QCommandLineOption(QStringLiteral("duration-ms"), "Exit after N milliseconds;0 means until Ctrl-C.", "milliseconds", "0"),
            QCommandLineOption(QStringLiteral("scenario"), "Simulation normal or stall.", "scenario", "normal"),
            QCommandLineOption(QStringLiteral("endpoint"), "Local control endpoint token.", "name", handbridge::ipc::defaultEndpoint()),
            QCommandLineOption(QStringLiteral("state-dir"), "Private profile/configuration directory.", "path"),
            QCommandLineOption(QStringLiteral("leapc-library"), "Absolute path to the installed LeapC runtime library.", "path"),
            QCommandLineOption(QStringLiteral("check-leapc"), "Load and verify the LeapC runtime without starting tracking or writing state.")});
        parser.process(app);
        const bool simulate = parser.isSet("simulate");
        if (!parser.positionalArguments().isEmpty()) throw std::invalid_argument("Unexpected positional argument");
        const auto scenario = parser.value("scenario");
        if (scenario != "normal" && scenario != "stall") throw std::invalid_argument("Unknown simulation scenario");
        if (!simulate && scenario != "normal") throw std::invalid_argument("--scenario stall requires --simulate");
        const auto leapcLibrary = parser.value("leapc-library");
        if (!leapcLibrary.isEmpty() && (!QDir::isAbsolutePath(leapcLibrary) || simulate))
            throw std::invalid_argument("--leapc-library requires an absolute installed-library path and real tracking mode");
        if (parser.isSet("check-leapc")) {
            if (simulate) throw std::invalid_argument("--check-leapc cannot be combined with --simulate");
            const auto probe = probeLeapC(leapcLibrary.toStdString());
            const QJsonObject report{{"provider", "leapc"}, {"library", QString::fromStdString(probe.path)},
                {"version", QString::fromStdString(probe.version)}, {"client_api_version", QString::fromStdString(probe.apiVersion)}, {"hardware_verified", false}};
            std::cout << QJsonDocument(report).toJson(QJsonDocument::Compact).toStdString() << '\n';
            return 0;
        }
        const auto endpoint = parser.value("endpoint");
        if (!QRegularExpression(QStringLiteral("^[A-Za-z0-9_.-]{1,80}$")).match(endpoint).hasMatch())
            throw std::invalid_argument("Endpoint must use1-80 letters, numbers, dot, underscore or hyphen");
        const auto duration = unsignedArgument(parser.value("duration-ms"), "duration-ms");
        const auto directory = privateDirectory(parser.value("state-dir"));
        ControlSettings settings;
        bool restored = false;
        const auto current = QDir(directory).filePath("current.json");
        if (parser.isSet("config")) settings = readSettings(parser.value("config"));
        else if (QFile::exists(current)) {
            try { settings = readSettings(current); }
            catch (...) {
                if (!QFile::exists(current + ".bak")) throw;
                settings = readSettings(current + ".bak");
                std::cerr << "Current configuration invalid; restored previous valid backup\n";
            }
            restored = true;
        }
        if (parser.isSet("host")) { settings.osc.host = parser.value("host").toStdString(); restored = false; }
        if (parser.isSet("port")) {
            const auto value = unsignedArgument(parser.value("port"), "port");
            if (value < 1 || value > 65535) throw std::invalid_argument("Port must be1-65535");
            settings.osc.port = static_cast<int>(value); restored = false;
        }
        settings.validate();
        ControlServer server(endpoint, directory, settings, simulate, scenario == "stall", leapcLibrary.toStdString(), restored);
        std::cerr << trackingSourceName(simulate) << " tracking; UNTESTED ON HARDWARE; local control endpoint=" << endpoint.toStdString()
                  << "; UDP send success is not receiver acknowledgement\n";
        std::signal(SIGINT, interrupt); std::signal(SIGTERM, interrupt);
        const auto started = std::chrono::steady_clock::now();
        QTimer shutdown;
        shutdown.setInterval(10);
        QObject::connect(&shutdown, &QTimer::timeout, &app, [&]() {
            const auto elapsed = static_cast<quint64>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started).count());
            if (interrupted || server.failed() || (duration != 0 && elapsed >= duration)) app.quit();
        });
        shutdown.start();
        const int result = app.exec();
        server.stop();
        const auto summary = server.summary();
        std::cout << QJsonDocument(summary).toJson(QJsonDocument::Compact).toStdString() << '\n';
        return server.failed() ? 1 : summary["output"].toObject()["errors"].toInteger() ? 2 : result;
    } catch (const std::exception& error) {
        std::cerr << "handbridge-engine: " << error.what() << '\n'; return 1;
    }
}
