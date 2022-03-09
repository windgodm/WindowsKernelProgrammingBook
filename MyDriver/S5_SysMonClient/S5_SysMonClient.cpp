/*
* Base on "Windows Kernel Programing" Chapter 6, 8, 9.
* Example of process,thread,image,register notification. This is part of user-mode client.
* Target filename: SysMon
* /MTd
*/

#include <iostream>
#include <windows.h>
#include "../S5_SysMon/S5_SysMonCommon.h"

BYTE buffer[1 << 17]; // 128kb

int Error(const char* msg);
void ShowRecord(DWORD size);
void ShowTime(const LARGE_INTEGER& time);

int main() {
	HANDLE hDevice = CreateFile(L"\\\\.\\S5SysMon", GENERIC_READ, FILE_SHARE_READ,
		nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE)
		return Error("Failed to open device");

	DWORD btr;
	char c;
	int i = 0;

	do {
		printf("[p/s] [n]> ");
		scanf_s("%c %d", &c, 1, &i);
		getchar();

		if (c == 'p') {
			for (; i; i--) {
				char cls[4] = { 'c', 'l', 's', 0 };
				system(cls);
				if (!ReadFile(hDevice, buffer, sizeof(buffer), &btr, nullptr))
					Error("Failed to read");
				if (btr)
					ShowRecord(btr);
				Sleep(500);
			}
		}
		else if (c == 's') {
			DeviceIoControl(
				hDevice,
				IOCTL_SYS_MON_SET,
				&i, // inbuf
				1, // inbuf size
				nullptr, // outbuf
				0, // outbuf size
				&btr,
				nullptr);
		}
		else if (c == 'e') {
			break;
		}
		else if (c == 'h') {
			printf("p: print\n");
			printf("s: set\n");
			printf("e: exit\n");
			printf("h: help\n");
		}
		else {
			printf("c:%d i:%d", c, i);
		}
	} while (c != 'e');

	CloseHandle(hDevice);
}

int Error(const char* msg) {
	printf("%s (error=%d)\n", msg, GetLastError());
	return 1;
}

void ShowRecord(DWORD size) {
	auto header = (RecordHeader*)buffer;
	auto end = buffer + size;

	while ((BYTE*)header < end) {
		printf("======\n");
		switch (header->type) {
		case RecordType::ProcessCreate: {
			ShowTime(header->time);
			auto data = (ProcessCreateInfo*)header;
			printf("Process Create\n");
			printf("pid:%u parentPid:%u\n", data->pid, data->parentPid);
			std::wstring name((WCHAR*)((BYTE*)header + data->nameOffset), data->nameSize / 2);
			std::wstring cmd((WCHAR*)((BYTE*)header + data->cmdOffset), data->cmdSize / 2);
			printf("ImageFileName:%ws\n", name.c_str());
			printf("CommandLine:%ws\n", cmd.c_str());
			break;
		}
		case RecordType::ProcessExit: {
			ShowTime(header->time);
			auto data = (ProcessExitInfo*)header;
			printf("Process Exit\n");
			printf("pid:%u\n", data->pid);
			break;
		}
		case RecordType::ThreadCraete: {
			ShowTime(header->time);
			auto data = (ThreadCreateExitInfo*)header;
			printf("Thread Create\n");
			printf("pid:%u tid:%u\n", data->pid, data->tid);
			break;
		}
		case RecordType::ThreadExit: {
			ShowTime(header->time);
			auto data = (ThreadCreateExitInfo*)header;
			printf("Thread Exit\n");
			printf("pid:%u tid:%u\n", data->pid, data->tid);
			break;
		}
		case RecordType::ImageLoad: {
			ShowTime(header->time);
			auto data = (ImageLoadInfo*)header;
			printf("Image Load\n");
			printf("pid:%u ImageBase:0x%I64X\n", data->pid, data->ImageBase);
			std::wstring name((WCHAR*)((BYTE*)header + data->nameOffset), data->nameSize / 2);
			printf("FullImageName:%ws\n", name.c_str());
			break;
		}
		case RecordType::RegistrySetValue: {
			ShowTime(header->time);
			auto data = (RegistrySetValueInfo*)header;
			printf("Registry Load\n");
			printf("pid:%u tid:%u\n", data->pid, data->tid);
			printf("key name:%ws\n", data->keyName);
			printf("value name:%ws\n", data->valueName);
			printf("type:%d size:%d\n", data->dataType, data->dataSize);
			printf("data:");
			switch (data->dataType) {
			case REG_EXPAND_SZ: printf("%ws\n", (WCHAR*)data->data); break;
			case REG_DWORD: printf("0x%0x8X\n", *(DWORD*)data->data); break;
			default: {
				DWORD dataSize = min(data->dataSize, sizeof(data->data));
				for (DWORD i = 0; i < dataSize; i++)
					printf("%02X ", data->data[i]);
				printf("\n");
				break;
			} // default end
			} // switch (dataType) end
			break;
		} // case RegistrySetValue end
		default:
			break;
		}

		header = (RecordHeader*)((BYTE*)header + header->size);
	}
}

void ShowTime(const LARGE_INTEGER& time) {
	SYSTEMTIME st;
	FileTimeToSystemTime((FILETIME*)&time, &st);
	st.wHour = (st.wHour + 8) % 24;
	printf("%02d:%02d:%02d.%03d: ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}
