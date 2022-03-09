#pragma once
#include "MyFastMutex.h"

#define PROCESS_TERMINATE 1

const int MaxPids = 256;

struct Globals {
	ULONG pids[MaxPids];
	int count;
	MyFastMutex mutex;
	PVOID regHandle;
};
