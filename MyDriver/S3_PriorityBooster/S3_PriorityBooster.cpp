/*
* Base on "Windows Kernel Programing" Chapter 4.
* Example of set thread priority.
*/

#include <ntifs.h>
#include <ntddk.h>
#include "S3_PriorityBoosterCommon.h"

// unload routine
void PriorityBoosterUnload(_In_ PDRIVER_OBJECT DriverObject);
// dispatch routine
NTSTATUS PriorityBoosterCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
NTSTATUS PriorityBoosterDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);

extern "C"
NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(RegistryPath);

	NTSTATUS status;

	// set unload routine
	DriverObject->DriverUnload = PriorityBoosterUnload;
	// Set dispatch routine
	DriverObject->MajorFunction[IRP_MJ_CREATE] = PriorityBoosterCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = PriorityBoosterCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = PriorityBoosterDeviceControl;

	// create device
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\DEVICE\\S3PriorityBooster");
	PDEVICE_OBJECT devObj;
	status = IoCreateDevice(
		DriverObject,        // our driver object
		0,                   // no need for extra bytes
		&devName,            // the device name
		FILE_DEVICE_UNKNOWN, // device type
		0,                   // characteristics flags
		FALSE,               // not exclusive
		&devObj              // the resulting pointer
	);
	if (status < 0) {
		KdPrint(("[s3] Failed to create device object (0x%08X)\n", status));
		return status;
	}
	// create symbolic link to device
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\S3PriorityBooster");
	status = IoCreateSymbolicLink(&symLink, &devName);
	if (status < 0) {
		KdPrint(("[s3] Failed to create symbolic link (0x%08X)\n", status));
		return status;
	}

	return STATUS_SUCCESS;
}

// unload routine

void PriorityBoosterUnload(_In_ PDRIVER_OBJECT DriverObject) {

	// delete symbolic link
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\S3PriorityBooster");
	IoDeleteSymbolicLink(&symLink);
	// delete device
	IoDeleteDevice(DriverObject->DeviceObject);
}

// dispatch routine

NTSTATUS PriorityBoosterCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS PriorityBoosterDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);

	KdPrint(("[s3] device control called\n"));

	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto status = STATUS_SUCCESS;
	PThreadData data = nullptr;

	switch (stack->Parameters.DeviceIoControl.IoControlCode) {
	case IOCTL_PRIORITY_BOOSTER_SET:
		// check input buffer
		if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ThreadData)) {
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}
		// auto data = (PThreadData)stack->Parameters.DeviceIoControl.Type3InputBuffer;
		data = (PThreadData)stack->Parameters.DeviceIoControl.Type3InputBuffer;
		if (data == nullptr) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		if (data->priority < 1 || data->priority > 31) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		// get thread
		PETHREAD pEThread;
		status = PsLookupThreadByThreadId(ULongToHandle(data->threadId), &pEThread);
		if (status < 0) break;

		// set thread priority
		KeSetPriorityThread((PKTHREAD)pEThread, data->priority);

		// release
		ObDereferenceObject(pEThread);

		KdPrint(("[s3] ThreadId:%d Priority:%d\n", data->threadId, data->priority));
		break;

	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return status;
}
