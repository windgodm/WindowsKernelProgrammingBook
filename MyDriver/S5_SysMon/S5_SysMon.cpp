/*
* Base on "Windows Kernel Programing" Chapter 6, 8, 9.
* (6) Example of fast mutex.
* (8) Example of process notification, thread notification.
* (8) Exercise: Add iamge file name to ProcessCreateInfo.
* (8) Exercise: Add image load notifications, collecting user mode image loads only.
* (8) Exercise: Detect remote thread create.
* (9) Example of register notification.
* /integritycheck
*/

#include "pch.h"


// global data

Globals g_data;

// unload routine

void SysMonUnload(_In_ PDRIVER_OBJECT DriverObject);

// dispatch routine

NTSTATUS SysMonCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
NTSTATUS SysMonRead(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
NTSTATUS SysMonWrite(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
NTSTATUS SysMonDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);

// list entry

void PushRecord(LIST_ENTRY* pEntry);
void ClearRecord();

// notification

void SetNotify(UINT8 newBmNotify);
void OnProcessNotify(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo);
void OnThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create);
void OnImageNotify(PUNICODE_STRING FullImageName, HANDLE ProcessId, PIMAGE_INFO ImageInfo);
NTSTATUS OnRegistryNotify(_In_ PVOID CallbackContext, _In_opt_ PVOID Argument1, _In_opt_ PVOID Argument2);

// DriverEntry

extern "C"
NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(RegistryPath);

	NTSTATUS status;
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\DEVICE\\S5SysMon");
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\S5SysMon");
	PDEVICE_OBJECT devObj;

	// set routine
	DriverObject->DriverUnload = SysMonUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = SysMonCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = SysMonCreateClose;
	DriverObject->MajorFunction[IRP_MJ_READ] = SysMonRead;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = SysMonWrite;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = SysMonDeviceControl;

	g_data.pDriverObj = DriverObject;
	g_data.bmNotify = 0;
	InitializeListHead(&g_data.head);
	g_data.count = 0;
	g_data.mutex.Init();

	do {
		// device
		status = IoCreateDevice(
			DriverObject,        // our driver object
			0,                   // no need for extra bytes
			&devName,            // the device name
			FILE_DEVICE_UNKNOWN, // device type
			0,                   // characteristics flags
			FALSE,               // not exclusive
			&devObj              // the resulting pointer
		);
		if (status < 0) break;
		// Direct I/O
		devObj->Flags |= DO_DIRECT_IO;

		// symbolic link
		status = IoCreateSymbolicLink(&symLink, &devName);
		if (status < 0) break;

		status = STATUS_SUCCESS;
	} while (false);

	if (status < 0) {
		if (devObj)
			IoDeleteDevice(devObj);
	}

	return status;
}

// unload routine

void SysMonUnload(_In_ PDRIVER_OBJECT DriverObject) {
	// remove notification 
	
	SetNotify(0);

	// release links

	ClearRecord();

	// delete symbolic link

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\S5SysMon");
	IoDeleteSymbolicLink(&symLink);

	// delete device

	IoDeleteDevice(DriverObject->DeviceObject);
}

// dispatch routine

