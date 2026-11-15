#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariant>

// Presentation state only. Tracking, validation, profiles and OSC belong to the
// separate engine. All methods run on the GUI thread and use asynchronous IPC.
class Controller final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectionChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    Q_PROPERTY(QVariantMap config READ config NOTIFY configChanged)
    Q_PROPERTY(QVariantMap draft READ draft NOTIFY draftChanged)
    Q_PROPERTY(QVariantMap status READ status NOTIFY statusChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY statusChanged)
    Q_PROPERTY(QStringList logEntries READ logEntries NOTIFY logsChanged)
    Q_PROPERTY(QStringList profileNames READ profileNames NOTIFY profilesChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY uiChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY uiChanged)
    Q_PROPERTY(QString notice READ notice NOTIFY uiChanged)
    Q_PROPERTY(qint64 revision READ revision NOTIFY configChanged)
    Q_PROPERTY(qint64 savedRevision READ savedRevision NOTIFY uiChanged)
    Q_PROPERTY(QString endpoint READ endpoint CONSTANT)
public:
    explicit Controller(QString endpoint, QObject* parent = nullptr);
    ~Controller() override; // Never owns or stops the engine process.

    bool connected() const { return connected_; }
    bool busy() const;
    bool dirty() const { return draft_ != config_; }
    QVariantMap config() const { return config_.toVariantMap(); }
    QVariantMap draft() const { return draft_.toVariantMap(); }
    QVariantMap status() const { return status_.toVariantMap(); }
    QVariantList devices() const { return status_.value(QStringLiteral("devices")).toArray().toVariantList(); }
    QStringList logEntries() const { return logs_; }
    QStringList profileNames() const { return profiles_; }
    QString statusText() const;
    QString errorMessage() const { return errorMessage_; }
    QString notice() const { return notice_; }
    qint64 revision() const { return revision_; }
    qint64 savedRevision() const { return savedRevision_; }
    QString endpoint() const { return endpoint_; }

    Q_INVOKABLE void setSetting(const QString& path, const QVariant& value);
    Q_INVOKABLE void apply();
    Q_INVOKABLE void revert();
    Q_INVOKABLE void startSimulator();
    Q_INVOKABLE void connectToEngine();
    Q_INVOKABLE void saveProfile(const QString& name);
    Q_INVOKABLE void loadProfile(const QString& name);
    Q_INVOKABLE void restartDevice(const QString& serial);
    Q_INVOKABLE void resetOutput();
    Q_INVOKABLE void exportDiagnostics();

signals:
    void connectionChanged();
    void busyChanged();
    void configChanged();
    void draftChanged();
    void dirtyChanged();
    void statusChanged();
    void logsChanged();
    void profilesChanged();
    void uiChanged();

private:
    struct Pending {
        QString command;
        qint64 startedMs = 0;
        QJsonObject submittedConfig;
    };
    void receive();
    void message(const QJsonObject& message);
    void adoptState(const QJsonObject& message, bool replaceDraft = false);
    bool request(const QString& command, QJsonObject fields = {});
    void disconnected();
    void setError(const QString& message);
    void checkTimeouts();
    static bool isMutation(const QString& command);

    QString endpoint_;
    QLocalSocket socket_;
    QByteArray received_;
    QTimer reconnectTimer_, timeoutTimer_;
    QElapsedTimer clock_;
    QHash<qint64, Pending> pending_;
    qint64 nextId_ = 1, revision_ = 0, savedRevision_ = 0, draftRevision_ = 0;
    bool connected_ = false, connecting_ = false, rejected_ = false;
    QJsonObject config_, draft_, status_;
    QStringList logs_, profiles_;
    QString errorMessage_, notice_;
};
