/*
* Base on "Windows Kernel Programing" Chapter 9.
* Example of using object notification to protect process.
* This is part of user-mode client.
* Target filename: Protect
* /MTd
*/

#include <iostream>
#include <vector>
#include <windows.h>
#include "../S6_ProcessProtect/S6_ProcessProtectCommon.h"

using namespace std;

enum class Options {
	NoOpt,
	Add,
	Remove,
	Clear
};

int Error(const char* msg);

int wmain(int argc, const wchar_t* argv[]) {
	Options opt;

	if (argc < 2) {
		printf("Protect [add | remove | clear] [pid] ...\n");
		return 0;
	}

	if (_wcsicmp(argv[1], L"add") == 0)
		opt = Options::Add;
	else if (_wcsicmp(argv[1], L"remove") == 0)
		opt = Options::Remove;
	else if (_wcsicmp(argv[1], L"clear") == 0)
		opt = Options::Clear;
	else {
		printf("Protect [add | remove | clear] [pid] ...\n");
		return 0;
	}

	HANDLE hDevice = CreateFile(L"\\\\.\\S6ProcessProtect", GENERIC_WRITE | GENERIC_READ, 0,
		nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE)
		return Error("Failed to open device");

	vector<DWORD> pids;
	bool suc = false;
	DWORD btr;

	switch (opt) {
	case Options::Add: {
		for (int i = 0; i < argc - 2; i++)
			pids.push_back(_wtoi(argv[i + 2]));
		suc = DeviceIoControl(
			hDevice,
			IOCTL_PROCESS_PROTECT_BY_PID,
			pids.data(), // inbuf
			pids.size() * sizeof(DWORD), // inbuf size
			nullptr, // outbuf
			0, // outbuf size
			&btr,
			nullptr);
		break;
	}
	case Options::Remove: {
		for (int i = 0; i < argc - 2; i++)
			pids.push_back(_wtoi(argv[i + 2]));
		suc = DeviceIoControl(
			hDevice,
			IOCTL_PROCESS_UNPROTECT_BY_PID,
			pids.data(), // inbuf
			pids.size() * sizeof(DWORD), // inbuf size
			nullptr, // outbuf
			0, // outbuf size
			&btr,
			nullptr);
		break;
	}
	case Options::Clear:
		suc = DeviceIoControl(
			hDevice,
			IOCTL_PROCESS_PROTECT_CLEAR,
			nullptr, // inbuf
			0, // inbuf size
			nullptr, // outbuf
			0, // outbuf size
			&btr,
			nullptr);
		break;
	}

	if (!suc)
		return Error("Failed in DeviceIoControl");

	CloseHandle(hDevice);
}

int Error(const char* msg) {
	printf("%s (error=%d)\n", msg, GetLastError());
	return 1;
}
