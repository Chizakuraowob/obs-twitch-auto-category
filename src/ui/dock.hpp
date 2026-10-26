#pragma once
#include "settings/settings.hpp"
#include "twitch/client.hpp"
#include "detection/detector.hpp"
#include "update/updater.hpp"
#include <QFutureWatcher>
#include <QWidget>
#include <QSet>
#include <QPixmap>
#include <QPointer>
class QMessageBox;
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
    friend struct DockTestAccess;
    bool save();
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
    void notifySuggestion(const Candidate &candidate);
    void reviewUnknown();
    void recognitionDialog();
    void pickGame();
    void updateUnknown(const Processes &processes);
    bool rememberRecognition(const RecognitionRule &rule);
    void fetchArt(const Category &category);

    void setStatus(const QString &message);
    void animate(QWidget *widget);
    QString effectiveClientId() const;
    QString directory, settingsPath;
    Settings config;
    Client client;
    Updater updater;
    QNetworkAccessManager images;
    QFutureWatcher<Processes> watcher;
    QTimer scanTimer, searchTimer;
    QLabel *accountLabel, *currentLabel, *currentArt, *syncLabel, *statusLabel, *suggestionLabel, *searchHint;
    QLineEdit *searchInput;
    QListWidget *searchList, *activityList;
    QComboBox *suggestionChoice, *favoriteChoice;
    QComboBox *localGameChoice;
    QPushButton *linkLocalGame;
    QVector<RunningGame> localGames;
    QSet<QString> lookupAttempted;
    bool lookupPending = false;
    int libraryCount = 0;
    QPushButton *favoriteAdd, *favoriteApply, *favoriteRemove;
    QPushButton *connectButton, *searchButton, *applyButton, *moreButton, *suggestionApply, *resumeButton;
    QPushButton *activityToggle, *mappingButton, *ignoreButton;
    QVector<Category> searchCategories;
    QVector<Candidate> detected;
    QSet<QString> ignored;
    QHash<QString, QPixmap> artCache;
    QSet<QString> pendingArt;
    QString cursor, searchQuery, stableKey, autoAttemptKey;
    QString notificationKey;
    SuggestionSession suggestionSession;
    QPointer<QMessageBox> suggestionPrompt;

    WindowEvidence pendingWindow;
    QVector<WindowEvidence> observedWindows;
    QLabel *unknownLabel;
    QPushButton *reviewUnknownButton;



    bool reviewingUnknown = false;
    qint64 stableSince = 0, lastAuto = 0;
    bool streaming = false, manualOverride = false, stopped = false, settingsReadError = false, wizardOpen = false;
};
}
