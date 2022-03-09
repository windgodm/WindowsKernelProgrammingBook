/*
* Base on "Windows Kernel Programing" Chapter 3.
* Example of dynamic memory allocation and unicode_string.
*/

#include <ntddk.h>

#define DRIVER_TAG 'dcba' // big

void SampleUnload(_In_ PDRIVER_OBJECT DriverObject);

UNICODE_STRING g_RegistryPath;

extern "C"
NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	KdPrint(("[S2] DriverEntry() called\n"));

	// set unload
	DriverObject->DriverUnload = SampleUnload;

	// allocate memory
	g_RegistryPath.Buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool, RegistryPath->Length, DRIVER_TAG);
	if (g_RegistryPath.Buffer == nullptr) {
		KdPrint(("[S2] Failed to allocate memory\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// strcpy
	g_RegistryPath.MaximumLength = RegistryPath->Length;
	RtlCopyUnicodeString(&g_RegistryPath, RegistryPath);

	// Print
	DbgPrint("[S2]:RegistryPath{Length:%d, MaximumLength:%d, Buffer:%wZ}",
		RegistryPath->Length, RegistryPath->MaximumLength, RegistryPath);
	DbgPrint("[S2]:Copied{Length:%d, MaximumLength:%d, Buffer:%wZ}",
		g_RegistryPath.Length, g_RegistryPath.MaximumLength, &g_RegistryPath);

	return STATUS_SUCCESS;
}

void SampleUnload(_In_ PDRIVER_OBJECT DriverObject)
{
	KdPrint(("[S2] Unload() called\n"));

	UNREFERENCED_PARAMETER(DriverObject);

	ExFreePool(g_RegistryPath.Buffer);
}
