#include <windows.h>
int wmain(int argc, wchar_t **argv)
{
    if (argc != 2) return 1;
    HANDLE event = OpenEventW(SYNCHRONIZE, FALSE, argv[1]); if (!event) return 2;
    WaitForSingleObject(event, 30000); CloseHandle(event); return 0;
}
