# Twitch Category Assistant

Windows 11 x64 的 OBS 原生 Qt Widgets 分類管理 Dock。目標：OBS 32.2.2 / Qt 6.11.1；C++17。
目前為開發測試版。正式條款、正式隱私政策、真實 Twitch OAuth 與實機驗證狀態見 [驗證紀錄](docs/validation.md)。

## 建置

先依 [工具安裝說明](docs/toolchain-setup.md) 安裝 Visual Studio 2026 Build Tools、Windows SDK 26100、CMake 4.2+。

```powershell
cd D:\project\twitch-tool-project
.\scripts\build.ps1
```

腳本透過 vswhere 尋找工具，無須自行修改 PATH。首次 Configure 下載官方 OBS 32.2.2 及 2026-07-15 OBS/Qt 依賴到 `.deps`，驗證 SHA256，建立 OBS 開發函式庫，再編譯外掛、執行 核心測試、打包到 `dist`。不會自動安裝外掛。

可分開執行（Developer PowerShell，CMake 已在 PATH）：

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
# 執行測試前將 .deps/obs-deps-qt6-2026-07-15-x64/bin 加入該終端的 PATH
ctest --test-dir build_x64 -C RelWithDebInfo --output-on-failure
cmake --install build_x64 --config RelWithDebInfo --prefix dist
```

## 安裝至 OBS

1. 完全關閉 OBS。
2. 將 `dist/twitch-category-assistant` 整個資料夾複製到 `C:/ProgramData/obs-studio/plugins/`（可能需要管理員權限）。
3. 目標 DLL 為 `C:/ProgramData/obs-studio/plugins/twitch-category-assistant/bin/64bit/twitch-category-assistant.dll`。
4. 不要複製 Qt DLL 或覆寫 OBS 隨附 DLL。外掛使用 OBS 的 Qt 執行環境。
5. 啟動 OBS，在「停駐視窗」選單開啟「Twitch 分類助手」。拖曳標題列可浮動。
6. 若載入失敗，查看 OBS 日誌與實際 OBS/Qt 版本。未驗證的其他版本不保證相容。

移除：關閉 OBS 後移除此專用外掛資料夾；若要清除帳號憑證，先在外掛登出。

## Twitch Developer Console

1. 登入 https://dev.twitch.tv/console/apps ，依 Twitch 要求完成開發者帳號設定。
2. 註冊應用程式，Client Type 選 **Public**，保存 Client ID。
3. 本程式只實作 **Device Code Flow**，不使用 Client Secret、回呼伺服器或 Redirect URI。若 Console 表單仍要求 redirect 欄位，依當時表單規則填寫註冊資料；外掛不將該欄位用於登入。
4. 在外掛「設定」輸入 Client ID，不要輸入 Secret、Token 或 Twitch Stream Key。
5. 在首次精靈閱讀並勾選兩份待發布測試文件，按登入，於系統瀏覽器的 Twitch 官方頁面授權。
6. 權限為 `channel:manage:broadcast`。完成後驗證 client/user/scope，使用登入者本人頻道；不支援代管其他主播。

Token 存於 Windows 認證管理員的 `TwitchCategoryAssistant/` 項目。啟動與每小時驗證；401 時單一刷新，失敗則重新登入。登出會清除目前 Client ID 的本機憑證並嘗試撤銷遠端授權，遠端失敗會明確顯示。

## 使用

- 搜尋分類，選取後按「套用選取分類」。未直播也可使用。
- 更新狀態區分送出中、已接受／確認中、已讀回確認成功、失敗、結果不確定。
- 搜尋結果選取真實分類後，可新增對應 `.exe` 檔名的遊戲規則。
- 設定可編輯程序名稱／優先值及刪除規則。分類 ID 與名稱來自 API；改分類請刪除後重新新增。
- 預設每次詢問、多遊戲手動選擇、Dock 通知、僅本機 OBS 串流時提供建議。
- 自動更新需明確規則、候選穩定至少 9 秒、距上次自動更新至少 60 秒，且分類不同。
- 同一候選的自動修改不無限重試；網路失敗請手動確認或重新啟用自動分類。
- 手動套用會鎖住自動覆寫，按「恢復自動分類」才解除。
- 忽略建議直到該分類的候選不再出現。未知遊戲不猜測、不自動修改。
- 系統／淺／深三種主題；設定精靈可重新開啟。

## 資料與隱私

偏好、映射與兩份文件的版本／UTC 同意時間保存在 OBS 回傳的外掛設定目錄 `settings.json`。
不保存 Token 到設定檔、不記錄 Token、不上傳程序清單、不收集遙測、不讀取遊戲記憶體。
活動只保留本次工作階段最多 100 筆。損壞設定第一次重存前會備份原檔。
正式法律文件尚待提供；內建文件皆明確標示測試草稿，版本更換會重新要求同意。

## 結構

- `src/plugin/`：OBS 生命週期與 Dock
- `src/ui/`：原生 Dock、設定、規則、首次精靈與文件閱讀
- `src/twitch/`：Device OAuth、Helix、更新確認、錯誤與頻率限制
- `src/detection/`：Windows 程序掃描與候選政策
- `src/security/`：Windows Credential Manager
- `src/settings/`：設定驗證與原子儲存
- `tests/`：核心測試 與非網路 Mock transport
- `scripts/`：建置與經授權的工具安裝
- `docs/`：決策、安裝、測試與限制

## 常見問題與限制

- 空白搜尋結果：必須先登入，搜尋使用真實 Twitch API，不提供假結果。
- 自動建議沒有出現：檢查映射、暫停、僅直播選項、程序名稱與多候選政策。OBS 串流事件不代表 Twitch 遠端一定已上線。
- 登入失敗：確認 Public Client Type、Client ID、權限及時間／網路。Device Code Flow 沒有本機 redirect 設定。
- 429：遵守服務回傳的重設時間；回傳無有效時間時至少等待 60 秒。
- 204：只表示修改已接受；讀回 ID 相符後才顯示成功。
- 封面讀取失敗不影響分類功能，顯示無封面。只允許 Twitch 官方圖片 CDN，限制影像大小。
- Windows 系統通知：本版未實作，設定內停用標示；OBS Dock 通知可用。
- 程序名稱可能重複／偽裝，映射不是防作弊或身份驗證；不辨識所有遊戲、不注入程序、不要求管理員執行 OBS。
- 憑證保護依賴目前 Windows 使用者帳號；無法防止已控制該帳號的程式。
- 不含自動更新器、雲端服務或發布版簽章。

## 版本控制與來源

衍生自 OBS 官方 obs-plugintemplate，保留 GPL-2.0-or-later 授權。模板基準與決策見 docs/decisions.md。
本機 `main` 為專案分支，`upstream` 指向官方模板；未新增公開遠端或推送。

官方參考：
- https://github.com/obsproject/obs-studio/blob/32.2.2/CMakePresets.json
- https://dev.twitch.tv/docs/authentication/getting-tokens-oauth/#device-code-grant-flow
- https://dev.twitch.tv/docs/authentication/validate-tokens/
- https://dev.twitch.tv/docs/api/reference/
- https://docs.obsproject.com/reference-frontend-api

