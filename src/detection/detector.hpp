#pragma once
#include "settings/settings.hpp"
#include <QStringList>
namespace tca {
struct InstalledGame { QString key, name, directory; };
struct RunningGame { QString key, name, executable; };
struct Processes { QStringList executables; QString foreground; bool available = false; QVector<Rule> recognizedGames; QVector<RunningGame> localGames; int libraryCount = 0; };
struct Candidate { QString executable, id, name; int priority = 0; };
Processes scanProcesses(bool libraries = true);
QVector<InstalledGame> readSteamLibrary(const QString &steamRoot);
QVector<InstalledGame> readEpicLibrary(const QString &manifestDirectory);
InstalledGame matchInstalledGame(const QString &executablePath, const QVector<InstalledGame> &games);
Rule recognizeGame(const QString &executable, const QString &windowTitle);
QVector<Candidate> candidates(const Processes &processes, const QVector<Rule> &rules);
// -1 means the policy cannot unambiguously select a candidate.
int chooseCandidate(const QVector<Candidate> &list, const QString &foreground, int policy);
}
