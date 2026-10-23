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
struct Settings {
    QString clientId;
    int theme = 0; // system, light, dark
    bool automatic = false, notifications = true, liveOnly = true, paused = false;
    int multiGame = 1; // foreground, manual, priority
    bool wizardDone = false;
    QString termsVersion, privacyVersion, termsUtc, privacyUtc;
    QVector<Rule> rules;
    bool consented() const;
    QJsonObject json() const;
    static Settings fromJson(const QJsonObject &object);
    static Settings load(const QString &path, QString *error);
    bool save(const QString &path, QString *error) const;
};
bool validCategoryId(const QString &id);
bool validExecutable(const QString &name);
}
