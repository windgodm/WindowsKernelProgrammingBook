/*
* Exercise on "Windows Kernel Programing" Chapter 2.
* Use the RtlGetVersion function to output the Windows OS version.
*/

#include <ntddk.h>

void SampleUnload(_In_ PDRIVER_OBJECT DriverObject);

extern "C"
NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	KdPrint(("[E1] DriverEntry() called\n"));

	UNREFERENCED_PARAMETER(RegistryPath);

	// set unload
	DriverObject->DriverUnload = SampleUnload;

	// Get Version
	RTL_OSVERSIONINFOW versionInfo = { 0, };
	versionInfo.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
	RtlGetVersion(&versionInfo);

	// Print
	DbgPrint("[E1] Major:%d\n[E1] Minor:%d\n[E1] Build:%d",
		versionInfo.dwMajorVersion,
		versionInfo.dwMinorVersion,
		versionInfo.dwBuildNumber);

	return STATUS_SUCCESS;
}

void SampleUnload(_In_ PDRIVER_OBJECT DriverObject) {
	KdPrint(("[E1] Unload() called\n"));

	UNREFERENCED_PARAMETER(DriverObject);
}
