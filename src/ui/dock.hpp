#pragma once
#include "settings/settings.hpp"
#include "twitch/client.hpp"
#include "detection/detector.hpp"
#include <QFutureWatcher>
#include <QWidget>
#include <QSet>
#include <QPixmap>
class QLabel;
class QPushButton;
class QLineEdit;
class QListWidget;
class QComboBox;
namespace tca {
class Dock : public QWidget {
    Q_OBJECT
public:
    explicit Dock(QString settingsDirectory, QWidget *parent = nullptr);
    ~Dock() override;
    void setStreaming(bool active);
    void stop();
    bool needsSetup() const { return !config.wizardDone || !config.consented(); }
    void beginFirstRun();
private:
    void save();
    void theme();
    void settingsDialog();
    void wizard();
    void login();
    void document(const QString &kind);
    void updateAccount();
    void addActivity(const QString &text);
    void scan();
    void processScan();
    void applySelected();
    void refreshFavorites();
    void applySuggestion();
    void fetchArt(const Category &category);
    void rulesDialog();
    void setStatus(const QString &message);
    void animate(QWidget *widget);
    QString effectiveClientId() const;
    QString directory, settingsPath;
    Settings config;
    Client client;
    QNetworkAccessManager images;
    QFutureWatcher<Processes> watcher;
    QTimer scanTimer, searchTimer;
    QLabel *accountLabel, *currentLabel, *currentArt, *syncLabel, *statusLabel, *suggestionLabel, *searchHint;
    QLineEdit *searchInput;
    QListWidget *searchList, *activityList;
    QComboBox *suggestionChoice, *favoriteChoice;
    QPushButton *favoriteAdd, *favoriteApply, *favoriteRemove;
    QPushButton *connectButton, *searchButton, *applyButton, *moreButton, *suggestionApply, *resumeButton;
    QPushButton *activityToggle, *mappingButton, *ignoreButton;
    QVector<Category> searchCategories;
    QVector<Candidate> detected;
    QSet<QString> ignored;
    QHash<QString, QPixmap> artCache;
    QSet<QString> pendingArt;
    QString cursor, searchQuery, stableKey, autoAttemptKey;
    qint64 stableSince = 0, lastAuto = 0;
    bool streaming = false, manualOverride = false, stopped = false, settingsReadError = false, wizardOpen = false;
};
}
