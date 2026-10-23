#include "detector.hpp"
#include <QSet>
#include <windows.h>
#include <tlhelp32.h>
namespace tca {
Processes scanProcesses()
{
    Processes result;
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return result;
    DWORD foreground = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foreground);
    PROCESSENTRY32W entry{}; entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        result.available = true;
        do {
            const auto exe = QString::fromWCharArray(entry.szExeFile).toLower();
            result.executables.append(exe);
            if (entry.th32ProcessID == foreground) result.foreground = exe;
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    result.executables.removeDuplicates();
    return result;
}
QVector<Candidate> candidates(const Processes &processes, const QVector<Rule> &rules)
{
    QVector<Candidate> result;
    if (!processes.available) return result;
    QSet<QString> seen;
    for (const auto &rule : rules) {
        const auto exe = rule.executable.toLower();
        if (!processes.executables.contains(exe, Qt::CaseInsensitive) || seen.contains(exe)) continue;
        seen.insert(exe);
        // Conflicting mappings never qualify for automatic selection.
        bool conflict = false;
        for (const auto &other : rules)
            if (other.executable.compare(exe, Qt::CaseInsensitive) == 0 && other.categoryId != rule.categoryId) conflict = true;
        if (!conflict) result.append({exe, rule.categoryId, rule.categoryName, rule.priority});
    }
    return result;
}
int chooseCandidate(const QVector<Candidate> &list, const QString &foreground, int policy)
{
    if (list.isEmpty()) return -1;
    if (list.size() == 1) return 0;
    if (policy == 1) return -1;
    if (policy == 0) {
        for (int i = 0; i < list.size(); ++i)
            if (list[i].executable.compare(foreground, Qt::CaseInsensitive) == 0) return i;
        return -1;
    }
    int best = 0; bool tied = false;
    for (int i = 1; i < list.size(); ++i) {
        if (list[i].priority > list[best].priority) { best = i; tied = false; }
        else if (list[i].priority == list[best].priority) tied = true;
    }
    return tied ? -1 : best;
}
}
