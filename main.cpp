#include "intern.h"


static const size_t BUFLEN = 1024;

int main () {
    SocketContext context;

    Hub hub;
    if (!hub.Init())
        return -1;

    TcpListener listener;
    if (!listener.Init())
        return -1;

    if (!hub.Associate((HANDLE)listener.Socket(), &listener))
        return -1;

    if (!hub.Process())
        return -1;

    return 0;
}
