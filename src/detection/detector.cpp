#include "detector.hpp"
#include <QSet>
#include <QHash>
#include <QRegularExpression>
#include <windows.h>
#include <tlhelp32.h>
namespace tca {
Rule recognizeGame(const QString &executable, const QString &windowTitle)
{
    const auto exe = executable.toLower();
    // Java is shared by many applications; require the Minecraft game window, not its launcher.
    static const QRegularExpression javaTitle("^Minecraft\\*?\\s+(?:[0-9]+\\.[0-9]+|[0-9]{2}w[0-9]{2}[a-z])(?:.*)$");
    if (exe == "minecraft.windows.exe" ||
        ((exe == "javaw.exe" || exe == "java.exe") && javaTitle.match(windowTitle).hasMatch()))
        return {exe, "27471", "Minecraft", 0};
    return {};
}
struct GameWindows { QHash<DWORD, QString> processes; QVector<Rule> games; };
static BOOL CALLBACK inspectGameWindow(HWND window, LPARAM parameter)
{
    if (!IsWindowVisible(window)) return TRUE;
    auto &context = *reinterpret_cast<GameWindows *>(parameter);
    DWORD pid = 0; GetWindowThreadProcessId(window, &pid);
    const auto exe = context.processes.value(pid);
    if (exe.isEmpty()) return TRUE;
    wchar_t title[512]{}; GetWindowTextW(window, title, 512);
    const auto game = recognizeGame(exe, QString::fromWCharArray(title));
    if (!game.categoryId.isEmpty()) context.games.append(game);
    return TRUE;
}
Processes scanProcesses()
{
    Processes result;
    GameWindows games;
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
            if (exe == "javaw.exe" || exe == "java.exe" || exe == "minecraft.windows.exe") games.processes.insert(entry.th32ProcessID, exe);
            if (entry.th32ProcessID == foreground) result.foreground = exe;
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    EnumWindows(inspectGameWindow, reinterpret_cast<LPARAM>(&games));
    result.recognizedGames = games.games;
    result.executables.removeDuplicates();
    return result;
}
QVector<Candidate> candidates(const Processes &processes, const QVector<Rule> &rules)
{
    QVector<Candidate> result;
    if (!processes.available) return result;
    auto effectiveRules = rules;
    for (const auto &game : processes.recognizedGames) {
        bool overridden = false;
        for (const auto &rule : effectiveRules) if (rule.executable.compare(game.executable, Qt::CaseInsensitive) == 0) overridden = true;
        if (!overridden) effectiveRules.append(game);
    }
    QSet<QString> seen;
    for (const auto &rule : effectiveRules) {
        const auto exe = rule.executable.toLower();
        if (!processes.executables.contains(exe, Qt::CaseInsensitive) || seen.contains(exe)) continue;
        seen.insert(exe);
        // Conflicting mappings never qualify for automatic selection.
        bool conflict = false;
        for (const auto &other : effectiveRules)
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
