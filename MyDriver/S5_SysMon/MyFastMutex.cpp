#include "pch.h"
#include "MyFastMutex.h"

void MyFastMutex::Init() {
    ExInitializeFastMutex(&_mutex);
}

void MyFastMutex::Lock() {
    ExAcquireFastMutex(&_mutex);
}

void MyFastMutex::Unlock() {
    ExReleaseFastMutex(&_mutex);
}
