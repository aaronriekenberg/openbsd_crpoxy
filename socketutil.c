#include "socketutil.h"
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

int addressToNameAndPort(
  const struct sockaddr* address,
  const socklen_t addressSize,
  struct AddrPortStrings* addrPortStrings)
{
  int retVal;
  if ((retVal = getnameinfo(address,
                            addressSize,
                            addrPortStrings->addrString,
                            NI_MAXHOST,
                            addrPortStrings->portString,
                            NI_MAXSERV,
                            (NI_NUMERICHOST | NI_NUMERICSERV))) != 0)
  {
    printf("getnameinfo error: %s\n", gai_strerror(retVal));
    return -1;
  }
  return 0;
}

int setSocketListening(
  int socket)
{
  return listen(socket, SOMAXCONN);
}

int setSocketReuseAddress(
  int socket)
{
  int optval = 1;
  return setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

int setSocketNoDelay(
  int socket)
{
  int optval = 1;
  return setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

int setSocketSplice(
  int fromSocket, 
  int toSocket)
{
  return setsockopt(fromSocket, SOL_SOCKET, SO_SPLICE, &toSocket, sizeof(toSocket));
}

int setBidirectionalSplice(
  int socket1,
  int socket2)
{
  int retVal;

  retVal = setSocketSplice(socket1, socket2);
  if (retVal < 0)
  {
    return retVal;
  }

  retVal = setSocketSplice(socket2, socket1);

  return retVal;
}

off_t getSpliceBytesTransferred(
  int socket)
{
  off_t bytesTransferred;
  socklen_t optlen = sizeof(bytesTransferred);
  int retVal =
    getsockopt(socket, SOL_SOCKET, SO_SPLICE, &bytesTransferred, &optlen);
  if (retVal < 0)
  {
    bytesTransferred = 0;
  }
  return bytesTransferred;
}

int getSocketError(
  int socket)
{
  int optval = 0;
  socklen_t optlen = sizeof(optval);
  int retVal =
    getsockopt(socket, SOL_SOCKET, SO_ERROR, &optval, &optlen);
  if (retVal < 0)
  {
    return retVal;
  }
  return optval;
}

int signalSafeAccept(
  int sockfd,
  struct sockaddr* addr,
  socklen_t* addrlen)
{
  bool interrupted;
  int retVal;
  do
  {
    retVal = accept(sockfd, addr, addrlen);
    interrupted =
      ((retVal < 0) &&
       (errno == EINTR));
  } while (interrupted);
  return retVal;
}
