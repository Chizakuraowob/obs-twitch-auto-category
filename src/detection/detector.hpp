#pragma once
#include "settings/settings.hpp"
#include <QStringList>
#include <QSet>
namespace tca {
struct InstalledGame { QString key, name, directory; };
struct RunningGame { QString key, name, executable; };
struct WindowEvidence { QString executable, windowClass, title, pathHash, parentExecutable, suggestedName; bool foreground = false; };
struct Processes { QStringList executables; QString foreground; bool available = false; QVector<Rule> recognizedGames; QVector<RunningGame> localGames; int libraryCount = 0; QVector<WindowEvidence> windows; };
struct Candidate { QString executable, id, name; int priority = 0; };
QString suggestionKey(const Candidate &candidate);
struct SuggestionSession {
    QSet<QString> handled, games;
    QMap<QString, qint64> missingSince;
    bool played = false, idleNotified = false, idleReady = false;
    qint64 idleSince = -1;
    void pause() { idleSince = -1; idleReady = false; }
    void update(const Processes &processes, const QVector<Candidate> &detected, bool uncertain, qint64 now);
};
Processes scanProcesses(bool libraries = true, const QStringList &learnedExecutables = {}, bool inspectForeground = true);
bool canLearnWindow(const QString &executable);
QString windowKey(const WindowEvidence &window);
// -1 means unknown; -2 means conflicting rules. Neither may update a category.
int matchRecognition(const WindowEvidence &window, const QVector<RecognitionRule> &rules);
QVector<Rule> learnedCandidates(const QVector<WindowEvidence> &windows, const QVector<RecognitionRule> &rules);
QVector<InstalledGame> readSteamLibrary(const QString &steamRoot);
QVector<InstalledGame> readEpicLibrary(const QString &manifestDirectory);
InstalledGame matchInstalledGame(const QString &executablePath, const QVector<InstalledGame> &games, bool excludeHelpers = true);
Rule recognizeGame(const QString &executable, const QString &windowTitle);
RunningGame recognizeStandaloneGame(const QString &executable, const QString &windowTitle);
QVector<Candidate> candidates(const Processes &processes, const QVector<Rule> &rules);
// -1 means the policy cannot unambiguously select a candidate.
int chooseCandidate(const QVector<Candidate> &list, const QString &foreground, int policy);
}
