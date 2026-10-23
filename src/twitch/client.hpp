#pragma once
#include "security/credentials.hpp"
#include <QDateTime>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QSet>
#include <QTimer>
#include <functional>

namespace tca {
struct Category { QString id, name, art; };
struct Response { int code = 0; QJsonObject object; bool validJson = false; bool networkError = false; };
class Client : public QObject {
    Q_OBJECT
public:
    explicit Client(QObject *parent = nullptr, QNetworkAccessManager *transport = nullptr, bool persist = true);
    ~Client() override;
    void restore(const QString &clientId);
    void login(const QString &clientId);
    void cancelLogin();
    void logout();
    void shutdown();
    void search(const QString &query, const QString &cursor = {});
    void readCurrent();
    void apply(const Category &category);
    bool connected() const { return ready; }
    bool busy() const { return updating; }
    QString userName() const { return loginName; }
    Category current() const { return currentCategory; }
    static bool parseCategory(const QJsonObject &object, Category &category);
signals:
    void accountChanged();
    void authorization(const QUrl &url, const QString &code, int expires);
    void authorizationFinished();
    void results(const QVector<tca::Category> &categories, const QString &cursor);
    void searchFinished();
    void currentChanged();
    void status(const QString &message);
    void activity(const QString &message);
private:
    using Callback = std::function<void(const Response &)>;
    void request(const QByteArray &method, const QUrl &url, const QByteArray &body, bool authenticated, Callback callback);
    void resetSession();
    void pollDevice();
    void validate(bool allowRefresh = true);
    void refresh();
    bool acceptTokens(const Response &response);
    void confirm(Category target, quint64 operation, int attempt);
    void finishUpdate(const QString &message);
    void loadCurrentArt();
    QString failure(const Response &response) const;
    QUrl channelUrl() const;
    QNetworkAccessManager *network;
    QSet<QNetworkReply *> requests;
    QTimer pollTimer, validationTimer;
    QString clientId, userId, loginName, deviceCode;
    Tokens tokens;
    Category currentCategory;
    QDateTime deviceDeadline;
    qint64 helixBlockedUntil = 0;
    quint64 generation = 0, searchGeneration = 0, updateGeneration = 0;
    int pollInterval = 5;
    bool ready = false, updating = false, refreshing = false, persistTokens;
};
}
Q_DECLARE_METATYPE(tca::Category)
Q_DECLARE_METATYPE(QVector<tca::Category>)
