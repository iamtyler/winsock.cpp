#pragma once

#include <cstdio>

#include <WinSock2.h>
#include <MSWSock.h>
#include <Windows.h>


/****************************************************************************
*
*   hub.cpp
*
***/

class Hub;

struct IHubEventNotify {
    virtual bool OnHubEvent (size_t bytes) = 0;

    OVERLAPPED overlapped;
};

struct IHubNotify {
    virtual bool OnHubAssociate (Hub * hub) = 0;
};

class Hub {
public:
    Hub () : m_handle(NULL) {}

    bool Init ();
    bool Associate (HANDLE handle, IHubNotify * notify);
    bool Process ();

private:
    HANDLE m_handle;
};


/****************************************************************************
*
*   socket.cpp
*
***/

bool SocketInit ();
void SocketCleanup ();

class SocketContext {
public:
    SocketContext () { SocketInit(); }
    ~SocketContext () { SocketCleanup(); }
};

class SocketBase {
public:
    SocketBase () : SocketBase(INVALID_SOCKET) {}
    SocketBase (SOCKET socket) : m_socket(socket) {}

    virtual ~SocketBase () {
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
        }
    }

    SOCKET Socket () const { return m_socket; }

protected:
    SOCKET m_socket;
};

class TcpListener :
    public SocketBase,
    public IHubNotify,
    public IHubEventNotify
{
public:
    TcpListener () : m_accept(INVALID_SOCKET), m_hub(nullptr) {}
    ~TcpListener () override {
        if (m_accept != INVALID_SOCKET)
            closesocket(m_accept);
    }

    bool Init ();

private:
    bool OnHubAssociate (Hub * hub) override;
    bool OnHubEvent (size_t bytes) override;

    bool Accept ();

    static const size_t ADDR_BYTES = sizeof(sockaddr_in);
    static const size_t ADDR_SPACE_BYTES = ADDR_BYTES + 16;

    Hub * m_hub;
    SOCKET m_accept;
    char m_addrs[ADDR_SPACE_BYTES * 2];
};

class TcpSendNotify : IHubEventNotify {
    friend class TcpStream;

public:
    virtual bool OnTcpSendComplete (
        const char data[],
        size_t dataSize,
        size_t bytes
    ) = 0;

private:
    bool OnHubEvent (size_t bytes) override {
        return OnTcpSendComplete(m_buf.buf, m_buf.len, bytes);
    }

    WSABUF m_buf;
};

class TcpStream :
    public SocketBase,
    public IHubNotify,
    public IHubEventNotify
{
    friend class TcpListener;

public:
    bool Send (
        const char data[],
        size_t dataSize,
        TcpSendNotify * notify
    );

private:
    TcpStream (SOCKET socket) : SocketBase(socket), m_flags(0), m_id(++s_id) {
        m_buffer.buf = m_data;
        m_buffer.len = sizeof(m_data);
    }

    bool OnHubAssociate (Hub * hub) override;
    bool OnHubEvent (size_t bytes) override;

    bool Receive ();
    bool Process (char data[], size_t dataSize);
    void Close ();

    static const size_t DATA_BYTES = 1024;
    
    static unsigned s_id;

    unsigned m_id;

    char m_data[DATA_BYTES];
    WSABUF m_buffer;
    DWORD m_flags;
};
