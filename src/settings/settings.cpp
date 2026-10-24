#include "settings.hpp"
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <algorithm>

namespace tca {
bool validCategoryId(const QString &id) { return QRegularExpression("^[0-9]{1,20}$").match(id).hasMatch(); }
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
    return {{"schema", 1}, {"clientId", clientId}, {"theme", theme}, {"fontFamily", fontFamily}, {"fontSize", fontSize}, {"animations", animations}, {"automatic", automatic},
        {"notifications", notifications}, {"liveOnly", liveOnly}, {"paused", paused}, {"multiGame", multiGame},
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
