#pragma once
#include "settings/settings.hpp"
#include <QStringList>
namespace tca {
struct Processes { QStringList executables; QString foreground; bool available = false; QVector<Rule> recognizedGames; };
struct Candidate { QString executable, id, name; int priority = 0; };
Processes scanProcesses();
Rule recognizeGame(const QString &executable, const QString &windowTitle);
QVector<Candidate> candidates(const Processes &processes, const QVector<Rule> &rules);
// -1 means the policy cannot unambiguously select a candidate.
int chooseCandidate(const QVector<Candidate> &list, const QString &foreground, int policy);
}
