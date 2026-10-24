# Twitch 分類助手

在 OBS 裡搜尋、切換 Twitch 直播分類，讓直播分類跟上你的遊戲。

目前為開發預覽版，支援 Windows 11 x64，針對 OBS Studio 32.2.2 建置。正式公開發行尚未完成；未配置作者登入服務的 Preview 安裝包僅供測試。

## 關於

Twitch 分類助手是開源的 OBS 原生停駐面板。你可以搜尋分類並手動套用，也可以建立遊戲規則，讓外掛依本機執行中的遊戲提出建議。

- 按「連接 Twitch」，在 Twitch 官方網站授權。正式發行版不需申請開發者帳號或填寫 Client ID。
- 更新後向 Twitch 讀回分類，確認成功才顯示完成。
- 預設每次由你決定；也能開啟規則自動更新。
- 淺色、深色、跟隨系統，以及字型、字級和動畫設定。
- 沒有遙測或自建雲端後端。程序比對在本機完成；OAuth、搜尋與分類更新會連線到 Twitch。

<img src="docs/images/dock-preview.png" alt="Twitch 分類助手原生 OBS 面板，尚未連線狀態" width="380">

[查看外觀與字型設定畫面](docs/images/settings-preview.png)

## 安裝

### MSI 安裝（一般使用者）

公開發行後，從本專案 GitHub Releases 下載 Windows x64 `.msi`。目前尚無正式公開下載網址，請勿把 `unconfigured-preview` 當成可直接登入的正式版。

1. 先安裝 OBS Studio 32.2.2 x64，結束直播與錄影。
2. 雙擊 MSI，依照畫面安裝；需要 Windows 管理員授權。
3. 若 OBS 正在執行，依提示關閉。安裝器可以請求正常關閉，不會強制終止 OBS。
4. 完成頁可勾選啟動 OBS。首次啟動會顯示「Twitch 分類助手」面板和設定精靈。

不需安裝編譯器、Qt、WiX 或 .NET。外掛安裝於 `%ProgramData%/obs-studio/plugins/twitch-category-assistant`，不覆寫 OBS 自身的 DLL。

若之前是手動複製版本，請先備份並移走舊外掛資料夾，再安裝 MSI。MSI 版本之間可直接使用更新版升級。

### 自行編譯與封裝（進階使用者）

取得本專案原始碼，依 [作者建置指南](README.md) 安裝工具鏈並執行：

```powershell
.\scripts\build.ps1 -ClientId 'YOUR_PUBLIC_CLIENT_ID'
.\scripts\package-msi.ps1
```

自行發佈的分支需使用自己的 Public OAuth 應用程式識別碼；一般使用者安裝作者發行的 MSI 不需要做這一步。不輸入 ID 也能編譯檢查介面；登入需在配置之後使用。

