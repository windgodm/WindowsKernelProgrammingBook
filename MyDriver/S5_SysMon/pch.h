#pragma once
#include <ntddk.h>
#include "S5_SysMonCommon.h"
#include "MyFastMutex.h"

#define DRIVER_TAG 'omys' // symo
#define RECORD_MAXNUM 128 // 128 * (32 + 256*4) = 132 kB

// Record

template<typename T>
struct RecordEntry {
	LIST_ENTRY Entry;
	T Data;
};

struct Globals {
	PDRIVER_OBJECT pDriverObj;
	UINT8 bmNotify;
	LIST_ENTRY head;
	int count;
	MyFastMutex mutex;
	LARGE_INTEGER regCookie;
};
