#include "updater.hpp"
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QVersionNumber>
#include <QDir>
#include <QCoreApplication>
#include <QProcess>
#include <QFileInfo>
#include <QUuid>
#include <windows.h>
#include <wintrust.h>
#include <softpub.h>
namespace tca {
bool validUpdateRepository(const QString &r)
{
    return r.size() <= 200 && QRegularExpression("^[A-Za-z0-9][A-Za-z0-9_.-]*/[A-Za-z0-9][A-Za-z0-9_.-]*$").match(r).hasMatch() && r.compare("obsproject/obs-plugintemplate", Qt::CaseInsensitive) != 0;
}
static bool secureUrl(const QUrl &url) { return url.isValid() && url.scheme() == "https" && url.userInfo().isEmpty() && url.port() == -1 && url.fragment().isEmpty(); }
bool updateDownloadUrl(const QUrl &url)
{
    return secureUrl(url) && (url.host() == "github.com" || url.host() == "release-assets.githubusercontent.com" || url.host() == "objects.githubusercontent.com");
}
bool parseRelease(const QByteArray &data, const QString &repository, const QString &current, Release *release, QString *error)
{
    *release = {}; *error = "GitHub 回應格式或更新檔驗證資料不完整";
    if (data.size() > 2 * 1024 * 1024 || !validUpdateRepository(repository)) return false;
    const auto doc = QJsonDocument::fromJson(data); const auto o = doc.object();
    if (!doc.isObject() || !o["draft"].isBool() || !o["prerelease"].isBool() || o["draft"].toBool() || o["prerelease"].toBool()) return false;
    const auto tag = o["tag_name"].toString(); const auto match = QRegularExpression("^v?([0-9]{1,5}\\.[0-9]{1,5}\\.[0-9]{1,5})$").match(tag);
    if (!match.hasMatch()) return false;
    const auto version = match.captured(1);
    if (QVersionNumber::fromString(version) <= QVersionNumber::fromString(current)) { error->clear(); return true; }
    const auto filename = "twitch-category-assistant-" + version + "-windows-x64.msi";
    for (const auto &entry : o["assets"].toArray()) {
        const auto asset = entry.toObject(); if (asset["name"].toString() != filename) continue;
        const auto digest = asset["digest"].toString(); const auto size = asset["size"].toDouble(); const QUrl url(asset["browser_download_url"].toString());
        if (!QRegularExpression("^sha256:[0-9a-fA-F]{64}$").match(digest).hasMatch() || size < 64 || size > 100 * 1024 * 1024 || size != static_cast<qint64>(size) || !secureUrl(url) || url.host() != "github.com" || !url.query().isEmpty() || url.path() != "/" + repository + "/releases/download/" + tag + "/" + filename) return false;
        if (!release->version.isEmpty()) return false;
        *release = {version, o["body"].toString().left(16000), filename, digest.mid(7).toLower(), QUrl("https://github.com/" + repository + "/releases/tag/" + tag), url, static_cast<qint64>(size)};
    }
    if (release->version.isEmpty()) return false;
    error->clear(); return true;
}
Updater::Updater(QString repository, QString current, QObject *parent, QNetworkAccessManager *transport)
    : QObject(parent), repository(std::move(repository)), current(std::move(current)), network(transport ? transport : new QNetworkAccessManager(this))
{
    status = configured() ? "尚未檢查更新" : "作者尚未配置 GitHub 更新來源";
    deadline.setSingleShot(true); connect(&deadline, &QTimer::timeout, this, &Updater::cancel);
}
void Updater::cancel()
{
    deadline.stop();
    if (reply) { disconnect(reply, nullptr, this, nullptr); reply->abort(); reply->deleteLater(); reply = nullptr; }
    output.reset(); status = "已取消或連線逾時"; emit changed();
}
void Updater::check()
{
    if (busy() || !configured()) return;
    latest = {}; downloaded.clear(); status = "正在向 GitHub 檢查穩定版本…";
    QNetworkRequest request(QUrl("https://api.github.com/repos/" + repository + "/releases/latest"));
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setRawHeader("Accept", "application/vnd.github+json"); request.setRawHeader("User-Agent", "Twitch-Category-Assistant"); request.setRawHeader("X-GitHub-Api-Version", "2022-11-28"); request.setTransferTimeout(15000); request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    reply = network->get(request); auto *active = reply.data(); deadline.start(20000); emit changed();
    connect(active, &QNetworkReply::readyRead, this, [active] { if (active->bytesAvailable() > 2 * 1024 * 1024) active->abort(); });
    connect(active, &QNetworkReply::finished, this, [this, active] {
        deadline.stop(); reply = nullptr; active->deleteLater(); const int code = active->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString error;
        if (code == 404) status = "尚無公開穩定版本，或儲存庫無法存取";
        else if (code == 403 || code == 429) status = "GitHub 暫時限制請求，請稍後再試";
        else if (active->error() != QNetworkReply::NoError || code != 200) status = "無法連線 GitHub，請檢查網路或稍後重試";
        else if (!parseRelease(active->readAll(), repository, current, &latest, &error)) { latest = {}; status = error; }
        else status = latest.version.isEmpty() ? "目前已是最新穩定版本" : "有新版本：" + latest.version;
        emit changed();
    });
}
void Updater::download(const QString &directory)
{
    if (busy() || latest.version.isEmpty() || !updateDownloadUrl(latest.download)) return;
    if (!QDir().mkpath(directory)) { status = "無法建立更新下載目錄"; emit changed(); return; }
    target = QDir(directory).filePath(latest.filename); output = std::make_unique<QSaveFile>(target);
    if (!output->open(QIODevice::WriteOnly)) { finishDownload("無法寫入更新檔"); return; }
    received = 0; hash.reset(); downloaded.clear(); status = "正在下載更新…"; deadline.start(120000); fetch(latest.download, 0);
}
void Updater::consume()
{
    if (!reply || !output || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) return;
    const auto bytes = reply->readAll(); received += bytes.size();
    if (received > latest.size || received > 100 * 1024 * 1024 || output->write(bytes) != bytes.size()) { reply->abort(); return; }
    hash.addData(bytes); emit progress(received, latest.size);
}
void Updater::fetch(const QUrl &url, int redirects)
{
    if (!updateDownloadUrl(url) || redirects > 5) { finishDownload("下載重新導向不受信任或次數過多"); return; }
    QNetworkRequest request(url); request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false); request.setRawHeader("User-Agent", "Twitch-Category-Assistant"); request.setTransferTimeout(20000); request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    reply = network->get(request); auto *active = reply.data(); emit changed();
    connect(active, &QNetworkReply::readyRead, this, &Updater::consume);
    connect(active, &QNetworkReply::finished, this, [this, active, redirects] {
        consume(); const int code = active->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto redirect = active->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl(); reply = nullptr; active->deleteLater();
        if (code >= 300 && code < 400 && !redirect.isEmpty()) { fetch(active->url().resolved(redirect), redirects + 1); return; }
        if (active->error() != QNetworkReply::NoError || code != 200 || received != latest.size || QString::fromLatin1(hash.result().toHex()) != latest.digest) { finishDownload("更新下載失敗或 SHA-256 不符；未保留不完整檔案"); return; }
        if (!output->commit()) { finishDownload("無法儲存下載檔"); return; }
        downloaded = target; finishDownload({});
    });
}
void Updater::finishDownload(const QString &error)
{
    deadline.stop(); output.reset();
    if (!error.isEmpty()) status = error;
    else {
        WINTRUST_FILE_INFO file{}; file.cbStruct = sizeof(file); file.pcwszFilePath = reinterpret_cast<LPCWSTR>(downloaded.utf16());
        WINTRUST_DATA trust{}; trust.cbStruct = sizeof(trust); trust.dwUIChoice = WTD_UI_NONE; trust.dwUnionChoice = WTD_CHOICE_FILE; trust.pFile = &file; trust.dwStateAction = WTD_STATEACTION_VERIFY; trust.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;
        GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2; const LONG result = WinVerifyTrust(nullptr, &action, &trust); trust.dwStateAction = WTD_STATEACTION_CLOSE; WinVerifyTrust(nullptr, &action, &trust);
        status = "SHA-256 已核對。" + QString(result == ERROR_SUCCESS ? "Windows 簽章有效。" : "簽章未通過本機驗證，Windows 仍可能阻擋安裝。") + "正在準備關閉 OBS 並安裝更新…";
    }
    emit changed();
    if (error.isEmpty()) emit downloadVerified();
}
bool Updater::launchInstaller(QString *error)
{
    if (busy() || downloaded.isEmpty() || latest.digest.isEmpty()) { *error = "更新檔尚未驗證完成"; return false; }
    QFile source(":/tca/install-update.ps1");
    const auto helper = QDir(QFileInfo(downloaded).absolutePath()).filePath("install-update.ps1");
    QSaveFile script(helper);
    if (!source.open(QIODevice::ReadOnly) || !script.open(QIODevice::WriteOnly)) { *error = "無法準備更新安裝程序；MSI 已保留"; return false; }
    const auto bytes = source.readAll();
    if (script.write(bytes) != bytes.size() || !script.commit()) { *error = "無法儲存更新安裝程序；MSI 已保留"; return false; }
    const auto powershell = QDir(qEnvironmentVariable("SystemRoot")).filePath("System32/WindowsPowerShell/v1.0/powershell.exe");
    const auto eventName = "Local\\TCA-Update-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, reinterpret_cast<LPCWSTR>(eventName.utf16()));
    if (!ready) { *error = "無法準備更新程序確認；OBS 將保持開啟"; return false; }
    const auto proceedName = eventName + "-continue";
    HANDLE proceed = CreateEventW(nullptr, TRUE, FALSE, reinterpret_cast<LPCWSTR>(proceedName.utf16()));
    if (!proceed) { CloseHandle(ready); *error = "無法準備更新程序確認；OBS 將保持開啟"; return false; }
    QProcess process; process.setProgram(powershell);
    process.setArguments({"-NoProfile", "-NonInteractive", "-ExecutionPolicy", "RemoteSigned", "-File", helper,
        "-ObsProcessId", QString::number(QCoreApplication::applicationPid()), "-ObsExecutable", QCoreApplication::applicationFilePath(),
        "-Installer", QFileInfo(downloaded).absoluteFilePath(), "-Digest", latest.digest, "-ReadyEvent", eventName, "-ProceedEvent", proceedName});
    process.setWorkingDirectory(QFileInfo(downloaded).absolutePath());
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) { args->flags |= CREATE_NO_WINDOW; });
    if (!process.startDetached()) { CloseHandle(ready); CloseHandle(proceed); *error = "無法啟動更新安裝程序；請從下載資料夾手動安裝"; return false; }
    const auto acknowledged = WaitForSingleObject(ready, 10000);
    const bool permitted = acknowledged == WAIT_OBJECT_0 && SetEvent(proceed);
    CloseHandle(ready); CloseHandle(proceed);
    if (!permitted) { *error = "安裝助手未能啟動或被 Windows 政策阻擋；OBS 保持開啟，MSI 已保留"; return false; }
    error->clear(); return true;
}
}