若不用 MSI，關閉 OBS，把 `dist/twitch-category-assistant` 複製到上述 plugins 目錄。可攜版與非標準配置請參照 [OBS 外掛安裝說明](https://obsproject.com/kb/plugins-guide)；目前 MSI 偵測的是官方安裝版。

## 使用說明

1. 在首次精靈閱讀功能說明、條款與隱私政策，勾選同意。開發預覽版會清楚標示測試草稿。
2. 按「連接 Twitch」，在官方網站完成登入與授權；如網頁要求驗證碼，使用面板顯示的代碼。外掛不會索取 Twitch 密碼。
3. 回到 OBS，等待顯示已連線，才可繼續設定更新與通知偏好。
4. 在面板搜尋遊戲或分類，選取結果後按「套用選取分類」。即使尚未開播也能手動更新。
5. 選取分類後，可按「建立這款遊戲的規則」，指定遊戲程序名稱，例如 `game.exe`。之後外掛會依規則提供建議；未知遊戲不會擅自猜測。

「設定」可更改字型、12–22 px 字級、主題、動畫、通知與多遊戲處理方式。字型和字級會即時預覽，按「儲存」才套用。活動紀錄預設收合，點選即可展開。

自動更新必須自行開啟，並符合明確且穩定的遊戲規則。手動套用後暫停自動覆寫；按「恢復自動分類」才恢復。預設只在本機 OBS 串流期間提出自動建議，可在設定調整。

## 常見問題

### Windows 顯示智慧型應用程式控制警告，OBS 說外掛載入失敗？

目前測試版尚未數位簽章，Windows 的應用程式控制可能阻擋 DLL，即使 MSI 安裝成功也不代表 DLL 可以載入。請提供警告中的檔名與 OBS 記錄給作者；不要以關閉系統防護作為安裝步驟。正式簽章發行版尚未提供。

作者可在事件檢視器的 `Microsoft-Windows-CodeIntegrity/Operational` 查看 3077／3033 事件，核對被阻擋的檔案。若 OBS 記錄已出現 `[Twitch Category Assistant] native dock initialized`，代表該次啟動已完成本外掛初始化；其他外掛的錯誤需要另行核對。

**面板不見了？**

在 OBS「停駐視窗」選單勾選「Twitch 分類助手」。首次設定完成後會保留你儲存的面板位置，不會每次啟動都重排版面。

**為什麼登入後還不能下一步？**

請確認已在 Twitch 網頁按下授權，並等面板顯示「已連線」。關閉網頁、取消授權、過期或網路失敗都不算登入成功，可關閉登入視窗後重試。

**顯示「尚未配置作者的登入服務」？**

你使用的是作者測試用 Preview；正式發行版會內建登入設定。一般使用者不必為此註冊 Developer Console。

**看不到遊戲建議？**

先建立 `.exe` 對應分類的規則，確認未暫停建議；若啟用「僅串流時」，需要 OBS 已開始串流。多個遊戲同時執行時，依所選策略手動決定或選擇優先項。

**按登入後顯示連線中斷／TLS 元件載入失敗，網頁沒有開啟？**

0.2.1 及以前的安裝包漏了 HTTPS 所需元件。請更新至 0.2.2 或更新版，完整關閉並重新開啟 OBS，再登入。手動安裝時須一起複製 `bin` 與 `data` 資料夾。

**顯示「已接受／確認中」或「結果不確定」？**

Twitch 接受修改與外掛確認結果是兩個階段。請等讀回確認或重新整理，避免連續重複操作。遇到頻率限制請等待面板提示的時間。

**資料放在哪？**

偏好、遊戲規則與文件同意時間存於 OBS 的外掛設定目錄；Token 存於 Windows 認證管理員。活動紀錄只保留本次工作階段。外掛不傳送程序清單或收集遙測；Twitch 仍需接收授權、搜尋和分類修改資料。

**如何登出或移除？**

在外掛設定按「登出並嘗試撤銷授權」，或使用「清除本機資料」。之後關閉 OBS，在 Windows「已安裝的應用程式」移除 Twitch Category Assistant。解除安裝保留個人設定與憑證，方便重新安裝；若需完全移除資料，請先使用外掛的清除功能。也可在 Twitch 的連線設定撤銷應用程式授權。

**其他 OBS 版本、Windows 通知、自動更新？**

目前針對 OBS 32.2.2 / Qt 6.11.1；其他版本需另做相容性驗證。支援 Dock 內通知，尚無 Windows 系統通知與外掛自動更新器。

## 貢獻者

歡迎在本專案提交 Issue、重現步驟、介面建議或 Pull Request。問題回報請附 Windows/OBS/外掛版本與錯誤訊息，不要附 Token、密碼或 Stream Key。開發與驗證流程見 [作者指南](README.md)。

感謝 [OBS Project](https://obsproject.com/) 與官方 [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) 提供基礎。貢獻紀錄以本專案 Git 歷史及 GitHub Contributors 為準。

採用 [GPL-2.0-or-later](LICENSE)。這是第三方社群外掛，並非 Twitch 官方產品。
