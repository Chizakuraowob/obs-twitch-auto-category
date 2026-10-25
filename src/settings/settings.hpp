#pragma once
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <QJsonArray>

namespace tca {
inline const QString DocumentVersion = QStringLiteral("draft-2026-10-25-recognition-v2");
struct Rule {
    QString executable, categoryId, categoryName;
    int priority = 0;
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
    QVector<RecognitionRule> recognitionRules;
    QMap<QString, Favorite> categoryCache;
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
