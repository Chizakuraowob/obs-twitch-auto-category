# 遊戲辨識與資料處理清單（0.3.0，2026-10-25）

此文件是供作者撰寫使用者條款與隱私權政策的實作紀錄，不是法律審查或最終政策。外掛內測試草稿版本更新為 draft-2026-10-25，既有使用者會重新閱讀同意流程。

## 本機辨識

| 資料 | 來源與用途 | 保存／對外傳送 |
|---|---|---|
| 程序名稱、前景程序 | Windows 程序快照，用於自訂規則與多遊戲選擇 | 工作階段記憶體，不上傳 |
| 可見視窗所屬程序的執行檔完整路徑 | Windows PROCESS_QUERY_LIMITED_INFORMATION；比對是否位於已知遊戲安裝目錄 | 暫存於掃描，不寫入設定、不上傳；不提權、不注入 |
| Minecraft 視窗標題 | 只對 Java／Minecraft 程序讀取，用於區分遊戲與啟動器 | 掃描時比對，不保存、不上傳 |
| Steam 遊戲名稱、App ID、安裝目錄 | HKCU Software/Valve/Steam 的 SteamPath，steamapps/libraryfolders.vdf 與 appmanifest_*.acf | 遊戲庫暫存於記憶體，約 60 秒刷新，不上傳整份資料 |
| Epic 遊戲名稱、AppName、安裝目錄 | ProgramData/Epic/EpicGamesLauncher/Data/Manifests/*.item | 同上；不登入 Epic、不讀取 Epic Token |
| GOG 遊戲名稱、ID、安裝目錄 | HKLM SOFTWARE/GOG.com/Games 與 WOW6432Node 對應位置 | 同上；不登入 GOG、不讀取 GOG Token |
| 平台遊戲 ID → Twitch 分類 ID／名稱 | 使用者手動連結，或允許聯網後取得唯一同名分類 | OBS 外掛 settings.json，最多 1,000 筆，保留直到清除 |

只查閱已知平台安裝紀錄，不遞迴掃描整顆磁碟、不讀取遊戲存檔或程序命令列。略過明顯啟動器、錯誤回報及安裝程序。拒絕 UNC／遠端磁碟遊戲庫；讀取權限不足則跳過，不修改系統權限。停用「本機遊戲庫」後不再安排相關掃描，已進行的讀取可能完成但不納入候選。

## 聯網與控制

- **遊戲名稱自動查詢預設關閉**。只有使用者在設定開啟、已同意文件且登入 Twitch，並偵測到沒有本機分類快取的執行中遊戲時，才向 `https://api.twitch.tv/helix/games?name=...` 發送請求。
- 該請求包含遊戲名稱、Client ID、OAuth Bearer Token 與一般 HTTPS 連線資訊（IP、請求時間）；不包含平台遊戲 ID、程序名稱、檔案路徑、視窗標題或完整安裝清單。Twitch 的處理及保存受其政策管轄，外掛不能替第三方承諾保存期限。
- 查詢逐筆進行；相同平台遊戲於同一工作階段失敗後不持續重試。重新登入、儲存設定或清除快取可重新嘗試；既有 HTTP 429 節流仍適用。
- 僅採用唯一、同名且格式有效的分類；查無結果、名稱不同、結果有歧義或網路失敗時，不猜測／自動更新，提供手動搜尋後連結本機遊戲。
- 關閉名稱查詢立即阻止新的查詢與關閉後收到結果的保存，但不能撤回已送出的請求。已有快取仍可離線辨識；「清除分類對應快取」刪除保存對應。如果名稱查詢仍開啟，之後可重新查詢建立快取。
- 未增加第三方遊戲資料庫下載、雲端辨識、遙測、作者伺服器或背景資料庫更新服務。Steam／Epic／GOG 不接收外掛的偵測請求。
- 原有 Twitch OAuth、手動分類搜尋／更新／確認及圖片 CDN 連線保持不變。開發者模式只禁止建議套用的寫入；若另外允許名稱查詢且已登入，該查詢仍可能聯網；手動套用仍會更新 Twitch。

## 保存與刪除

收藏、偏好、同意紀錄、自訂程序規則及分類對應快取位於 OBS 外掛設定檔；不含完整安裝目錄。Token 存於 Windows Credential Manager。遊戲庫暫存及活動紀錄只在記憶體。清除快取保留收藏和手動程序規則；「清除本機資料」清除全部偏好、對應、收藏及憑證，並嘗試撤銷 Twitch 授權。單純關閉自動偵測不刪除已存資料。

## 能力與來源限制

這是讀取使用者已安裝遊戲庫的可擴充辨識方式，不是已驗證涵蓋市場多數遊戲的完整資料庫。Steam／Epic／GOG 大量遊戲可共用相同流程；Xbox、EA、Ubisoft、Battle.net、可攜遊戲及非標準安裝目前仍需手動程序規則。需要遊戲有可見視窗、程序路徑可讀且位於正確安裝目錄。模組、商店與 Twitch 名稱差異可能需要首次手動對應。

未複製或散布第三方遊戲清單／圖像／程式碼，沒有新增資料庫再散布授權。安裝清單格式可能隨平台更新而改變。平台名稱及遊戲名稱屬各權利人，專案不宣稱獲平台認可。

實作參考：[Twitch Get Games API](https://dev.twitch.tv/docs/api/reference#get-games)、[Epic 本機紀錄格式參考](https://github.com/JosefNemec/PlayniteExtensions/blob/master/source/Libraries/EpicLibrary/EpicLauncher.cs)、[Steam 安裝清單讀取參考](https://github.com/beallio/game-install-finder)。這些為格式與 API 參考，不隨外掛下載或執行。