NTSTATUS SysMonCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS SysMonRead(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);

	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto status = STATUS_SUCCESS;
	ULONG infomation = 0;

	do {
		auto len = stack->Parameters.Read.Length;
		ULONG btr = 0;
		if (len == 0)
			break;

		// get buffer

		auto buffer = (UCHAR*)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
		if (!buffer) {
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		// work

		AutoLock<MyFastMutex> autolock(g_data.mutex);
		while (true) {
			if (IsListEmpty(&g_data.head)) break;

			auto pEntry = (RecordEntry<RecordHeader>*)RemoveHeadList(&g_data.head);
			auto size = pEntry->Data.size;
			if (len < size) {
				InsertHeadList(&g_data.head, (PLIST_ENTRY)pEntry);
				break;
			}
			g_data.count--;

			memcpy(buffer + btr, &pEntry->Data, size);

			len -= size;
			btr += size;
			ExFreePool(pEntry);
		}

		infomation = btr;
	} while (false);

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = infomation;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

NTSTATUS SysMonWrite(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);

	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto status = STATUS_SUCCESS;
	ULONG infomation = 0;
	auto len = stack->Parameters.Write.Length;

	// auto buffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
	infomation = len;

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = infomation;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

NTSTATUS SysMonDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);

	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto status = STATUS_SUCCESS;
	ULONG info = 0;

	switch (stack->Parameters.DeviceIoControl.IoControlCode) {
	case IOCTL_SYS_MON_SET: {
		// buffer io
		auto data = (UINT8*)Irp->AssociatedIrp.SystemBuffer;
		auto size = stack->Parameters.DeviceIoControl.InputBufferLength;
		if (size != sizeof(UINT8)) {
			status = STATUS_INVALID_BUFFER_SIZE;
			break;
		}
		SetNotify(*data);
		break;
	}
	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

// list entry

void PushRecord(LIST_ENTRY *pEntry) {
	AutoLock<MyFastMutex> autolock(g_data.mutex);

	if (g_data.count < RECORD_MAXNUM) {
		InsertTailList(&g_data.head, pEntry);
		g_data.count++;
	}
	else {
		// too many record, remove head(oldest one)
		auto head = RemoveHeadList(&g_data.head);
		ExFreePool(head); // LIST_ENTRY* == RecordEntry<>*
		InsertTailList(&g_data.head, pEntry);
	}
}

void ClearRecord() {
	while (!IsListEmpty(&g_data.head)) {
		auto pEntry = RemoveHeadList(&g_data.head);
		ExFreePool(pEntry); // LIST_ENTRY* == RecordEntry<>*
	}
}

// notification

void SetNotify(UINT8 newBmNotify) {
	AutoLock<MyFastMutex> lock(g_data.mutex);
	NTSTATUS status;
	UINT8 dif = g_data.bmNotify ^ newBmNotify;

	do {
		// process notification 
		if (dif & SYS_MON_PROCESS_NOTIFY) {
			if (newBmNotify & SYS_MON_PROCESS_NOTIFY){
				KdPrint(("[s5]Process On"));
				status = PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, false);
			}
			else {
				KdPrint(("[s5]Process Off"));
				status = PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, true);
			}
		}
		// thread notification
		if (dif & SYS_MON_THREAD_NOTIFY) {
			if (newBmNotify & SYS_MON_THREAD_NOTIFY) {
				KdPrint(("[s5]Thread On"));
				status = PsSetCreateThreadNotifyRoutine(OnThreadNotify);
			}
			else {
				KdPrint(("[s5]Thread Off"));
				status = PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);
			}
		}
		// image load notification
		if (dif & SYS_MON_IMAGE_LOAD_NOTIFY) {
			if (newBmNotify & SYS_MON_IMAGE_LOAD_NOTIFY) {
				KdPrint(("[s5]Image Load On"));
				status = PsSetLoadImageNotifyRoutine(OnImageNotify);
			}
			else{
				KdPrint(("[s5]Image Load Off"));
				status = PsRemoveLoadImageNotifyRoutine(OnImageNotify);
			}
		}
		// register notification
		if (dif & SYS_MON_REGISTRY_SET_VALUE_NOTIFY) {
			if (newBmNotify & SYS_MON_REGISTRY_SET_VALUE_NOTIFY) {
				KdPrint(("[s5]Registry Set Value On"));
				UNICODE_STRING altitude = RTL_CONSTANT_STRING(L"7657.114514");
				status = CmRegisterCallbackEx(
					OnRegistryNotify,
					&altitude,
					g_data.pDriverObj,
					nullptr,
					&g_data.regCookie,
					nullptr);
			}
			else {
				KdPrint(("[s5]Registry Set Value Off"));
				status = CmUnRegisterCallback(g_data.regCookie);
			}
		}
	} while (false);

	g_data.bmNotify = newBmNotify;
}

void OnProcessNotify(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo) {
	UNREFERENCED_PARAMETER(Process);
	
	if (CreateInfo) {
		// process create

		// alloc
		int allocSize = sizeof(RecordEntry<ProcessCreateInfo>);
		USHORT nameSize = 0;
		USHORT cmdSize = 0;
		if (CreateInfo->ImageFileName) {
			nameSize = CreateInfo->ImageFileName->Length;
			allocSize += nameSize;
		}
		if (CreateInfo->CommandLine) {
			cmdSize = CreateInfo->CommandLine->Length;
			allocSize += cmdSize;
		}
		auto recordEntry = (RecordEntry<ProcessCreateInfo>*)ExAllocatePoolWithTag(
			PagedPool,
			allocSize,
			DRIVER_TAG);
		if (recordEntry == nullptr) return;

		auto& record = recordEntry->Data;
		record.type = RecordType::ProcessCreate;
		record.size = (USHORT)(sizeof(ProcessCreateInfo) + nameSize + cmdSize);
		KeQuerySystemTimePrecise(&record.time);
		record.pid = HandleToULong(ProcessId);
		record.parentPid = HandleToULong(CreateInfo->ParentProcessId);
		if (nameSize) {
			record.nameSize = nameSize;
			record.nameOffset = sizeof(ProcessCreateInfo);
			memcpy((char*)&record + record.nameOffset, CreateInfo->ImageFileName->Buffer, nameSize);
		}
		else {
			record.nameSize = 0;
		}
		if (cmdSize) {
			record.cmdSize = cmdSize;
			record.cmdOffset = sizeof(ProcessCreateInfo) + nameSize;
			memcpy((char*)&record + record.cmdOffset, CreateInfo->CommandLine->Buffer, cmdSize);
		}
		else {
			record.nameSize = 0;
		}

		PushRecord(&recordEntry->Entry);
	}
	else {
		// process exit

		auto recordEntry = (RecordEntry<ProcessExitInfo>*)ExAllocatePoolWithTag(
			PagedPool,
			sizeof(RecordEntry<ProcessExitInfo>),
			DRIVER_TAG);
		if (recordEntry == nullptr) return;

		auto& record = recordEntry->Data;
		record.type = RecordType::ProcessExit;
		record.size = sizeof(ProcessExitInfo);
		KeQuerySystemTimePrecise(&record.time);
		record.pid = HandleToULong(ProcessId);

		PushRecord(&recordEntry->Entry);
	}
}

void OnThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create) {

	auto recordEntry = (RecordEntry<ThreadCreateExitInfo>*)ExAllocatePoolWithTag(
		PagedPool,
		sizeof(RecordEntry<ProcessExitInfo>),
		DRIVER_TAG);
	if (recordEntry == nullptr) return;

	auto& record = recordEntry->Data;
	record.type = Create ? RecordType::ThreadCraete : RecordType::ThreadExit;
	record.size = sizeof(ProcessExitInfo);
	KeQuerySystemTimePrecise(&record.time);
	record.pid = HandleToULong(ProcessId);
	record.tid = HandleToULong(ThreadId);

	// Detect remote thread creations.
	if (Create) {
		auto curPid = PsGetCurrentProcessId();
		if (curPid != ProcessId) {
			KdPrint(("Detect remote thread create pid:%u tid:%u rpid:%u",
				HandleToULong(ProcessId),
				HandleToULong(ThreadId),
				HandleToULong(curPid)));
		}
	}

	PushRecord(&recordEntry->Entry);
}

void OnImageNotify(PUNICODE_STRING FullImageName, HANDLE ProcessId, PIMAGE_INFO ImageInfo) {

	if (ImageInfo->SystemModeImage & 1)
		return;

	// alloc
	int allocSize = sizeof(RecordEntry<ImageLoadInfo>);
	USHORT nameSize = 0;
	if (FullImageName) {
		nameSize = FullImageName->Length;
		allocSize += nameSize;
	}
	auto recordEntry = (RecordEntry<ImageLoadInfo>*)ExAllocatePoolWithTag(
		PagedPool,
		allocSize,
		DRIVER_TAG);
	if (recordEntry == nullptr) return;

	auto& record = recordEntry->Data;
	record.type = RecordType::ImageLoad;
	record.size = sizeof(ImageLoadInfo) + nameSize;
	KeQuerySystemTimePrecise(&record.time);
	record.pid = HandleToULong(ProcessId);
	record.ImageBase = (UINT64)ImageInfo->ImageBase;
	if (nameSize) {
		record.nameSize = nameSize;
		record.nameOffset = sizeof(ImageLoadInfo);
		memcpy((char*)&record + record.nameOffset, FullImageName->Buffer, nameSize);
	}
	else {
		record.nameSize = 0;
	}

	PushRecord(&recordEntry->Entry);
}

NTSTATUS OnRegistryNotify(_In_ PVOID CallbackContext, _In_opt_ PVOID Argument1, _In_opt_ PVOID Argument2) {
	UNREFERENCED_PARAMETER(CallbackContext);

	static const WCHAR machine[] = L"\\REGISTRY\\MACHINE\\";

	auto status = STATUS_SUCCESS;

	switch ((REG_NOTIFY_CLASS)(ULONG_PTR)Argument1) {
	case RegNtPostSetValueKey: {
		auto args = (PREG_POST_OPERATION_INFORMATION)Argument2;
		if (!args) break;
		if (args->Status < 0) break;
		
		PCUNICODE_STRING name = nullptr;
		status = CmCallbackGetKeyObjectIDEx(&g_data.regCookie, args->Object, nullptr, &name, 0);
		if (status < 0) break;

		if (wcsncmp(name->Buffer, machine, ARRAYSIZE(machine) - 1) == 0) {
			auto preInfo = (PREG_SET_VALUE_KEY_INFORMATION)args->PreInformation;
			//if (!preInfo) break;

			// alloc
			auto allocSize = sizeof(RecordEntry<RegistrySetValueInfo>);
			auto recordEntry = (RecordEntry<RegistrySetValueInfo>*)ExAllocatePoolWithTag(
				PagedPool,
				allocSize,
				DRIVER_TAG);
			if (recordEntry == nullptr) break;
			RtlZeroMemory(recordEntry, allocSize);
			
			auto& record = recordEntry->Data;
			record.type = RecordType::RegistrySetValue;
			record.size = sizeof(RegistrySetValueInfo);
			KeQuerySystemTimePrecise(&record.time);
			record.pid = HandleToULong(PsGetCurrentProcessId());
			record.tid = HandleToULong(PsGetCurrentThreadId());
			wcsncpy_s(record.keyName, name->Buffer, name->Length / sizeof(WCHAR) - 1);
			wcsncpy_s(record.valueName, preInfo->ValueName->Buffer, preInfo->ValueName->Length / sizeof(WCHAR) - 1);
			record.dataType = preInfo->Type;
			record.dataSize = preInfo->DataSize;
			memcpy(record.data, preInfo->Data, min(record.dataSize, sizeof(record.data)));

			PushRecord(&recordEntry->Entry);
		}

		CmCallbackReleaseKeyObjectIDEx(name);

		break;
	}
	default:
		break;
	}

	return status;
}
