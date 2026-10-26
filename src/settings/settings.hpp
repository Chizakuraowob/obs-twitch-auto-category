#pragma once
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QJsonArray>

namespace tca {
inline const QString DocumentVersion = QStringLiteral("draft-2026-10-26-updates-v3");
struct Rule {
    QString executable, categoryId, categoryName;
    int priority = 0;
    bool ignored = false;
};
struct Favorite { QString id, name; };
struct RecognitionRule {
    QString executable, windowClass, title, pathHash, categoryId, categoryName;
    bool titlePrefix = false, ignored = false;
};
bool validRecognitionRule(const RecognitionRule &rule);
QString recognitionKey(const RecognitionRule &rule);
QJsonArray recognitionJson(const QVector<RecognitionRule> &rules);
bool importRecognitionRules(const QByteArray &data, const QVector<RecognitionRule> &existing, QVector<RecognitionRule> *merged, QString *error);
struct Settings {
    QString clientId;
    QString fontFamily = QStringLiteral("Microsoft JhengHei UI");
    int fontSize = 14;
    bool animations = true;
    int theme = 0; // system, light, dark
    bool automatic = false, notifications = true, liveOnly = true, paused = false;
    bool developerMode = false;
    bool autoDetect = true, detectLibraries = true, allowCategoryLookup = false;
    bool learnUnknown = true;
    bool chattingSuggestion = true, checkUpdates = false;
    int chattingDelay = 90;
    QString skippedUpdate;
    QVector<RecognitionRule> recognitionRules;
    QMap<QString, Favorite> categoryCache;
    QStringList ignoredLibraries;
    int multiGame = 1; // foreground, manual, priority
    bool wizardDone = false;
    QString termsVersion, privacyVersion, termsUtc, privacyUtc;
    QVector<Rule> rules;
    QVector<Favorite> favorites;
    bool suggestionsEnabled(bool streaming) const { return autoDetect && !paused && (developerMode || !liveOnly || streaming); }
    bool consented() const;
    QJsonObject json() const;
    static Settings fromJson(const QJsonObject &object);
    static Settings load(const QString &path, QString *error);
    bool save(const QString &path, QString *error) const;
};
bool validCategoryId(const QString &id);
bool validLibraryKey(const QString &key);
bool validExecutable(const QString &name);
}
