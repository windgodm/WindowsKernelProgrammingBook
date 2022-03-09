/*
* Base on "Windows Kernel Programing" Chapter 4.
* Example of set thread priority. This is part of user-mode client.
* Target filename: Booster
* /MTd
*/

#include <iostream>
#include <windows.h>
#include "../S3_PriorityBooster/S3_PriorityBoosterCommon.h"

using std::cout;
using std::endl;

int Error(const char* msg);

int main(int argc, const char* argv[])
{
    if (argc < 3) {
        cout << "Usage: Booster [threadid] [priority]\n";
        return 0;
    }

    // get device handle
    HANDLE hDevice = CreateFile(L"\\\\.\\S3PriorityBooster", GENERIC_WRITE,
        FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDevice == INVALID_HANDLE_VALUE)
        return Error("Failed to open device");

    // construction parameter
    ThreadData data;
    data.threadId = atoi(argv[1]);
    data.priority = atoi(argv[2]);

    // device io
    DWORD returned;
    BOOL success = DeviceIoControl(
        hDevice,
        IOCTL_PRIORITY_BOOSTER_SET,
        &data, sizeof(ThreadData),
        nullptr, 0,
        &returned, nullptr
    );
    if (success)
        printf("Thread priority change succeeded!\n");
    else
        Error("Thread priority change failed!\n");

    // release
    CloseHandle(hDevice);
}

int Error(const char* msg)
{
    printf("%s (error=%d)\n", msg, GetLastError());
    return 1;
}
