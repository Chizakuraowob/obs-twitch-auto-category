# 隱私權政策

文件版本：2026-10-26-v4。適用版本：0.5.2 起。

## 維護者與適用範圍

本政策說明 Chizakura 維護的 Twitch 分類助手實際處理哪些資料。外掛沒有作者營運的資料接收後端、遙測或廣告追蹤。這不代表完全不連網：Twitch 授權、搜尋、分類操作、圖片及 GitHub更新(可選)仍需連線第三方服務。

維護者：Chizakura。私人聯絡信箱：admin@chizakura.dev。文件生效日期：2026-10-26。一般問題可至 [專案 Issues](https://github.com/Chizakuraowob/obs-twitch-auto-category/issues) 回報；Issues 是公開空間，請勿提供私人資料、Token 或直播金鑰。本政策說明目前版本的資料處理方式。

## 本機讀取與用途

- 程序檔名、前景程序與符合條件的視窗標題／類別、可讀取的路徑、父程序名稱：用於遊戲辨識、確認、提醒去重與遊戲結束計時。手動新增遊戲會列出符合條件的可見視窗。
- Steam／Epic 安裝紀錄及 GOG 登錄資訊：取得本機遊戲名稱、平台 ID 及目錄，與執行中的程序比對。不讀取這些平台的登入 Token。
- 不讀取遊戲記憶體、命令列、存檔或畫面，不遞迴掃描整個磁碟，也不提高程序權限。

程序清單、原始路徑、父程序、未確認視窗標題及完整遊戲庫不會由外掛上傳。未確認資料只留於工作階段記憶體。已確認規則可保存程序、視窗類別、分類、忽略選擇，以及路徑 SHA-256 指紋或標題／字面前綴。標題可能包含私人內容，路徑指紋也不是匿名化保證。

## 保存位置與期限

- 偏好、收藏、辨識規則、遊戲庫對應快取、忽略項目、文件同意版本與 UTC 時間：保存在 OBS 提供的外掛設定目錄 `settings.json`。標準 Windows 安裝通常位於 `%APPDATA%/obs-studio/plugin_config/twitch-category-assistant`；可攜版或不同帳號可能不同。保存至你刪除或重設。
- Twitch Access／Refresh Token：保存在目前 Windows 使用者的認證管理員，不寫入設定 JSON；登入工作階段中也會存在記憶體。用於維持、驗證與刷新授權。
- 活動紀錄及辨識暫存：主要留在本次工作階段。OBS 自身的日誌另可能保存外掛版本、初始化與 TLS 可用性等診斷資訊，由 OBS 管理；回報日誌前請自行檢查內容。
- 更新下載：完成的 MSI 保存在設定目錄的 `updates` 子目錄，直到你自行刪除。取消、失敗或雜湊不符的下載不作為可用安裝檔保留。
- 設定讀取錯誤後儲存可能建立 `.backup-時間` 備份；匯出的規則 JSON 與備份檔須自行管理。

## Twitch 與圖片服務

OAuth、Token 驗證／刷新／撤銷會連線 `id.twitch.tv`，授權網頁由 Twitch 提供。`api.twitch.tv` 接收執行搜尋、查詢與更新分類所需的識別碼、搜尋文字和 OAuth 認證。分類圖片由 Twitch 提供的 CDN 位址下載。Twitch 及其服務供應者可取得 IP、請求時間及必要帳號／裝置連線資訊。

「向 Twitch 查詢遊戲名稱」預設關閉；啟用後會把偵測到的執行中遊戲名稱送到 Twitch API，以取得分類對應。關閉可停止新的自動查詢，不會撤回已送出的請求。未知視窗標題不會自動當成聯網搜尋文字；手動搜尋會送出你輸入的內容。開發者模式不會阻止所有聯網操作。

第三方資料保存與權利請求依 [Twitch 隱私政策](https://www.twitch.tv/p/en/legal/privacy-notice/) 處理，維護者不能替 Twitch 承諾保存地點或期限。

## GitHub 更新

自動檢查預設關閉，開啟後在啟動時檢查一次；手動檢查也會連線。請求前往 `api.github.com`，更新下載可經 `github.com`、`release-assets.githubusercontent.com` 或 `objects.githubusercontent.com`。GitHub 可取得 IP、User-Agent 及請求資訊；外掛不向 GitHub 傳送 Twitch Token、Client ID、遊戲、程序或視窗資料。

只有你按下下載才取得 MSI；外掛核對 API 提供的 SHA-256。Windows 簽章驗證只用本機快取，不為此額外查詢撤銷服務。GitHub 的資料處理依 [GitHub 隱私聲明](https://docs.github.com/en/site-policy/privacy-policies/github-general-privacy-statement)。瀏覽器開啟專案、聯絡或授權連結時，相關網站也會收到一般瀏覽請求。

## 控制、刪除與聯絡

可在設定關閉自動偵測、聯網名稱查詢或更新檢查；在「我的遊戲」刪除／忽略規則，在收藏區移除收藏，或清除分類快取。登出會清除目前 Client ID 的本機憑證並嘗試撤銷 Twitch 授權；網路失敗時請到 Twitch 連接管理頁確認撤銷。

「清除本機資料」重設偏好、規則與同意紀錄並處理目前帳號憑證；既有備份、匯出檔、其他 Client ID 的憑證及下載 MSI 不會全部自動刪除。解除安裝不應視為完整資料刪除，若要移除殘留資料，請關閉 OBS 後檢查設定目錄與 Windows 認證管理員。

匯出規則不含 Token 或 Client ID，但可能包含私人標題與指紋，分享前請檢查。維護者無法直接讀取、替你修改或刪除電腦內的資料。若主動提交 Issue／附件，內容由 GitHub 保存且可能公開；請只提供必要資訊。文件改變會標示新版本，資料處理用途改變時應同步更新說明與適用的確認流程。
