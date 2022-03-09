/*
* Base on "Windows Kernel Programing" Chapter 7.
* Example of Direct I/O. This is part of user-mode client.
* Target filename: Zero
* /MTd
*/

#include <iostream>
#include <windows.h>

int Error(const char* msg);

int main()
{
    // get device handle
    HANDLE hDevice = CreateFile(L"\\\\.\\S4Zero", GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDevice == INVALID_HANDLE_VALUE)
        return Error("Failed to open device");

    // test buf
    BYTE buffer[64];
    for (int i = 0; i < sizeof(buffer); i++)
        buffer[i] = i + 1;

    // read
    DWORD btr;
    if (!ReadFile(hDevice, buffer, sizeof(buffer), &btr, nullptr))
        return Error("failed to read");
    if (btr != sizeof(buffer))
        printf("Wrong (btr:%d)\n", btr);
    // check
    long bufSum = 0;
    for (int i : buffer)
        bufSum += i;
    if (bufSum != 0)
        printf("Wrong data\n");
    else
        printf("Read succeed\n");

    // write
    DWORD btw;
    if (!WriteFile(hDevice, buffer, sizeof(buffer), &btw, nullptr))
        return Error("failed to write");
    if (btw != sizeof(buffer))
        printf("Wrong (btw:%d)", btw);
    else
        printf("Write succeed\n");

    // release
    CloseHandle(hDevice);
}

int Error(const char* msg)
{
    printf("%s (error=%d)\n", msg, GetLastError());
    return 1;
}
