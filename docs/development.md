# Twitch Category Assistant — 開發與發佈指南

這份文件供作者與貢獻者使用。一般使用者請閱讀 [README](../README.md)。

Windows 11 x64 原生 OBS Dock，C++17 / Qt Widgets；目前針對 OBS 32.2.2、Qt 6.11.1 建置。其他版本需自行驗證相容性。

## OAuth：由發佈者配置，一般使用者直接授權

仍使用 OAuth 的 **Public Client Device Code Flow**。正式發行的 DLL 內建公開 Client ID；使用者只需按「連接 Twitch」，在 Twitch 官方網頁登入授權，再返回精靈。使用者不需要 Developer Console、Client Secret 或自行編譯。

發佈者只需在 [Twitch Developer Console](https://dev.twitch.tv/console/apps) 註冊一次 Public 類型應用程式，建置時傳入 Client ID。這是應用程式識別碼，不是密鑰；可以存在公開二進位檔。不要把 Client Secret、Token 或 Stream Key 放入建置參數。

權限為 `channel:manage:broadcast`。登入後驗證 client/user/scope；啟動及每小時驗證 Token，401 時串行刷新。Token 存在目前 Windows 使用者的認證管理員。未完成有效登入，精靈不能進入下一步或標記完成。

未配置 ID 的開發版會顯示說明；作者可在「設定 → 進階與帳號 → 開發者選項」暫時填入 ID。這個測試選項不出現在已內建 ID 的發行版。正式 MSI 封裝命令會拒絕未配置 ID 的建置；`-Preview` 僅供作者檢查安裝流程。

## 建置

依 [工具鏈說明](toolchain-setup.md) 安裝 Visual Studio 2026 Build Tools、Windows SDK 26100、CMake 4.2+。一般使用者不需這些工具。

```powershell
# 發佈者：將以下值換成自己的 Public Client ID
.\scripts\build.ps1 -ClientId 'YOUR_PUBLIC_CLIENT_ID'

# 本地開發可不指定 ID；保留 CMake cache 內已配置的值
.\scripts\build.ps1
# 明確清除內建 ID：.\scripts\build.ps1 -ClientId ''
```

腳本下載並驗證官方 OBS 32.2.2 / OBS dependencies 2026-07-15，編譯、執行測試，將 DLL 與資料放進 `dist/twitch-category-assistant`。執行時使用 OBS 隨附的 Qt 執行環境；額外封裝匹配版本的 `qschannelbackend.dll` 至 `data/qt-plugins/tls`，供 HTTPS 使用，並附其授權與來源資訊。OBS 的 Qt Core／Network／Widgets DLL 不會被替換。

## 產生 MSI

```powershell
# 已配置 Client ID 並完成建置後
.\scripts\package-msi.ps1
# 作者目前尚未配置 ID 時，只產生明確標示的測試安裝包
.\scripts\package-msi.ps1 -Preview
```

封裝需要 .NET 8 Runtime x64 或更新相容版本，不需要 .NET SDK。腳本下載固定 SHA256 的 WiX 4.0.6 和 UI/Util 擴充到 `.tools/wix`，不全域安裝。使用者執行 MSI 不需要 .NET、WiX 或編譯器。

輸出在 `dist/installer/`，含 MSI 與 SHA256。MSI 安裝至 `%ProgramData%/obs-studio/plugins/twitch-category-assistant`，支援升級與 Windows 已安裝應用程式解除安裝。偵測 OBS 官方安裝登錄路徑；可攜版使用手動安裝。安裝前先結束直播與錄影；安裝器偵測到 OBS 執行時要求使用者自行關閉後重試，不主動關閉或強制終止程序。完成頁可勾選啟動 OBS。靜默安裝遇到 OBS 執行中會失敗，不自動關閉 OBS。

目前安裝包未簽章，法律文件見 terms.md 與 privacy.md，公開前須確認維護者聯絡資訊。正式發布前需配置真實 ID、更新法律文件及版本、執行 Twitch/OBS/安裝升級移除驗收，並評估程式簽章與 Windows 相容性。發布時應一併提供對應來源與 GPL 授權。每次升級增加 `buildspec.json` 的三段版本號；保持 installer 的 UpgradeCode 固定。

Windows Smart App Control 會檢查被 OBS 載入的 DLL，不只 MSI。正式發行需向受信任的憑證機構／簽章服務取得程式碼簽章身分；本機自簽憑證不等於受信任的公開發行簽章。依序簽署外掛 DLL（build 與 dist 必須維持相同檔案）、封裝的 TLS DLL，再產生及簽署 MSI，最後重新產生 MSI SHA256。使用 SHA256 摘要與可信時間戳，並以 Windows SDK SignTool `verify /pa /v` 檢查每個檔案。仍須在啟用 Smart App Control 的測試機驗收；不能宣稱簽章即保證通過所有組織政策。

參考：[Microsoft Smart App Control 簽章說明](https://learn.microsoft.com/windows/apps/develop/smart-app-control/code-signing-for-smart-app-control)。請勿將關閉 Windows 安全防護加入安裝流程。

## 手動安裝與測試

關閉 OBS，複製 `dist/twitch-category-assistant` 至 `%ProgramData%/obs-studio/plugins/`。`scripts/install-plugin.ps1` 可協助第一次複製，但遇到既有目錄會停止，避免覆蓋未確認的版本。舊的手動安裝轉 MSI 前請先備份並移走舊外掛目錄。

首次啟動 OBS 時會將 Dock 停駐在右側並顯示設定精靈。完成後尊重使用者保存的版面；未完成精靈則下次啟動再次提示。0.5.3 起保留舊版已完成狀態，文件更新只顯示獨立同意確認，不重跑整個精靈。

```powershell
# build.ps1 已設定 Qt PATH 並執行下面兩項 CTest
# tca-tests：設定、程序映射、OAuth、API 更新與錯誤分支
# tca-ui-check：實際 Qt widgets、同意與登入必填、字級儲存與截圖
```

實際 Twitch 測試必須由使用者在官方網站授權，不能由 Mock 測試代替。OBS 載入與 MSI 安裝、升級、修復及解除安裝也須另做實機驗證。

`tca-tls-check` 會在清除 SDK 的 Qt 插件搜尋路徑後檢查 TLS 封裝。需要實際測試取得 Twitch 授權網址時，將 OBS 的 `bin/64bit` 加入目前終端 PATH，再執行：

```powershell
.\build_x64\RelWithDebInfo\tca-tls-check.exe .\dist\twitch-category-assistant\data\qt-plugins --device
```

此檢查使用已內建的 Client ID，只取得裝置授權網址，不開瀏覽器、不輪詢、不保存 Token。發佈安裝包時必須包含整個 data 目錄，單獨複製 DLL 會漏掉 TLS 元件。

## 專案結構

- `src/plugin/`：OBS 生命週期、首次停駐與串流事件。
- `src/ui/`：Dock、設定、規則、OAuth 精靈與文件。
- `src/twitch/`：Device OAuth、Helix、更新後讀回、頻率限制。
- `src/detection/`：本機程序與映射規則；不注入遊戲。
- `src/security/`：Windows Credential Manager。
- `src/settings/`：偏好、文件同意與原子儲存。
- `tests/`：核心與 UI 回歸測試；Mock 僅在測試使用。
- `installer/`、`scripts/`：MSI 規格及建置封裝。

## Git 與官方參考

衍生自 OBS 官方 [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate)，保留 GPL-2.0-or-later。官方專案儲存庫為 [Chizakuraowob/obs-twitch-auto-category](https://github.com/Chizakuraowob/obs-twitch-auto-category)，開發分支為 main；本機 upstream 可保留指向官方模板。

- [Twitch Device Code OAuth](https://dev.twitch.tv/docs/authentication/getting-tokens-oauth/#device-code-grant-flow)
- [Token 驗證](https://dev.twitch.tv/docs/authentication/validate-tokens/)
- [Twitch API](https://dev.twitch.tv/docs/api/reference/)
- [OBS Frontend API](https://docs.obsproject.com/reference-frontend-api)
- [OBS 32.2.2 建置設定](https://github.com/obsproject/obs-studio/blob/32.2.2/CMakePresets.json)

0.3.0 的本機遊戲庫辨識與可選聯網查詢，詳見 [資料處理清單](data-processing.md)。發布條款與隱私政策時請依此清單確認來源、傳送項目、保存、刪除及第三方責任；尚未證明涵蓋市場多數遊戲。

0.4.0 新增通用視窗辨識、一次確認後保存、忽略及規則匯入／匯出，不需替新遊戲新增程式規則。設定與資料處理清單已更新；發布前應檢查新版同意草稿、匯出隱私提示及實際遊戲辨識。

## 發布與更新來源

更新來源由 CMake `TCA_UPDATE_REPOSITORY` 固定為 `Chizakuraowob/obs-twitch-auto-category`；衍生專案應改成自己的儲存庫，留空可停用。檢查 `/releases/latest`，只接受 `vX.Y.Z` 或 `X.Y.Z` 的穩定版。Release 請附上 `twitch-category-assistant-X.Y.Z-windows-x64.msi`；GitHub API 必須提供該資產的 SHA-256 digest 與大小，否則拒絕內建下載。打包腳本亦產生 `.sha256` 供人工核對。

0.5.8 起，使用者按下載、驗證並安裝後，外掛核對雜湊及本機 Windows 簽章狀態，確認 OBS 輸出已停止並正常關閉 OBS；本機助手等待退出、重新核對雜湊，再開啟 MSI 安裝精靈。成功才刪除 MSI，取消或失敗時保留。尚未配置作者身分釘選；同一 GitHub 來源提供的雜湊不等同作者身分保證，Windows 的權限與安全限制仍適用。發布前仍需測試簽章、升級與實際安裝；一般 CI 不會建立 Release，草稿需由維護者手動選擇版本 tag 啟動。

MSI 原生檢查工具不依賴 Qt，不發送關閉訊息或終止程序。`tca-installer-test.exe MSI_PATH CHECK_DLL_PATH FIXTURE_EXE_PATH` 在真實 MSI session 中直接呼叫程序檢查函式，測試靜默阻擋、取消、重試，不執行檔案安裝。假 OBS fixture 只等待本測試的結束事件，不是實際 OBS。

## 自訂產品資訊

MSI／DLL 名稱、作者、信箱、版權、描述與安裝文案設定在根目錄 `buildspec.json`；更改後重新建置和封裝。`version` 為三段產品版本（上限 `255.255.65535`），`revision` 為 DLL 四段版號的末段（0–65535）。新版 MSI 必須提高 `version`，只改 `revision` 不會觸發新版升級。不要變更 `name` 或安裝器的 UpgradeCode，以免影響既有設定與升級身分。MSI 的修訂編號是打包工具產生的 PackageCode GUID，不是 DLL 的 `revision`。

## CI、來源封裝與簽章

GitHub 工作流程、作者端變數與草稿 Release 操作見 [ci-release.md](ci-release.md)。本機執行 `scripts/package-sources.ps1` 可收集已提交版本的對應來源、第三方來源、SBOM 及雜湊清單；工作目錄必須乾淨。

目前未取得正式程式碼簽章，Smart App Control 可能封鎖外掛 DLL。安裝器不自動信任自簽憑證或變更系統防護；CI 建置成功不代表已相容 SAC。

