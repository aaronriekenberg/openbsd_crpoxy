#ifndef SOCKETUTIL_H
#define SOCKETUTIL_H

#include <netdb.h>
#include <stdbool.h>
#include <sys/socket.h>

struct SockAddrInfo
{
  union
  {
    struct sockaddr sa;
    struct sockaddr_storage saStorage;
  };
  socklen_t saSize;
};

struct AddrPortStrings
{
  char addrString[NI_MAXHOST];
  char portString[NI_MAXSERV];
};

extern bool addrInfoToNameAndPort(
  const struct addrinfo* addrinfo,
  struct AddrPortStrings* addrPortStrings);

extern bool sockAddrInfoToNameAndPort(
  const struct SockAddrInfo* sockAddrInfo,
  struct AddrPortStrings* addrPortStrings);

extern bool setSocketListening(
  const int socket);

extern bool setSocketReuseAddress(
  const int socket);

extern bool setSocketSplice(
  const int fromSocket,
  const int toSocket);

extern bool setBidirectionalSplice(
  const int socket1,
  const int socket2);

extern off_t getSpliceBytesTransferred(
  const int socket);

extern int getSocketError(
  const int socket);

extern bool signalSafeAccept(
  const int sockfd,
  int* acceptFD,
  struct SockAddrInfo* sockAddrInfo);

extern bool getSocketName(
  const int socketFD,
  struct SockAddrInfo* sockAddrInfo);

#endif
