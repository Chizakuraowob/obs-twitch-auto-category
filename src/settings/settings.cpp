#include "settings.hpp"
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QCryptographicHash>
#include <algorithm>

namespace tca {
bool validCategoryId(const QString &id) { return QRegularExpression("^[0-9]{1,20}$").match(id).hasMatch(); }
bool validLibraryKey(const QString &key) { return key == "builtin:nte" || QRegularExpression("^(steam|epic|gog):[a-zA-Z0-9._-]{1,128}$").match(key).hasMatch(); }
bool validExecutable(const QString &name)
{
    return name.size() <= 260 && QRegularExpression("^[^/\\\\:*?\"<>|]+\\.exe$", QRegularExpression::CaseInsensitiveOption).match(name).hasMatch();
}
bool validRecognitionRule(const RecognitionRule &r)
{
    const auto safeText = [](const QString &s) { return s.size() <= 256 && !s.contains(QRegularExpression("[\\x00-\\x1f\\x7f]")); };
    const bool path = !r.pathHash.isEmpty();
    return validExecutable(r.executable) && safeText(r.executable) && !r.windowClass.isEmpty() && safeText(r.windowClass) && safeText(r.title) &&
        (!path || QRegularExpression("^[0-9a-f]{64}$").match(r.pathHash).hasMatch()) &&
        (path || r.title.trimmed().size() >= 2) && (!r.titlePrefix || r.title.trimmed().size() >= 4) &&
        (r.ignored ? r.categoryId.isEmpty() && r.categoryName.isEmpty() : validCategoryId(r.categoryId) && !r.categoryName.trimmed().isEmpty() && safeText(r.categoryName));
}
QString recognitionKey(const RecognitionRule &r)
{
    const QJsonArray fields{r.executable.toLower(), r.windowClass, r.title.simplified().toCaseFolded(), r.pathHash, r.titlePrefix};
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(fields).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
}
QJsonArray recognitionJson(const QVector<RecognitionRule> &rules)
{
    QJsonArray array;
    for (const auto &r : rules) array.append(QJsonObject{{"exe", r.executable}, {"class", r.windowClass}, {"title", r.title}, {"pathHash", r.pathHash},
        {"categoryId", r.categoryId}, {"categoryName", r.categoryName}, {"titlePrefix", r.titlePrefix}, {"ignored", r.ignored}});
    return array;
}
static bool parseRecognition(const QJsonArray &array, QVector<RecognitionRule> *rules)
{
    if (array.size() > 500) return false;
    QSet<QString> seen;
    for (const auto &entry : array) {
        if (!entry.isObject()) return false;
        const auto o = entry.toObject();
        for (const auto &key : {"exe", "class", "title", "pathHash", "categoryId", "categoryName"}) if (!o[key].isString()) return false;
        if (!o["titlePrefix"].isBool() || !o["ignored"].isBool()) return false;
        RecognitionRule r{o["exe"].toString().toLower(), o["class"].toString(), o["title"].toString().simplified(), o["pathHash"].toString(), o["categoryId"].toString(), o["categoryName"].toString(), o["titlePrefix"].toBool(), o["ignored"].toBool()};
        const auto key = recognitionKey(r);
        if (!validRecognitionRule(r) || seen.contains(key)) return false;
        seen.insert(key); rules->append(r);
    }
    return true;
}
bool importRecognitionRules(const QByteArray &data, const QVector<RecognitionRule> &existing, QVector<RecognitionRule> *merged, QString *error)
{
    *error = "規則檔格式錯誤、包含衝突或超過 500 筆；未變更既有規則";
    if (data.size() > 1024 * 1024) return false;
    const auto document = QJsonDocument::fromJson(data); const auto root = document.object();
    QVector<RecognitionRule> incoming;
    if (!document.isObject() || root["format"] != "tca-recognition" || root["version"].toInt() != 1 || !root["rules"].isArray() || !parseRecognition(root["rules"].toArray(), &incoming)) return false;
    auto next = existing;
    for (const auto &r : incoming) {
        bool duplicate = false;
        for (const auto &old : next) if (recognitionKey(old) == recognitionKey(r)) {
            if (old.categoryId != r.categoryId || old.ignored != r.ignored) return false;
            duplicate = true;
        }
        if (!duplicate) next.append(r);
    }
    if (next.size() > 500) return false;
    *merged = next; error->clear(); return true;
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
        list.append(QJsonObject{{"exe", r.executable}, {"id", r.categoryId}, {"name", r.categoryName}, {"priority", r.priority}, {"ignored", r.ignored}});
    QJsonArray savedFavorites;
    for (const auto &f : favorites) savedFavorites.append(QJsonObject{{"id", f.id}, {"name", f.name}});
    QJsonObject cache; for (auto it = categoryCache.begin(); it != categoryCache.end(); ++it) cache[it.key()] = QJsonObject{{"id", it->id}, {"name", it->name}};
    return {{"schema", 1}, {"clientId", clientId}, {"theme", theme}, {"fontFamily", fontFamily}, {"fontSize", fontSize}, {"animations", animations}, {"automatic", automatic},
        {"notifications", notifications}, {"liveOnly", liveOnly}, {"paused", paused}, {"multiGame", multiGame},
        {"developerMode", developerMode}, {"favorites", savedFavorites},
        {"autoDetect", autoDetect}, {"detectLibraries", detectLibraries}, {"allowCategoryLookup", allowCategoryLookup}, {"categoryCache", cache},
        {"learnUnknown", learnUnknown}, {"recognitionRules", recognitionJson(recognitionRules)},
        {"chattingSuggestion", chattingSuggestion}, {"chattingDelay", chattingDelay}, {"checkUpdates", checkUpdates}, {"skippedUpdate", skippedUpdate},
        {"ignoredLibraries", QJsonArray::fromStringList(ignoredLibraries)},
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
    s.learnUnknown = o["learnUnknown"].toBool(true);
    s.chattingSuggestion = o["chattingSuggestion"].toBool(true); s.chattingDelay = std::clamp(o["chattingDelay"].toInt(90), 15, 600);
    s.checkUpdates = o["checkUpdates"].toBool(false); s.skippedUpdate = o["skippedUpdate"].toString().left(32);
    for (const auto &key : o["ignoredLibraries"].toArray()) if (validLibraryKey(key.toString()) && !s.ignoredLibraries.contains(key.toString()) && s.ignoredLibraries.size() < 1000) s.ignoredLibraries.append(key.toString());
    QVector<RecognitionRule> learned; if (parseRecognition(o["recognitionRules"].toArray(), &learned)) s.recognitionRules = learned;
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
        Rule rule{r["exe"].toString().trimmed().toLower(), r["id"].toString(), r["name"].toString().left(256), r["priority"].toInt(), r["ignored"].toBool()};
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
    QVector<RecognitionRule> learned;
    if (doc.object().contains("recognitionRules") && (!doc.object()["recognitionRules"].isArray() || !parseRecognition(doc.object()["recognitionRules"].toArray(), &learned))) {
        *error = "辨識規則損壞；原設定保留，儲存前會先備份"; return {};
    }
    return fromJson(doc.object());
}
bool Settings::save(const QString &path, QString *error) const
{
    QSaveFile file(path);
    const auto data = QJsonDocument(json()).toJson();
    if (data.size() > 1024 * 1024) { *error = "設定超過 1 MB，請減少規則或快取；原檔未變更"; return false; }
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        *error = "設定儲存失敗"; return false;
    }
    return true;
}
}
