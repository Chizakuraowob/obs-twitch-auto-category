#include "client.hpp"
#include "settings/settings.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QSslSocket>
#include <algorithm>

namespace tca {
static QByteArray form(std::initializer_list<QPair<QString, QString>> pairs)
{
    QUrlQuery query;
    for (const auto &pair : pairs) query.addQueryItem(pair.first, pair.second);
    return query.query(QUrl::FullyEncoded).toUtf8().replace("+", "%2B");
}
Client::Client(QObject *parent, QNetworkAccessManager *transport, bool persist)
    : QObject(parent), network(transport ? transport : new QNetworkAccessManager(this)), persistTokens(persist)
{
    pollTimer.setSingleShot(true);
    connect(&pollTimer, &QTimer::timeout, this, &Client::pollDevice);
    validationTimer.setInterval(60 * 60 * 1000);
    connect(&validationTimer, &QTimer::timeout, this, [this] { validate(); });
}
Client::~Client() { shutdown(); }
void Client::shutdown()
{
    ++generation; pollTimer.stop(); validationTimer.stop();
    if (reading) { reading = false; emit currentReadChanged(); }
    const auto pending = requests;
    for (auto *reply : pending) { disconnect(reply, nullptr, this, nullptr); reply->abort(); reply->deleteLater(); }
    requests.clear();
    tokens = {};
}
void Client::resetSession()
{
    shutdown(); ++searchGeneration; ++updateGeneration;
    ready = updating = refreshing = false;
    userId.clear(); loginName.clear(); deviceCode.clear(); currentCategory = {};
    emit accountChanged(); emit currentChanged(); emit searchFinished();
}
void Client::request(const QByteArray &method, const QUrl &url, const QByteArray &body, bool authenticated, Callback callback)
{
    if (authenticated && QDateTime::currentSecsSinceEpoch() < helixBlockedUntil) {
        callback({429, {}, false, false}); return;
    }
    QNetworkRequest req(url);
    req.setTransferTimeout(15000);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    req.setRawHeader("Accept", "application/json");
    if (authenticated) {
        req.setRawHeader("Client-Id", clientId.toUtf8());
        req.setRawHeader("Authorization", "Bearer " + tokens.access.toUtf8());
    } else if (url.path().endsWith("/validate")) req.setRawHeader("Authorization", "OAuth " + tokens.access.toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, authenticated ? "application/json" : "application/x-www-form-urlencoded");
    auto *reply = method == "GET" ? network->get(req) : network->sendCustomRequest(req, method, body);
    requests.insert(reply);
    const auto epoch = generation;
    connect(reply, &QNetworkReply::readyRead, this, [reply] { if (reply->bytesAvailable() > 2 * 1024 * 1024) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, epoch, authenticated, callback] {
        requests.remove(reply); reply->deleteLater();
        if (epoch != generation) return;
        Response response;
        response.code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        response.networkError = reply->error() != QNetworkReply::NoError && (response.code == 0 || response.code < 400);
        response.networkErrorCode = static_cast<int>(reply->error());
        if (response.networkError) response.tlsAvailable = QSslSocket::supportsSsl();
        QJsonParseError parse;
        const auto bytes = reply->readAll();
        const auto document = bytes.size() <= 2 * 1024 * 1024 ? QJsonDocument::fromJson(bytes, &parse) : QJsonDocument{};
        response.validJson = document.isObject() && parse.error == QJsonParseError::NoError;
        response.object = document.object();
        if (authenticated && response.code == 429) {
            bool valid = false;
            const auto reset = reply->rawHeader("Ratelimit-Reset").toLongLong(&valid);
            const auto now = QDateTime::currentSecsSinceEpoch();
            helixBlockedUntil = valid ? std::clamp(reset, now + 1, now + 3600) : now + 60;
        }
        callback(response);
    });
}
QString Client::failure(const Response &r) const
{
    if (r.networkError || r.code == 0) {
        if (!r.tlsAvailable) return "HTTPS 元件載入失敗（TLS）；請重新安裝完整外掛並重啟 OBS";
        if (r.networkErrorCode == QNetworkReply::SslHandshakeFailedError) return "HTTPS 安全連線驗證失敗；請檢查系統時間、憑證或代理設定";
        if (r.networkErrorCode == QNetworkReply::HostNotFoundError) return "找不到 Twitch 伺服器；請檢查網路與 DNS";
        return QString("連線中斷或逾時（網路錯誤 %1）").arg(r.networkErrorCode);
    }
    if (r.code == 401) return "授權失效，正在嘗試更新授權；請稍後重試";
    if (r.code == 403) return "Twitch 拒絕操作，請確認帳號及權限";
    if (r.code == 429) return "Twitch 請求頻率受限，請稍後再試";
    if (r.code >= 500) return "Twitch 服務暫時異常";
    return QString("Twitch 回應異常（HTTP %1）").arg(r.code);
}
void Client::restore(const QString &id)
{
    resetSession(); clientId = id;
    if (persistTokens) tokens = Credentials::read(id);
    if (!tokens.access.isEmpty()) { validationTimer.start(); validate(); }
}
void Client::login(const QString &id)
{
    resetSession(); clientId = id.trimmed();
    if (!QRegularExpression("^[a-zA-Z0-9]{1,128}$").match(clientId).hasMatch()) {
        emit status("請先設定有效的 Twitch Public Client ID", StatusLevel::Error); emit authorizationFinished(); return;
    }
    emit status("正在取得 Twitch 裝置授權碼…", StatusLevel::Progress);
    request("POST", QUrl("https://id.twitch.tv/oauth2/device"),
        form({{"client_id", clientId}, {"scopes", "channel:manage:broadcast"}}), false, [this](const Response &r) {
        const auto o = r.object;
        const QUrl uri(o["verification_uri"].toString());
        const int seconds = o["expires_in"].toInt();
        if (r.code != 200 || r.networkError || !r.validJson || o["device_code"].toString().isEmpty() ||
            o["user_code"].toString().isEmpty() || seconds <= 0 || seconds > 86400 ||
            uri.scheme() != "https" || uri.host() != "www.twitch.tv" || uri.path() != "/activate" ||
            !uri.userInfo().isEmpty() || (uri.port() != -1 && uri.port() != 443)) {
            emit status(failure(r), StatusLevel::Error); emit authorizationFinished(); return;
        }
        deviceCode = o["device_code"].toString();
        pollInterval = std::clamp(o["interval"].toInt(5), 5, 60);
        deviceDeadline = QDateTime::currentDateTimeUtc().addSecs(seconds);
        emit authorization(uri, o["user_code"].toString(), seconds);
        pollTimer.start(pollInterval * 1000);
    });
}
void Client::cancelLogin()
{
    resetSession(); emit authorizationFinished(); emit status("已取消登入");
}
bool Client::acceptTokens(const Response &r)
{
    if (r.code != 200 || r.networkError || !r.validJson) return false;
    Tokens next{r.object["access_token"].toString(), r.object["refresh_token"].toString()};
    if (next.access.isEmpty() || next.refresh.isEmpty() || r.object["token_type"].toString().compare("bearer", Qt::CaseInsensitive) != 0) return false;
    tokens = next;
    if (persistTokens && !Credentials::write(clientId, tokens))
        emit activity("憑證安全儲存失敗；目前連線僅維持於記憶體，重啟後需登入");
    return true;
}
void Client::pollDevice()
{
    if (deviceCode.isEmpty()) return;
    if (QDateTime::currentDateTimeUtc() >= deviceDeadline) {
        deviceCode.clear(); emit authorizationFinished(); emit status("授權碼已到期，請重新登入", StatusLevel::Error); return;
    }
    request("POST", QUrl("https://id.twitch.tv/oauth2/token"), form({{"client_id", clientId},
        {"device_code", deviceCode}, {"scopes", "channel:manage:broadcast"},
        {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"}}), false, [this](const Response &r) {
        if (QDateTime::currentDateTimeUtc() >= deviceDeadline) {
            deviceCode.clear(); emit authorizationFinished(); emit status("授權碼已到期，請重新登入", StatusLevel::Error); return;
        }
        if (acceptTokens(r)) {
            deviceCode.clear(); validationTimer.start(); emit authorizationFinished();
            emit status("已取得授權，正在驗證帳號…", StatusLevel::Progress); validate(false); return;
        }
        const auto error = r.object["message"].toString(r.object["error"].toString());
        if (error == "authorization_pending" || error == "slow_down" || r.code == 429 || r.code >= 500 || r.networkError || !r.code) {
            if (error == "slow_down" || r.code == 429) pollInterval = std::min(60, pollInterval + 5);
            pollTimer.start(pollInterval * 1000); return;
        }
        deviceCode.clear(); emit authorizationFinished(); emit status("授權未完成或已被拒絕，請重新登入", StatusLevel::Error);
    });
}
void Client::validate(bool allowRefresh)
{
    if (tokens.access.isEmpty() || refreshing) return;
    request("GET", QUrl("https://id.twitch.tv/oauth2/validate"), {}, false, [this, allowRefresh](const Response &r) {
        if (r.code == 401 && allowRefresh) { refresh(); return; }
        const auto o = r.object;
        if (r.code == 200 && !r.networkError && r.validJson && o["client_id"].toString() == clientId &&
            validCategoryId(o["user_id"].toString()) && o["scopes"].toArray().contains("channel:manage:broadcast") && o["expires_in"].toInt() > 0) {
            userId = o["user_id"].toString(); loginName = o["login"].toString(); ready = true;
            emit accountChanged(); emit status("Twitch 帳號已連線", StatusLevel::Success); readCurrent();
        } else {
            ready = false; emit accountChanged();
            if (r.code == 200 || r.code == 401) {
                tokens = {}; validationTimer.stop();
                if (persistTokens) Credentials::erase(clientId);
                emit status("授權帳號、Client ID 或權限驗證失敗，請重新登入", StatusLevel::Error);
            } else emit status("暫時無法驗證授權，請重新連線；" + failure(r), StatusLevel::Error);
        }
    });
}
void Client::refresh()
{
    if (refreshing) return;
    ready = false; emit accountChanged();
    if (tokens.refresh.isEmpty()) { emit status("請重新登入 Twitch", StatusLevel::Error); return; }
    refreshing = true;
    request("POST", QUrl("https://id.twitch.tv/oauth2/token"), form({{"client_id", clientId},
        {"grant_type", "refresh_token"}, {"refresh_token", tokens.refresh}}), false, [this](const Response &r) {
        refreshing = false;
        if (acceptTokens(r)) { validate(false); return; }
        // A lost response may already have consumed a one-time refresh token. Never retry it blindly.
        tokens = {}; validationTimer.stop();
        if (persistTokens) Credentials::erase(clientId);
        emit status("無法更新授權，請重新登入 Twitch", StatusLevel::Error);
    });
}
void Client::logout()
{
    const auto access = tokens.access; const auto id = clientId;
    const bool erased = !persistTokens || Credentials::erase(id);
    resetSession(); emit authorizationFinished();
    emit status(erased ? "已登出並清除本機憑證" : "已斷開連線，但本機憑證清除失敗", erased ? StatusLevel::Notice : StatusLevel::Error);
    if (access.isEmpty()) return;
    request("POST", QUrl("https://id.twitch.tv/oauth2/revoke"), form({{"client_id", id}, {"token", access}}), false,
        [this](const Response &r) { emit activity(r.code == 200 && !r.networkError ? "Twitch 授權已撤銷" : "未能確認遠端撤銷；可至 Twitch 連線設定移除此應用程式"); });
}
bool Client::parseCategory(const QJsonObject &o, Category &c)
{
    c = {o["id"].toString(), o["name"].toString(), o["box_art_url"].toString()};
    return validCategoryId(c.id) && !c.name.isEmpty() && c.name.size() <= 256;
}
void Client::search(const QString &query, const QString &cursor)
{
    const auto seq = ++searchGeneration;
    if (!ready || query.trimmed().isEmpty()) { emit results({}, {}); emit searchFinished(); return; }
    QUrl url("https://api.twitch.tv/helix/search/categories");
    QUrlQuery q; q.addQueryItem("query", query.left(200)); q.addQueryItem("first", "20");
    if (!cursor.isEmpty()) q.addQueryItem("after", cursor);
    url.setQuery(q);
    request("GET", url, {}, true, [this, seq](const Response &r) {
        if (seq != searchGeneration) return;
        emit searchFinished();
        if (r.code != 200 || r.networkError || !r.validJson || !r.object["data"].isArray()) {
            emit status(failure(r), StatusLevel::Error); if (r.code == 401) refresh(); return;
        }
        QVector<Category> list;
        for (const auto &v : r.object["data"].toArray()) { Category c; if (parseCategory(v.toObject(), c)) list.append(c); }
        emit results(list, r.object["pagination"].toObject()["cursor"].toString());
    });
}
QUrl Client::channelUrl() const
{
    QUrl url("https://api.twitch.tv/helix/channels"); QUrlQuery q;
    q.addQueryItem("broadcaster_id", userId); url.setQuery(q); return url;
}
static bool channel(const Response &r, const QString &user, Category &c)
{
    const auto array = r.object["data"].toArray();
    if (r.code != 200 || r.networkError || !r.validJson || array.size() != 1) return false;
    const auto obj = array.first().toObject();
    if (obj["broadcaster_id"].toString() != user || !obj["game_id"].isString() || !obj["game_name"].isString()) return false;
    c = {obj["game_id"].toString(), obj["game_name"].toString(), {}};
    return c.id.isEmpty() || validCategoryId(c.id);
}
void Client::readCurrent()
{
    if (!ready || updating || reading) return;
    reading = true; emit currentReadChanged();
    emit status("正在重新整理目前分類…", StatusLevel::Progress);
    const auto operation = updateGeneration;
    request("GET", channelUrl(), {}, true, [this, operation](const Response &r) {
        reading = false; emit currentReadChanged();
        if (operation != updateGeneration || updating) return;
        Category c;
        if (channel(r, userId, c)) { currentCategory = c; emit currentChanged(); emit status("已讀取目前分類", StatusLevel::Success); loadCurrentArt(); }
        else { emit status("無法取得最新分類；" + failure(r), StatusLevel::Error); if (r.code == 401) refresh(); }
    });
}
void Client::apply(const Category &category)
{
    if (!ready || updating || !validCategoryId(category.id)) return;
    updating = true; const auto operation = ++updateGeneration;
    emit status("正在送出分類更新…", StatusLevel::Progress);
    request("PATCH", channelUrl(), QJsonDocument(QJsonObject{{"game_id", category.id}}).toJson(QJsonDocument::Compact), true,
        [this, category, operation](const Response &r) {
        if (operation != updateGeneration) return;
        if (r.code == 204 && !r.networkError) {
            emit status("已送出更新請求，正在確認結果", StatusLevel::Progress); confirm(category, operation, 0);
        } else if (r.networkError || r.code == 0 || r.code >= 500) {
            emit status("更新回應不確定，正在讀取分類確認"); confirm(category, operation, 0);
        } else {
            finishUpdate("更新失敗；最後已知分類：" + currentCategory.name + "\n原因：" + failure(r));
            if (r.code == 401) refresh();
        }
    });
}
void Client::resolveCategory(const QString &key, const QString &name)
{
    if (!ready || !validLibraryKey(key) || name.trimmed().isEmpty() || name.size() > 256) { emit categoryResolved(key, {}); return; }
    QUrl url("https://api.twitch.tv/helix/games"); QUrlQuery query; query.addQueryItem("name", name); url.setQuery(query);
    request("GET", url, {}, true, [this, key, name](const Response &r) {
        const auto data = r.object["data"].toArray(); Category category;
        if (r.code != 200 || r.networkError || data.size() != 1 || !parseCategory(data.first().toObject(), category) || category.name.compare(name, Qt::CaseInsensitive) != 0) category = {};
        emit categoryResolved(key, category);
    });
}
void Client::confirm(Category target, quint64 operation, int attempt)
{
    request("GET", channelUrl(), {}, true, [this, target, operation, attempt](const Response &r) {
        if (operation != updateGeneration) return;
        Category actual;
        if (channel(r, userId, actual)) {
            currentCategory = actual; emit currentChanged();
            if (actual.id == target.id) { finishUpdate("更新成功，目前分類：" + actual.name, StatusLevel::Success); loadCurrentArt(); return; }
        }
        if (r.code == 401 || r.code == 403 || r.code == 429 || attempt >= 3) {
            finishUpdate("暫時無法確認分類是否更新；最後已知分類：" + currentCategory.name, StatusLevel::Notice);
            if (r.code == 401) refresh();
            return;
        }
        const auto epoch = generation;
        QTimer::singleShot((1 << attempt) * 1000, this, [this, target, operation, attempt, epoch] {
            if (epoch == generation && operation == updateGeneration) confirm(target, operation, attempt + 1);
        });
    });
}
void Client::finishUpdate(const QString &message, StatusLevel level)
{
    updating = false; emit status(message, level); emit activity(message);
}
void Client::loadCurrentArt()
{
    if (!validCategoryId(currentCategory.id)) return;
    const auto id = currentCategory.id; const auto operation = updateGeneration;
    QUrl url("https://api.twitch.tv/helix/games"); QUrlQuery query; query.addQueryItem("id", id); url.setQuery(query);
    request("GET", url, {}, true, [this, id, operation](const Response &r) {
        if (r.code != 200 || r.networkError || operation != updateGeneration || currentCategory.id != id) return;
        const auto data = r.object["data"].toArray(); Category c;
        if (data.size() == 1 && parseCategory(data.first().toObject(), c) && c.id == id) {
            currentCategory.art = c.art.replace("{width}", "90").replace("{height}", "120"); emit currentChanged();
        }
    });
}
}
