/*
* Base on "Windows Kernel Programing" Chapter 2.
* Simple driver example.
*/

// #include <ntddk.h>
#include <ntifs.h>

void SampleUnload(_In_ PDRIVER_OBJECT DriverObject);

extern "C"
NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	KdPrint(("[Sample] DriverEntry() called\n"));

	UNREFERENCED_PARAMETER(RegistryPath);

	// set unload
	DriverObject->DriverUnload = SampleUnload;

	PVOID address = (PVOID)0x403010;
	PEPROCESS process = (PEPROCESS)0xffffaa8f3767c300;
	KAPC_STATE apc_state;
	RtlZeroMemory(&apc_state, sizeof(KAPC_STATE));

	KeStackAttachProcess(process, &apc_state);

	DbgBreakPoint();

	MmIsAddressValid(address);

	KeUnstackDetachProcess(&apc_state);

	return STATUS_SUCCESS;
}

void SampleUnload(_In_ PDRIVER_OBJECT DriverObject) {
	KdPrint(("[Sample] Unload() called\n"));

	UNREFERENCED_PARAMETER(DriverObject);
}
