#include "detector.hpp"
#include <QSet>
#include <QHash>
#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QMutex>
#include <QElapsedTimer>
#include <QCryptographicHash>
#include <algorithm>
#include <windows.h>
#include <tlhelp32.h>
namespace tca {
QString suggestionKey(const Candidate &candidate) { return candidate.executable.toLower() + "/" + candidate.id; }
void SuggestionSession::update(const Processes &processes, const QVector<Candidate> &detected, qint64 now, int delay)
{
    if (!processes.available) { pause(); return; }
    for (const auto &candidate : detected) games.insert(candidate.executable.toLower());
    bool running = false;
    const auto tracked = games;
    for (const auto &exe : tracked) {
        if (processes.executables.contains(exe, Qt::CaseInsensitive)) { running = true; missingSince.remove(exe); }
        else {
            if (!missingSince.contains(exe)) missingSince.insert(exe, now);
            // A missing window is not an exit; require a sustained absence from the process snapshot.
            if (now - missingSince.value(exe) >= 60) {
                games.remove(exe); missingSince.remove(exe);
                for (auto it = handled.begin(); it != handled.end();) {
                    if (it->startsWith(exe + "/")) it = handled.erase(it); else ++it;
                }
            }
        }
    }
    if (!detected.isEmpty()) { played = true; idleNotified = false; }
    if (!played || running || !detected.isEmpty() || !processes.localGames.isEmpty()) { pause(); return; }
    if (idleSince < 0) idleSince = now;
    idleReady = now - idleSince >= std::clamp(delay, 15, 600);
}
static QByteArray boundedFile(const QString &path)
{
    QFile file(path); if (!file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024) return {};
    return file.readAll();
}
static QStringList vdfValues(const QString &text, const QString &key)
{
    const QRegularExpression field("\"" + QRegularExpression::escape(key) + "\"\\s*\"((?:\\\\.|[^\"\\\\])*)\"");
    QStringList values; auto matches = field.globalMatch(text);
    while (matches.hasNext()) { auto value = matches.next().captured(1); value.replace("\\\\", "\\"); value.replace("\\\"", "\""); values.append(value); }
    return values;
}
static QString localDirectory(const QString &path)
{
    const auto clean = QDir::fromNativeSeparators(path);
    if (!QRegularExpression("^[A-Za-z]:/").match(clean).hasMatch() || GetDriveTypeW(reinterpret_cast<LPCWSTR>(clean.left(3).utf16())) == DRIVE_REMOTE) return {};
    return QFileInfo(clean).isDir() ? QFileInfo(clean).canonicalFilePath() : QString{};
}
QVector<InstalledGame> readSteamLibrary(const QString &steamRoot)
{
    QVector<InstalledGame> games; const auto root = localDirectory(steamRoot); if (root.isEmpty()) return games;
    QStringList libraries{root}; libraries.append(vdfValues(QString::fromUtf8(boundedFile(root + "/steamapps/libraryfolders.vdf")), "path")); libraries.removeDuplicates();
    QSet<QString> ids;
    for (const auto &library : libraries) {
        const auto base = localDirectory(library); if (base.isEmpty()) continue;
        const QDir apps(base + "/steamapps");
        for (const auto &file : apps.entryList({"appmanifest_*.acf"}, QDir::Files)) {
            const auto text = QString::fromUtf8(boundedFile(apps.filePath(file)));
            const auto id = vdfValues(text, "appid"), name = vdfValues(text, "name"), folder = vdfValues(text, "installdir");
            if (id.size() != 1 || name.size() != 1 || folder.size() != 1 || !validCategoryId(id[0]) || name[0].trimmed().isEmpty() || name[0].size() > 256 ||
                folder[0].contains('/') || folder[0].contains('\\') || folder[0] == "." || folder[0] == "..") continue;
            const auto path = localDirectory(apps.filePath("common/" + folder[0]));
            if (path.isEmpty() || ids.contains(id[0]) || games.size() >= 10000) continue;
            ids.insert(id[0]); games.append({"steam:" + id[0], name[0].trimmed(), path});
        }
    }
    return games;
}
QVector<InstalledGame> readEpicLibrary(const QString &manifestDirectory)
{
    QVector<InstalledGame> games; const auto path = localDirectory(manifestDirectory); if (path.isEmpty()) return games;
    QDir directory(path); QSet<QString> ids;
    for (const auto &file : directory.entryList({"*.item"}, QDir::Files)) {
        const auto item = QJsonDocument::fromJson(boundedFile(directory.filePath(file))).object();
        const auto id = item["AppName"].toString(), name = item["DisplayName"].toString().trimmed(), location = localDirectory(item["InstallLocation"].toString());
        if (!validLibraryKey("epic:" + id) || name.isEmpty() || name.size() > 256 || location.isEmpty() || ids.contains(id) || games.size() >= 10000) continue;
        ids.insert(id); games.append({"epic:" + id, name, location});
    }
    return games;
}
static QVector<InstalledGame> installedGames()
{
    static QMutex mutex; const QMutexLocker lock(&mutex);
    static QElapsedTimer age; static QVector<InstalledGame> cached;
    if (age.isValid() && age.elapsed() < 60000) return cached;
    QSettings steam("HKEY_CURRENT_USER\\Software\\Valve\\Steam", QSettings::NativeFormat);
    cached = readSteamLibrary(steam.value("SteamPath").toString());
    cached += readEpicLibrary(qEnvironmentVariable("ProgramData") + "/Epic/EpicGamesLauncher/Data/Manifests");
    for (const auto &key : {"HKEY_LOCAL_MACHINE\\SOFTWARE\\GOG.com\\Games", "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\GOG.com\\Games"}) {
        QSettings gog(key, QSettings::NativeFormat);
        for (const auto &id : gog.childGroups()) {
            const auto path = localDirectory(gog.value(id + "/path").toString()), name = gog.value(id + "/gameName").toString().trimmed();
            if (validLibraryKey("gog:" + id) && !path.isEmpty() && !name.isEmpty() && name.size() <= 256 && cached.size() < 20000) cached.append({"gog:" + id, name, path});
        }
    }
    age.start(); return cached;
}
InstalledGame matchInstalledGame(const QString &executablePath, const QVector<InstalledGame> &games, bool excludeHelpers)
{
    const QFileInfo file(executablePath); const auto name = file.fileName().toLower();
    if (!name.endsWith(".exe") || (excludeHelpers && QRegularExpression("launcher|crash|report|unins|setup|helper|anticheat").match(name).hasMatch())) return {};
    const auto path = file.canonicalFilePath(); if (path.isEmpty()) return {};
    InstalledGame match;
    for (const auto &game : games) {
        const auto root = QDir::fromNativeSeparators(game.directory);
        if (path.startsWith(root + '/', Qt::CaseInsensitive)) {
            if (root.size() == match.directory.size() && match.key != game.key) return {};
            if (root.size() > match.directory.size()) match = game;
        }
    }
    return match;
}
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
RunningGame recognizeStandaloneGame(const QString &executable, const QString &windowTitle)
{
    const auto exe = executable.toLower(), title = windowTitle.trimmed();
    // Protected game processes may deny path reads; require both known executable and game title.
    if (exe == "htgame.exe" && (title == "NTE" || title == "異環" || title == "异环" || title == "NTE: Neverness to Everness"))
        return {"builtin:nte", "NTE: Neverness to Everness", exe};
    return {};
}
bool canLearnWindow(const QString &executable)
{
    const auto exe = executable.toLower();
    static const QSet<QString> excluded{"obs64.exe", "explorer.exe", "dwm.exe", "taskmgr.exe", "chrome.exe", "msedge.exe", "firefox.exe", "brave.exe", "opera.exe", "cmd.exe", "powershell.exe", "pwsh.exe", "windowsterminal.exe", "code.exe", "codex.exe", "devenv.exe", "searchhost.exe", "applicationframehost.exe"};
    return validExecutable(exe) && !excluded.contains(exe) && !QRegularExpression("launcher|crash|report|unins|setup|helper|anticheat").match(exe).hasMatch();
}
QString windowKey(const WindowEvidence &w)
{
    // Include title for pending confirmations, even when a path is readable.
    return recognitionKey({w.executable, w.windowClass, w.title.simplified(), w.pathHash, {}, {}});
}
int matchRecognition(const WindowEvidence &w, const QVector<RecognitionRule> &rules)
{
    int found = -1; const auto title = w.title.simplified();
    for (int i = 0; i < rules.size(); ++i) {
        const auto &r = rules[i];
        if (!validRecognitionRule(r) || r.executable.compare(w.executable, Qt::CaseInsensitive) != 0 || r.windowClass != w.windowClass) continue;
        if (!r.pathHash.isEmpty() && r.pathHash != w.pathHash) continue;
        if (!r.title.isEmpty() && (r.titlePrefix ? !title.startsWith(r.title, Qt::CaseInsensitive) : title.compare(r.title, Qt::CaseInsensitive) != 0)) continue;
        if (found >= 0 && (rules[found].categoryId != r.categoryId || rules[found].ignored != r.ignored)) return -2;
        found = i;
    }
    return found;
}
QVector<Rule> learnedCandidates(const QVector<WindowEvidence> &windows, const QVector<RecognitionRule> &rules)
{
    QVector<Rule> result;
    QSet<QString> blocked;
    for (const auto &w : windows) {
        const int match = matchRecognition(w, rules);
        if (match < 0 || rules[match].ignored) { blocked.insert(w.executable.toLower()); continue; }
        const auto &r = rules[match]; result.append({w.executable, r.categoryId, r.categoryName, 0});
    }
    result.erase(std::remove_if(result.begin(), result.end(), [&](const Rule &r) { return blocked.contains(r.executable.toLower()); }), result.end());
    return result;
}
static QString processPath(DWORD pid)
{
    QString result; const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process) {
        wchar_t path[32768]{}; DWORD length = 32768;
        if (QueryFullProcessImageNameW(process, 0, path, &length)) result = QDir::fromNativeSeparators(QString::fromWCharArray(path, length));
        CloseHandle(process);
    }
    return result;
}
struct GameWindows {
    QHash<DWORD, QString> processes; QHash<DWORD, DWORD> parents;
    QVector<Rule> games; QVector<InstalledGame> installed; QVector<RunningGame> running; QVector<WindowEvidence> windows;
    QStringList learnedExecutables; HWND foreground = nullptr; bool libraries = false, inspectForeground = true, inspectAll = false;
};
static BOOL CALLBACK inspectGameWindow(HWND window, LPARAM parameter)
{
    if (!IsWindowVisible(window)) return TRUE;
    auto &context = *reinterpret_cast<GameWindows *>(parameter);
    DWORD pid = 0; GetWindowThreadProcessId(window, &pid);
    const auto exe = context.processes.value(pid);
    if (exe.isEmpty()) return TRUE;
    const bool inspect = context.learnedExecutables.contains(exe, Qt::CaseInsensitive) ||
        ((context.inspectAll || (context.inspectForeground && window == context.foreground)) && canLearnWindow(exe));
    const auto path = (inspect || !context.installed.isEmpty()) ? processPath(pid) : QString{};
    if (inspect && pid != GetCurrentProcessId()) {
        wchar_t title[257]{}, windowClass[257]{}; GetWindowTextW(window, title, 257); GetClassNameW(window, windowClass, 257);
        WindowEvidence evidence; evidence.executable = exe; evidence.title = QString::fromWCharArray(title).simplified(); evidence.windowClass = QString::fromWCharArray(windowClass);
        if (!path.isEmpty()) evidence.pathHash = QString::fromLatin1(QCryptographicHash::hash(QDir::cleanPath(path).toCaseFolded().toUtf8(), QCryptographicHash::Sha256).toHex());
        const auto parent = context.parents.value(pid); evidence.parentExecutable = context.processes.value(parent);
        evidence.foreground = window == context.foreground;
        // Launcher lineage is only a hint for the confirmation UI, never an automatic match.
        if (evidence.foreground && !context.installed.isEmpty()) evidence.suggestedName = matchInstalledGame(processPath(parent), context.installed, false).name;
        if (!evidence.windowClass.isEmpty() && evidence.title.size() >= 2 && GetWindowTextLengthW(window) <= 256) context.windows.append(evidence);
    }
    if (context.libraries && exe == "htgame.exe") {
        wchar_t title[512]{}; GetWindowTextW(window, title, 512);
        const auto game = recognizeStandaloneGame(exe, QString::fromWCharArray(title));
        if (!game.key.isEmpty()) {
            bool duplicate = false; for (const auto &old : context.running) if (old.key == game.key) duplicate = true;
            if (!duplicate) context.running.append(game);
            return TRUE;
        }
    }
    if (exe == "javaw.exe" || exe == "java.exe" || exe == "minecraft.windows.exe") {
        wchar_t title[512]{}; GetWindowTextW(window, title, 512);
        const auto game = recognizeGame(exe, QString::fromWCharArray(title));
        if (!game.categoryId.isEmpty()) context.games.append(game);
    }
    if (!context.installed.isEmpty()) {
        if (!path.isEmpty()) {
                const auto game = matchInstalledGame(path, context.installed);
                if (!game.key.isEmpty()) {
                    bool duplicate = false; for (const auto &old : context.running) if (old.key == game.key && old.executable == exe) duplicate = true;
                    if (!duplicate) context.running.append({game.key, game.name, exe});
                }
        }
    }
    return TRUE;
}
Processes scanProcesses(bool libraries, const QStringList &learnedExecutables, bool inspectForeground, bool inspectAll)
{
    Processes result;
    GameWindows games;
    games.libraries = libraries;
    games.inspectAll = inspectAll;
    games.learnedExecutables = learnedExecutables; games.inspectForeground = inspectForeground; games.foreground = GetForegroundWindow();
    if (libraries) games.installed = installedGames();
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
            games.processes.insert(entry.th32ProcessID, exe);
            games.parents.insert(entry.th32ProcessID, entry.th32ParentProcessID);
            if (entry.th32ProcessID == foreground) result.foreground = exe;
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    EnumWindows(inspectGameWindow, reinterpret_cast<LPARAM>(&games));
    result.recognizedGames = games.games;
    result.localGames = games.running; result.libraryCount = games.installed.size();
    result.windows = games.windows;
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
        for (const auto &rule : rules) if (rule.executable.compare(game.executable, Qt::CaseInsensitive) == 0) overridden = true;
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
        if (!conflict && !rule.ignored) result.append({exe, rule.categoryId, rule.categoryName, rule.priority});
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
