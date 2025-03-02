#pragma once

#include <ntifs.h>
#include <wdf.h>
#include "Globals.hpp"
#include "WinTypes.hpp"

/*
 * @brief Retrieves the PID of the first matching process by name.
 * https://www.unknowncheats.me/forum/general-programming-and-reversing/572734-pid-process-name.html
 * 
 * @param[in] ProcessName Name of the process to look up.
 *
 * @returns HANDLE Process ID of the matching process, or NULL if not found.
 */
HANDLE 
BeGetPidFromProcessName(_In_ CONST UNICODE_STRING& ProcessName)
{
    NTSTATUS Status     = STATUS_SUCCESS;
    ULONG    BufferSize = 0;
    PVOID    Buffer     = NULL;

    PSYSTEM_PROCESS_INFORMATION pCurrent = NULL;

    Status = BeGlobals::pZwQuerySystemInformation(SystemProcessInformation, Buffer, BufferSize, &BufferSize);
    if (Status == STATUS_INFO_LENGTH_MISMATCH)
    {
        Buffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, BufferSize, DRIVER_TAG);
        if (Buffer == NULL)
        {
            return pCurrent;
        }
        else
        {
            Status = BeGlobals::pZwQuerySystemInformation(SystemProcessInformation, Buffer, BufferSize, &BufferSize);
            if (!NT_SUCCESS(Status))
            {
                ExFreePoolWithTag(Buffer, DRIVER_TAG);
                return pCurrent;
            }
        }
    }

    pCurrent = (PSYSTEM_PROCESS_INFORMATION)Buffer;
    while (pCurrent) 
    {
        if (pCurrent->ImageName.Buffer != NULL)
        {
            if (RtlCompareUnicodeString(&(pCurrent->ImageName), &ProcessName, TRUE) == 0)
            {
                ExFreePoolWithTag(Buffer, DRIVER_TAG);
                return pCurrent->ProcessId;
            }
        }
        if (pCurrent->NextEntryOffset == 0) {
            pCurrent = NULL;
        }
        else 
        {
            pCurrent = (PSYSTEM_PROCESS_INFORMATION)(((PUCHAR)pCurrent) + pCurrent->NextEntryOffset);
        }
    }

    return pCurrent;
}

/*
 * @brief Retrieves the EPROCESS pointer for a given PID.
 *
 * @param[in] Pid PID of the process to retrieve.
 *
 * @returns PEPROCESS Pointer to the EPROCESS object, or NULL if not found.
 */
PEPROCESS
BeGetEprocessByPid(_In_ ULONG Pid)
{
    PEPROCESS Process;
    if (PsLookupProcessByProcessId(ULongToHandle(Pid), &Process) != 0)
    {
        LOG_MSG("PID %i not found \r\n", (ULONG)Pid);
        return NULL;
    }

    return Process;
}