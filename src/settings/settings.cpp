#include "settings.hpp"
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <algorithm>

namespace tca {
bool validCategoryId(const QString &id) { return QRegularExpression("^[0-9]{1,20}$").match(id).hasMatch(); }
bool validLibraryKey(const QString &key) { return QRegularExpression("^(steam|epic|gog):[a-zA-Z0-9._-]{1,128}$").match(key).hasMatch(); }
bool validExecutable(const QString &name)
{
    return name.size() <= 260 && QRegularExpression("^[^/\\\\:*?\"<>|]+\\.exe$", QRegularExpression::CaseInsensitiveOption).match(name).hasMatch();
}
bool Settings::consented() const
{
    return termsVersion == DocumentVersion && privacyVersion == DocumentVersion &&
           QDateTime::fromString(termsUtc, Qt::ISODate).isValid() &&
           QDateTime::fromString(privacyUtc, Qt::ISODate).isValid();
}
QJsonObject Settings::json() const
{
    QJsonArray list;
    for (const auto &r : rules)
        list.append(QJsonObject{{"exe", r.executable}, {"id", r.categoryId}, {"name", r.categoryName}, {"priority", r.priority}});
    QJsonArray savedFavorites;
    for (const auto &f : favorites) savedFavorites.append(QJsonObject{{"id", f.id}, {"name", f.name}});
    QJsonObject cache; for (auto it = categoryCache.begin(); it != categoryCache.end(); ++it) cache[it.key()] = QJsonObject{{"id", it->id}, {"name", it->name}};
    return {{"schema", 1}, {"clientId", clientId}, {"theme", theme}, {"fontFamily", fontFamily}, {"fontSize", fontSize}, {"animations", animations}, {"automatic", automatic},
        {"notifications", notifications}, {"liveOnly", liveOnly}, {"paused", paused}, {"multiGame", multiGame},
        {"developerMode", developerMode}, {"favorites", savedFavorites},
        {"autoDetect", autoDetect}, {"detectLibraries", detectLibraries}, {"allowCategoryLookup", allowCategoryLookup}, {"categoryCache", cache},
        {"wizardDone", wizardDone}, {"wizardRevision", 2}, {"termsVersion", termsVersion}, {"privacyVersion", privacyVersion},
        {"termsUtc", termsUtc}, {"privacyUtc", privacyUtc}, {"rules", list}};
}
Settings Settings::fromJson(const QJsonObject &o)
{
    Settings s;
    s.clientId = o["clientId"].toString().trimmed();
    if (!QRegularExpression("^[a-zA-Z0-9]{0,128}$").match(s.clientId).hasMatch()) s.clientId.clear();
    s.theme = std::clamp(o["theme"].toInt(), 0, 2);
    s.fontFamily = o["fontFamily"].toString(s.fontFamily).left(128);
    s.fontSize = std::clamp(o["fontSize"].toInt(14), 12, 22);
    s.animations = o["animations"].toBool(true);
    s.multiGame = std::clamp(o["multiGame"].toInt(1), 0, 2);
    s.automatic = o["automatic"].toBool(); s.notifications = o["notifications"].toBool(true);
    s.liveOnly = o["liveOnly"].toBool(true); s.paused = o["paused"].toBool();
    s.developerMode = o["developerMode"].toBool();
    s.autoDetect = o["autoDetect"].toBool(true); s.detectLibraries = o["detectLibraries"].toBool(true); s.allowCategoryLookup = o["allowCategoryLookup"].toBool();
    const auto cache = o["categoryCache"].toObject();
    for (auto it = cache.begin(); it != cache.end() && s.categoryCache.size() < 1000; ++it) {
        const auto value = it.value().toObject(); const auto id = value["id"].toString(), name = value["name"].toString().trimmed();
        if (validLibraryKey(it.key()) && validCategoryId(id) && !name.isEmpty() && name.size() <= 256) s.categoryCache.insert(it.key(), {id, name});
    }
    QSet<QString> favoriteIds;
    for (const auto &entry : o["favorites"].toArray()) {
        const auto f = entry.toObject();
        Favorite favorite{f["id"].toString(), f["name"].toString().trimmed().left(256)};
        if (validCategoryId(favorite.id) && !favorite.name.isEmpty() && !favoriteIds.contains(favorite.id) && s.favorites.size() < 100) {
            favoriteIds.insert(favorite.id); s.favorites.append(favorite);
        }
    }
    s.wizardDone = o["wizardDone"].toBool() && o["wizardRevision"].toInt() == 2;
    s.termsVersion = o["termsVersion"].toString(); s.privacyVersion = o["privacyVersion"].toString();
    s.termsUtc = o["termsUtc"].toString(); s.privacyUtc = o["privacyUtc"].toString();
    for (const auto &entry : o["rules"].toArray()) {
        auto r = entry.toObject();
        Rule rule{r["exe"].toString().trimmed().toLower(), r["id"].toString(), r["name"].toString().left(256), r["priority"].toInt()};
        if (validExecutable(rule.executable) && validCategoryId(rule.categoryId) && !rule.categoryName.isEmpty() && s.rules.size() < 500)
            s.rules.push_back(rule);
    }
    return s;
}
Settings Settings::load(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.exists()) return {};
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) { *error = "無法讀取設定檔"; return {}; }
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject() || doc.object()["schema"].toInt() != 1) {
        *error = "設定檔損壞或版本不支援；原檔保留，儲存前請先備份"; return {};
    }
    return fromJson(doc.object());
}
bool Settings::save(const QString &path, QString *error) const
{
    QSaveFile file(path);
    const auto data = QJsonDocument(json()).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        *error = "設定儲存失敗"; return false;
    }
    return true;
}
}
