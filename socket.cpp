#include "intern.h"


/****************************************************************************
*
*   Data
*
***/

unsigned TcpStream::s_id = 0;


/****************************************************************************
*
*   Local functions
*
***/

//===========================================================================
static inline SOCKET CreateSocket () {
    return WSASocket(
        AF_INET,                // af
        SOCK_STREAM,            // type
        IPPROTO_TCP,            // protocol
        NULL,                   // lpProtocolInfo
        0,                      // g
        WSA_FLAG_OVERLAPPED     // dwFlags
    );
}


/****************************************************************************
*
*   TestSendNotify
*
***/

class TestSendNotify : public TcpSendNotify {
public:
    bool OnTcpSendComplete (
        const char data[],
        size_t dataSize,
        size_t bytes
    ) override {
        delete this;
        return true;
    }
};


/****************************************************************************
*
*   TcpListener
*
***/

//===========================================================================
bool TcpListener::Init () {
    m_socket = CreateSocket();
    if (m_socket == INVALID_SOCKET)
        return false;

    USHORT port = 12345;

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.S_un.S_addr = 0;
    addr.sin_port = ((port & 0xFF) << 8) | ((port & 0xFF00) >> 8);
    ZeroMemory(addr.sin_zero, sizeof(addr.sin_zero));

    int result = bind(
        m_socket,           // s
        (sockaddr *)&addr,  // name
        sizeof(sockaddr_in) // namelen
    );
    if (result != 0)
        return false;

    result = listen(
        m_socket,   // s
        SOMAXCONN   // backlog
    );
    if (result != 0)
        return false;

    return true;
}

//===========================================================================
bool TcpListener::OnHubAssociate (Hub * hub) {
    m_hub = hub;
    return Accept();
}

//===========================================================================
bool TcpListener::OnHubEvent (size_t bytes) {
    TcpStream * stream = new TcpStream(m_accept);
    m_accept = INVALID_SOCKET;

    if (!m_hub->Associate((HANDLE)stream->Socket(), stream))
        return false;

    return Accept();
}

//===========================================================================
bool TcpListener::Accept () {
    if (m_accept != INVALID_SOCKET)
        return true;

    m_accept = CreateSocket();
    if (m_accept == INVALID_SOCKET)
        return false;

    ZeroMemory(m_addrs, sizeof(m_addrs));
    ZeroMemory(&overlapped, sizeof(OVERLAPPED));

    AcceptEx(
        m_socket,           // sListenSocket
        m_accept,           // sAcceptSocket
        m_addrs,            // lpOutputBuffer
        0,                  // dwReceiveDataLength
        ADDR_SPACE_BYTES,   // dwLocalAddressLength
        ADDR_SPACE_BYTES,   // dwRemoteAddressLength
        NULL,               // lpdwBytesReceived
        &overlapped         // lpOverlapped
    );
    int error = WSAGetLastError();
    return error == ERROR_IO_PENDING;
}


/****************************************************************************
*
*   TcpStream
*
***/

//===========================================================================
bool TcpStream::Send (
    const char data[],
    size_t dataSize,
    TcpSendNotify * notify
) {
    notify->m_buf.buf = const_cast<CHAR *>(data);
    notify->m_buf.len = static_cast<ULONG>(dataSize);

    int result = WSASend(
        m_socket,
        &notify->m_buf,
        1,
        NULL,
        0,
        &notify->overlapped,
        NULL
    );

    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error == WSAECONNABORTED)
            delete this;
        else if (error != ERROR_IO_PENDING)
            return false;
    }
    
    return true;
}

//===========================================================================
bool TcpStream::OnHubAssociate (Hub * hub) {
    printf("stream %d create\n", m_id);
    return Receive();
}

//===========================================================================
bool TcpStream::OnHubEvent (size_t bytes) {
    if (bytes == 0) {
        printf("stream %d close\n", m_id);
        delete this;
        return true;
    }

    printf("stream %d receive\n", m_id);

    if (!Process(m_data, bytes))
        return false;

    return Receive();
}

//===========================================================================
bool TcpStream::Receive () {
    ZeroMemory(&m_data, sizeof(m_data));
    ZeroMemory(&overlapped, sizeof(OVERLAPPED));

    int result = WSARecv(
        m_socket,       // s
        &m_buffer,      // lpBuffers
        1,              // dwBufferCount
        NULL,           // lpNumberOfBytesRecvd
        &m_flags,       // lpFlags
        &overlapped,    // lpOverlapped
        NULL            // lpCompletionRoutine
    );

    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error == WSAECONNABORTED) {
            printf("stream %d abort\n", m_id);
            delete this;
        }
        else if (error != ERROR_IO_PENDING) {
            return false;
        }
    }

    return true;
}

//===========================================================================
bool TcpStream::Process (char data[], size_t dataSize) {
    static const char REPLY[] = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n";
    return Send(REPLY, sizeof(REPLY) - 1, new TestSendNotify());
}

//===========================================================================
void TcpStream::Close () {
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}


/****************************************************************************
*
*   Exported functions
*
***/

//===========================================================================
bool SocketInit () {
    WORD version = MAKEWORD(2, 2);
    WSAData data;
    ZeroMemory(&data, sizeof(data));
    int result = WSAStartup(version, &data);
    return result == 0 && data.wVersion == version;
}

//===========================================================================
void SocketCleanup () {
    WSACleanup();
}