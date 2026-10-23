#pragma once
#include "settings/settings.hpp"
#include <QStringList>
namespace tca {
struct Processes { QStringList executables; QString foreground; bool available = false; };
struct Candidate { QString executable, id, name; int priority = 0; };
Processes scanProcesses();
QVector<Candidate> candidates(const Processes &processes, const QVector<Rule> &rules);
// -1 means the policy cannot unambiguously select a candidate.
int chooseCandidate(const QVector<Candidate> &list, const QString &foreground, int policy);
}
