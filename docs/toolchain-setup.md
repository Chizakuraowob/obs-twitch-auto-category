# Windows 建置工具安裝審查

目標：OBS 32.2.2 x64（本機 Qt 6.11.1），Windows 11。
官方 CMakePresets.json 使用 Visual Studio 18 2026、Windows SDK 10.0.26100.0。
採 Visual Studio **2026 Build Tools**；VS 2026 generator 需要 CMake **4.2 或更新穩定版**。

## 安裝內容與影響

- C++ Build Tools 工作負載及推薦組件：MSVC、MSBuild、Windows SDK、CMake 工具。
- 指定 Windows SDK 26100 及 C++ CMake tools。
- 下載 Microsoft 官方 bootstrapper；驗證 Microsoft 簽章成功後才執行。
- 寫入 Program Files、Windows SDK 與安裝器快取，需要管理員授權，可能使用數 GB；以安裝器實際估算為準。
- --norestart 不主動重新啟動電腦。回傳 3010 時記錄需重新啟動。
- 不安裝完整 Visual Studio IDE、不修改 OBS、不自動部署外掛。
- Qt 與 OBS 開發依賴由 CMake 下載到專案 .deps，驗證官方版本與 SHA256。

## 手動安裝

1. 從 https://aka.ms/vs/18/stable/vs_buildtools.exe 下載官方安裝器。
2. 確認檔案有有效的 Microsoft 數位簽章。
3. 選擇 C++ Build Tools 工作負載。
4. 確認包含 MSVC x64/x86、Windows 11 SDK 10.0.26100、C++ CMake tools。
5. 安裝完成後開啟 Developer PowerShell，執行 cmake --version，需至少 4.2。
6. 若內建 CMake 過舊，從 https://cmake.org/download/ 安裝更新穩定版，或解壓 ZIP 到專案 .tools。

## 自動安裝

scripts/install-toolchain.ps1 為可審查的實際腳本，批准系統安裝後才執行。
不修改 ExecutionPolicy、不自動重新啟動。

## 來源

- https://github.com/obsproject/obs-studio/blob/32.2.2/CMakePresets.json
- https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2018%202026.html
- https://learn.microsoft.com/en-us/visualstudio/install/use-command-line-parameters-to-install-visual-studio
