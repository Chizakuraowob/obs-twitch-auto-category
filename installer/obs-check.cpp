#include <windows.h>
#include <tlhelp32.h>
#include <msiquery.h>

// No process termination, window messaging or dependency on Qt inside Windows Installer.
extern "C" __declspec(dllexport) int __stdcall TcaObsRunning()
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return -1;
    PROCESSENTRY32W entry{}; entry.dwSize = sizeof(entry);
    bool ok = Process32FirstW(snapshot, &entry) != FALSE;
    if (!ok) { CloseHandle(snapshot); return -1; }
    do {
        if (_wcsicmp(entry.szExeFile, L"obs64.exe") == 0) { CloseHandle(snapshot); return 1; }
    } while (Process32NextW(snapshot, &entry));
    const DWORD error = GetLastError(); CloseHandle(snapshot);
    return error == ERROR_NO_MORE_FILES ? 0 : -1;
}
extern "C" __declspec(dllexport) UINT __stdcall TcaCheckObs(MSIHANDLE install)
{
    wchar_t level[16]{}; DWORD size = 16; MsiGetPropertyW(install, L"UILevel", level, &size);
    for (;;) {
        const int state = TcaObsRunning();
        PMSIHANDLE record = MsiCreateRecord(0);
        MsiRecordSetStringW(record, 0, state == 0 ? L"TCA_OBS_CHECK: clear" : state > 0 ? L"TCA_OBS_CHECK: running" : L"TCA_OBS_CHECK: failed");
        MsiProcessMessage(install, INSTALLMESSAGE_INFO, record);
        if (state == 0) return ERROR_SUCCESS;
        MsiRecordSetStringW(record, 0, state > 0 ? L"OBS 正在執行。請先結束直播／錄影，儲存設定並手動關閉所有 OBS。完成後按「重試」重新檢查，或按「取消」退出。安裝器不會強制關閉 OBS。" : L"無法檢查 OBS 程序，為避免覆寫使用中的外掛，安裝已停止。請稍後重試。 ");
        const bool interactive = level[0] >= L'4';
        const int answer = MsiProcessMessage(install, static_cast<INSTALLMESSAGE>(INSTALLMESSAGE_ERROR | (interactive && state > 0 ? MB_RETRYCANCEL : MB_OK) | MB_ICONWARNING), record);
        if (interactive && state > 0 && answer == IDRETRY) continue;
        return answer == IDCANCEL ? ERROR_INSTALL_USEREXIT : ERROR_INSTALL_FAILURE;
    }
}
