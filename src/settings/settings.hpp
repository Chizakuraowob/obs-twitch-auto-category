#pragma once
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace tca {
inline const QString DocumentVersion = QStringLiteral("draft-2026-10-24");
struct Rule {
    QString executable, categoryId, categoryName;
    int priority = 0;
};
struct Favorite { QString id, name; };
struct Settings {
    QString clientId;
    QString fontFamily = QStringLiteral("Microsoft JhengHei UI");
    int fontSize = 14;
    bool animations = true;
    int theme = 0; // system, light, dark
    bool automatic = false, notifications = true, liveOnly = true, paused = false;
    bool developerMode = false;
    int multiGame = 1; // foreground, manual, priority
    bool wizardDone = false;
    QString termsVersion, privacyVersion, termsUtc, privacyUtc;
    QVector<Rule> rules;
    QVector<Favorite> favorites;
    bool suggestionsEnabled(bool streaming) const { return !paused && (developerMode || !liveOnly || streaming); }
    bool consented() const;
    QJsonObject json() const;
    static Settings fromJson(const QJsonObject &object);
    static Settings load(const QString &path, QString *error);
    bool save(const QString &path, QString *error) const;
};
bool validCategoryId(const QString &id);
bool validExecutable(const QString &name);
}
