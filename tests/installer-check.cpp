#include <windows.h>
#include <msiquery.h>
#include <cstdio>
#include <string>
struct Probe { HANDLE event, process; bool retry = false; int prompts = 0; };
static int CALLBACK message(void *context, UINT type, LPCWSTR)
{
    if ((type & 0xff000000) != INSTALLMESSAGE_ERROR) return IDOK;
    auto &probe = *static_cast<Probe *>(context); ++probe.prompts;
    if ((type & 0xf) != MB_RETRYCANCEL) return IDCANCEL;
    if (!probe.retry || probe.prompts > 1) return IDCANCEL;
    SetEvent(probe.event); WaitForSingleObject(probe.process, 3000); return IDRETRY;
}
int wmain(int argc, wchar_t **argv)
{
    if (argc != 4) return 1;
    HMODULE library = LoadLibraryW(argv[2]); if (!library) return 2;
    auto running = reinterpret_cast<int (__stdcall *)()>(GetProcAddress(library, "TcaObsRunning")); if (!running) return 3;
    auto check = reinterpret_cast<UINT (__stdcall *)(MSIHANDLE)>(GetProcAddress(library, "TcaCheckObs")); if (!check) return 3;
    const int baseline = running(); if (baseline < 0) return 4;
    MsiEnableLogW(INSTALLLOGMODE_VERBOSE | INSTALLLOGMODE_ERROR | INSTALLLOGMODE_INFO, L"dist\\installer\\process-probe.log", INSTALLLOGATTRIBUTES_FLUSHEACHLINE);
    MsiSetInternalUI(INSTALLUILEVEL_NONE, nullptr); MSIHANDLE raw = 0;
    if (MsiOpenPackageExW(argv[1], MSIOPENPACKAGEFLAGS_IGNOREMACHINESTATE, &raw) != ERROR_SUCCESS) return 5;
    PMSIHANDLE install(raw); MsiSetPropertyW(install, L"UILevel", L"2");
    const UINT initial = check(install);
    if (initial != (baseline ? ERROR_INSTALL_FAILURE : ERROR_SUCCESS)) { std::printf("Initial guard failed: baseline=%d, return=%u\n", baseline, initial); return 6; }
    const auto eventName = L"Local\\TcaInstallerFixture-" + std::to_wstring(GetCurrentProcessId());
    HANDLE event = CreateEventW(nullptr, TRUE, FALSE, eventName.c_str()); if (!event) return 7;
    std::wstring command = L"\"" + std::wstring(argv[3]) + L"\" \"" + eventName + L"\"";
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION child{};
    if (!CreateProcessW(argv[3], command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child)) { CloseHandle(event); return 8; }
    int result = 0;
    if (running() != 1 || check(install) != ERROR_INSTALL_FAILURE) result = 9;
    Probe probe{event, child.hProcess}; MsiSetExternalUIW(message, INSTALLLOGMODE_ERROR, &probe); MsiSetPropertyW(install, L"UILevel", L"5");
    if (check(install) != ERROR_INSTALL_USEREXIT || probe.prompts != 1) result = 10;
    probe.retry = true; probe.prompts = 0;
    const UINT retried = check(install);
    if (retried != (baseline ? ERROR_INSTALL_USEREXIT : ERROR_SUCCESS) || probe.prompts != (baseline ? 2 : 1)) result = 11;
    SetEvent(event); WaitForSingleObject(child.hProcess, 3000); CloseHandle(child.hThread); CloseHandle(child.hProcess); CloseHandle(event);
    MsiSetExternalUIW(nullptr, 0, nullptr); FreeLibrary(library);
    std::printf("Installer process guard: result=%d; external OBS=%d; silent block/cancel/retry exercised; no installation actions executed\n", result, baseline);
    return result;
}
