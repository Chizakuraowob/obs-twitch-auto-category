# GitHub 建置與草稿發布

`.github/workflows/windows.yml` 沿用 `scripts/build.ps1`、`package-msi.ps1` 與現有測試。使用 GitHub 官方 `windows-2025-vs2026` runner，建置工具版本由腳本與 CMake preset 再次檢查。此工作流程尚未推送，也尚未在 GitHub runner 執行。

PR、main 提交、`v*` tag 及手動啟動會建置、執行 CTest、封裝 MSI、檢查 MSI 結構、收集完整對應來源並上傳 Actions artifact。沒有 Public Client ID 的一般建置產生 Preview MSI；tag 建置缺少 ID 或版本不吻合會失敗。正常發行版另檢查 DLL/MSI 中繼資料。runner 沒有安裝 OBS，因此略過本機安裝前提驗證；仍驗證 payload、程序阻擋動作順序及 OS 條件邊界。這不等同實機 MSI 安裝測試。

## 配置與執行

1. 審查後自行推送程式與 workflow 到 GitHub。
2. 儲存庫 Settings → Secrets and variables → Actions → Variables 新增 `TWITCH_CLIENT_ID`，填公開 Client ID。它不是秘密，不使用 Client Secret。
3. 先檢查 main workflow 的編譯、CTest、MSI 及來源收集均成功。
4. 將 `buildspec.json` 版本改為新版本並提交；建立完全一致的 `vX.Y.Z` tag，作者決定推送後，tag workflow 必須成功。
5. 在 Actions 手動執行 workflow，選取該版本 tag 並勾選 `create_draft`。它會重建並建立草稿 Release，不自動公開。相同 Release 已存在時會失敗，避免覆寫既有資產。
6. 人工檢查變更說明、來源檔、MSI、SHA-256、簽章狀態、升級與解除安裝，再決定是否公開。尚未配置簽章時草稿會明示未簽章／SAC 限制。

Actions 預設只有 contents:read；只有手動草稿 job 有 contents:write。checkout 不保存 Git 憑證，發布命令不讀 PR 文字作為 shell 指令，第三方 Actions 固定 commit。沒有 `pull_request_target`，不讓外部 PR 使用發布權限或簽章 Secrets。未建立作者簽章身分整合前不提供任何假簽章步驟。

`scripts/package-sources.ps1` 要求乾淨的 Git 工作目錄，以 `git archive HEAD` 產生確切 commit 來源，不打包 build/cache、私人設定或憑證。QtBase、obs-deps 與 WiX 來源使用固定 SHA-256 檢查，附 Qt SBOM 與 `SOURCE-MANIFEST.txt`。來源版本更新時須同步腳本、授權文件與校驗資料。首次收集會連線 Qt 官方及 GitHub 下載來源；只在作者建置／CI 執行，不影響外掛使用者的網路行為。

Release 的 MSI 名稱與 GitHub API SHA-256 digest 須符合既有更新器需求。作者公開 Release 後才會被 `/releases/latest` 偵測；草稿不對一般使用者提供。更新器仍由使用者關閉 OBS 後手動執行 MSI，不支援執行中替換 DLL。

runner 參考：[GitHub 官方映像](https://github.com/actions/runner-images/blob/main/images/windows/Windows2025-VS2026-Readme.md)。免費簽章準備見 [code-signing.md](code-signing.md)。
