#pragma once

#define PRIORITY_BOOSTER_DEVICE 0x8000
#define IOCTL_PRIORITY_BOOSTER_SET CTL_CODE(\
	PRIORITY_BOOSTER_DEVICE, 0x800, METHOD_NEITHER, FILE_ANY_ACCESS)

typedef struct _ThreadData {
	ULONG threadId;
	int priority;
}ThreadData, * PThreadData;
