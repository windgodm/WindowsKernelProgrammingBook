/*
* Base on "Windows Kernel Programing" Chapter 7.
* Example of Direct I/O.
*/

#include "pch.h"

// unload routine
void ZeroUnload(_In_ PDRIVER_OBJECT DriverObject);
// dispatch routine
NTSTATUS ZeroCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
NTSTATUS ZeroRead(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
NTSTATUS ZeroWrite(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);

extern "C"
NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(RegistryPath);

	NTSTATUS status;

	// set routine
	DriverObject->DriverUnload = ZeroUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = ZeroCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = ZeroCreateClose;
	DriverObject->MajorFunction[IRP_MJ_READ] = ZeroRead;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = ZeroWrite;

	// create device
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\DEVICE\\S4Zero");
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\S4Zero");
	PDEVICE_OBJECT devObj;
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
		if (status < 0) {
			KdPrint(("[s4] Failed to create device object (0x%08X)\n", status));
			break;
		}
		// Direct I/O
		devObj->Flags |= DO_DIRECT_IO;

		// symbolic link
		status = IoCreateSymbolicLink(&symLink, &devName);
		if (status < 0) {
			KdPrint(("[s4] Failed to create symbolic link (0x%08X)\n", status));
			break;
		}

		status = STATUS_SUCCESS;
	} while (false);

	if (status < 0) {
		if (devObj)
			IoDeleteDevice(devObj);
	}

	return status;
}

// unload routine

void ZeroUnload(_In_ PDRIVER_OBJECT DriverObject) {
	// delete symbolic link
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\S4Zero");
	IoDeleteSymbolicLink(&symLink);
	// delete device
	IoDeleteDevice(DriverObject->DeviceObject);
}

// dispatch routine

NTSTATUS ZeroCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS ZeroRead(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);

	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto status = STATUS_SUCCESS;
	ULONG infomation = 0;
	
	do {
		auto len = stack->Parameters.Read.Length;
		if (len == 0)
			break;
		
		// get buffer

		auto buffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
		if (!buffer) {
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		// work

		memset(buffer, 0, len);
		infomation = len;

	} while (false);
	
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = infomation;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

NTSTATUS ZeroWrite(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp) {
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
