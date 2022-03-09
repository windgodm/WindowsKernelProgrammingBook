#pragma once

#define SYS_MON_DEVICE 0x8000
#define IOCTL_SYS_MON_SET \
	CTL_CODE(SYS_MON_DEVICE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define SYS_MON_PROCESS_NOTIFY 1
#define SYS_MON_THREAD_NOTIFY 2
#define SYS_MON_IMAGE_LOAD_NOTIFY 4
#define SYS_MON_REGISTRY_SET_VALUE_NOTIFY 8

enum class RecordType : short {
    None,
    ProcessCreate,
    ProcessExit,
    ThreadCraete,
    ThreadExit,
    ImageLoad,
    RegistrySetValue
};

struct RecordHeader {
    RecordType type;
    USHORT size;
    LARGE_INTEGER time;
};

struct ProcessCreateInfo : RecordHeader {
    ULONG pid;
    ULONG parentPid;
    USHORT nameSize;
    USHORT nameOffset;
    USHORT cmdSize;
    USHORT cmdOffset;
};

struct ProcessExitInfo : RecordHeader {
    ULONG pid;
};

struct ThreadCreateExitInfo : RecordHeader {
    ULONG tid;
    ULONG pid;
};

struct ImageLoadInfo : RecordHeader {
    ULONG pid;
    UINT64 ImageBase;
    USHORT nameSize;
    USHORT nameOffset;
};

struct RegistrySetValueInfo : RecordHeader {
    ULONG pid;
    ULONG tid;
    WCHAR keyName[256];
    WCHAR valueName[64];
    ULONG dataType; // REG_xxx
    UCHAR data[128]; // data
    ULONG dataSize;
};
