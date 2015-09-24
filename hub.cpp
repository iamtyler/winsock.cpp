#include "intern.h"


/****************************************************************************
*
*   TcpListener
*
***/

//===========================================================================
bool Hub::Init () {
    m_handle = CreateIoCompletionPort(
        INVALID_HANDLE_VALUE,   // FileHandle
        NULL,                   // ExistingCompletionPort
        0,                      // CompletionKey
        1                       // NumberOfConcurrentThreads
    );
    return m_handle != NULL;
}

//===========================================================================
bool Hub::Associate (HANDLE handle, IHubNotify * notify) {
    handle = CreateIoCompletionPort(
        handle,                 // FileHandle
        m_handle,               // ExistingCompletionPort
        (ULONG_PTR)notify,      // CompletionKey
        0                       // NumberOfConcurrentThreads
    );
    return handle == m_handle
        && (!notify || notify->OnHubAssociate(this));
}

//===========================================================================
bool Hub::Process () {
    OVERLAPPED * overlapped;
    DWORD bytes;
    ULONG_PTR key;

    for (;;) {
        BOOL success = GetQueuedCompletionStatus(
            m_handle,       // CompletionPort
            &bytes,         // lpNumberOfBytes
            &key,           // lpCompletionKey
            &overlapped,    // lpOverlapped
            INFINITE        // dwMilliseconds
        );
        if (success == FALSE)
            return false;

        IHubEventNotify * notify = CONTAINING_RECORD(overlapped, IHubEventNotify, overlapped);
        if (!notify->OnHubEvent(bytes))
            return false;
    }
}
