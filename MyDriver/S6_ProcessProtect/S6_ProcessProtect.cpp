/*
* Base on "Windows Kernel Programing" Chapter 9.
* Example of using object notification to protect process.
* /integritycheck 
*/

#include "pch.h"
#include <ntifs.h>
#include "S6_ProcessProtectCommon.h"

// global data

Globals g_data;

// unload routine

void ProcessProtectUnload(_In_ PDRIVER_OBJECT DriverObject);

// dispatch routine

NTSTATUS ProcessProtectCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
NTSTATUS ProcessProtectDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);

// object notification 

OB_PREOP_CALLBACK_STATUS OnPreOpenProcess(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION Info);

// function

bool AddProcess(ULONG pid);
bool RemoveProcess(ULONG pid); bool FindProcess(ULONG pid);
bool FindProcess(ULONG pid);
void ClearProcessWithLock();

// Driver Entry

extern "C"
NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(RegistryPath);

	NTSTATUS status;
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\DEVICE\\S6ProcessProtect");
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\S6ProcessProtect");
	PDEVICE_OBJECT devObj;

	// init data
	g_data.count = 0;
	g_data.mutex.Init();
	ClearProcessWithLock();
	g_data.regHandle = nullptr;

	// set routine
	DriverObject->DriverUnload = ProcessProtectUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = ProcessProtectCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = ProcessProtectCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ProcessProtectDeviceControl;

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
		// devObj->Flags |= DO_DIRECT_IO;

		// symbolic link
		status = IoCreateSymbolicLink(&symLink, &devName);
		if (status < 0) break;

		OB_OPERATION_REGISTRATION objOps[] = {
			{
				PsProcessType,
				OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE,
				OnPreOpenProcess, // pre
				nullptr           // post
			}
		};
		OB_CALLBACK_REGISTRATION objCallback = {
			OB_FLT_REGISTRATION_VERSION,
			1, // count
			RTL_CONSTANT_STRING(L"12345.114514"), // altitude
			nullptr, // context
			objOps
		};
		status = ObRegisterCallbacks(&objCallback, &g_data.regHandle);
		if (status < 0) break;

		status = STATUS_SUCCESS;
	} while (false);

	if (status < 0) {
		IoDeleteSymbolicLink(&symLink);
		if (devObj)
			IoDeleteDevice(devObj);
	}

	return status;
}

// unload routine

void ProcessProtectUnload(_In_ PDRIVER_OBJECT DriverObject) {
	// remove object 
	if(g_data.regHandle)
		ObUnRegisterCallbacks(g_data.regHandle);
	
	// delete symbolic link
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\S6ProcessProtect");
	IoDeleteSymbolicLink(&symLink);
	// delete device
	IoDeleteDevice(DriverObject->DeviceObject);
}

// dispatch routine

NTSTATUS ProcessProtectCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS ProcessProtectDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);

	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto status = STATUS_SUCCESS;
	ULONG info = 0;

	switch (stack->Parameters.DeviceIoControl.IoControlCode) {
	case IOCTL_PROCESS_PROTECT_BY_PID: {
		auto data = (ULONG*)Irp->AssociatedIrp.SystemBuffer;
		auto size = stack->Parameters.DeviceIoControl.InputBufferLength;
		if (size % sizeof(ULONG) != 0) { 
			status = STATUS_INVALID_BUFFER_SIZE;
			break;
		}
		AutoLock<MyFastMutex> lock(g_data.mutex);

		for (int i = 0; i < size / sizeof(ULONG); i++) {
			auto pid = data[i];
			// check
			if (pid == 0) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			if (FindProcess(pid))
				continue;
			if (g_data.count == MaxPids) {
				status = STATUS_TOO_MANY_CONTEXT_IDS;
				break;
			}
			// add
			if (!AddProcess(pid)) {
				status = STATUS_UNSUCCESSFUL;
				break;
			}
			info += sizeof(ULONG);
		}
		break;
	}
	case IOCTL_PROCESS_UNPROTECT_BY_PID: {
		auto data = (ULONG*)Irp->AssociatedIrp.SystemBuffer;
		auto size = stack->Parameters.DeviceIoControl.InputBufferLength;
		if (size % sizeof(ULONG) != 0) {
			status = STATUS_INVALID_BUFFER_SIZE;
			break;
		}
		AutoLock<MyFastMutex> lock(g_data.mutex);

		for (int i = 0; i < size / sizeof(ULONG); i++) {
			auto pid = data[i];
			// check
			if (pid == 0) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			// remove
			if (!RemoveProcess(pid)) {
				status = STATUS_UNSUCCESSFUL;
				break;
			}
			info += sizeof(ULONG);
		}
		break;
	}
	case IOCTL_PROCESS_PROTECT_CLEAR:
		ClearProcessWithLock();
		break;
	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

// object notification 

OB_PREOP_CALLBACK_STATUS OnPreOpenProcess(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION Info) {
	UNREFERENCED_PARAMETER(RegistrationContext);

	if (Info->KernelHandle)
		return OB_PREOP_SUCCESS;

	auto proc = (PEPROCESS)Info->Object;
	auto pid = HandleToULong(PsGetProcessId(proc));

	AutoLock<MyFastMutex> lock(g_data.mutex);

	if (FindProcess(pid)) {
		Info->Parameters->CreateHandleInformation.DesiredAccess &= (~PROCESS_TERMINATE);
	}

	return OB_PREOP_SUCCESS;
}

// function

bool AddProcess(ULONG pid) {
	for (int i = 0; i < MaxPids; i++) {
		if (g_data.pids[i] == 0) {
			g_data.pids[i] = pid;
			g_data.count++;
			return true;
		}
	}
	return false;
}

bool RemoveProcess(ULONG pid) {
	for (int i = 0; i < MaxPids; i++) {
		if (g_data.pids[i] == pid) {
			g_data.pids[i] = 0;
			g_data.count--;
			return true;
		}
	}
	return false;
}

bool FindProcess(ULONG pid) {
	for (int i = 0; i < MaxPids; i++) {
		if (g_data.pids[i] == pid)
			return true;
	}
	return false;
}

void ClearProcessWithLock() {
	AutoLock<MyFastMutex> lock(g_data.mutex);
	memset(g_data.pids, 0, sizeof(ULONG) * MaxPids);
}
