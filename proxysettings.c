#include "log.h"
#include "memutil.h"
#include "proxysettings.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_CONNECT_TIMEOUT_MS (5000)

static void printUsageAndExit()
{
  printf("Usage:\n"
         "  cproxy -l <local addr>:<local port>\n"
         "         [-l <local addr>:<local port>...]\n"
         "         -r <remote addr>:<remote port>\n"
         "         [-c <connect timeout milliseconds>]\n"
         "Arguments:\n"
         "  -c <connect timeout milliseconds>: specify connection timeout in milliseconds\n"
         "  -l <local addr>:<local port>: specify listen address and port\n"
         "  -r <remote addr>:<remote port>: specify remote address and port\n");
  exit(1);
}

static struct addrinfo* parseAddrPort(
  const char* optarg)
{
  struct addrinfo hints;
  struct addrinfo* addressInfo = NULL;
  char addressString[NI_MAXHOST];
  char portString[NI_MAXSERV];
  const size_t optargLen = strlen(optarg);
  size_t colonIndex;
  size_t hostLength;
  size_t portLength;
  int retVal;

  colonIndex = optargLen;
  while (colonIndex > 0)
  {
    --colonIndex;
    if (optarg[colonIndex] == ':')
    {
      break;
    }
  }

  if ((colonIndex <= 0) ||
      (colonIndex >= (optargLen - 1)))
  {
    proxyLog("invalid address:port argument: '%s'", optarg);
    goto fail;
  }

  hostLength = colonIndex;
  portLength = optargLen - colonIndex - 1;

  if ((hostLength >= NI_MAXHOST) ||
      (portLength >= NI_MAXSERV))
  {
    proxyLog("invalid address:port argument: '%s'", optarg);
    goto fail;
  }

  strncpy(addressString, optarg, hostLength);
  addressString[hostLength] = 0;

  strncpy(portString, &(optarg[colonIndex + 1]), portLength);
  portString[portLength] = 0;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_ADDRCONFIG;

  if (((retVal = getaddrinfo(addressString, portString,
                             &hints, &addressInfo)) != 0) ||
      (addressInfo == NULL))
  {
    proxyLog("error resolving address %s %s",
             optarg, gai_strerror(retVal));
    goto fail;
  }

  return addressInfo;

fail:
  exit(1);
}

static struct addrinfo* parseRemoteAddrPort(
  const char* optarg,
  struct AddrPortStrings* addrPortStrings)
{
  struct addrinfo* addressInfo = parseAddrPort(optarg);

  if (!addressToNameAndPort(
        addressInfo->ai_addr,
        addressInfo->ai_addrlen,
        addrPortStrings))
  {
    exit(1);
  }

  return addressInfo;
}

static uint32_t parseConnectTimeoutMS(char* optarg)
{
  const char* errstr;
  const long long connectTimeoutMS = strtonum(optarg, 1, UINT32_MAX, &errstr);
  if (errstr != NULL)
  {
    proxyLog("invalid connect timeout argument '%s': %s", optarg, errstr);
    exit(1);
  }
  return connectTimeoutMS;
}

const struct ProxySettings* processArgs(
  int argc,
  char** argv)
{
  int retVal;
  struct ServerAddrInfo* pServerAddrInfo;
  struct ProxySettings* proxySettings =
    checkedCalloc(1, sizeof(struct ProxySettings));

  proxySettings->connectTimeoutMS = DEFAULT_CONNECT_TIMEOUT_MS;
  SIMPLEQ_INIT(&(proxySettings->serverAddrInfoList));
  do
  {
    retVal = getopt(argc, argv, "c:l:r:");
    switch (retVal)
    {
    case 'c':
      proxySettings->connectTimeoutMS = parseConnectTimeoutMS(optarg);
      break;

    case 'l':
      pServerAddrInfo = checkedCalloc(1, sizeof(struct ServerAddrInfo));
      pServerAddrInfo->addrinfo = parseAddrPort(optarg);
      SIMPLEQ_INSERT_TAIL(
        &(proxySettings->serverAddrInfoList),
        pServerAddrInfo, entry);
      break;

    case 'r':
      if (proxySettings->remoteAddrInfo)
      {
        goto fail;
      }
      proxySettings->remoteAddrInfo =
        parseRemoteAddrPort(
          optarg,
          &(proxySettings->remoteAddrPortStrings));
      break;

    case '?':
      goto fail;
      break;
    }
  }
  while (retVal != -1);

  if (SIMPLEQ_EMPTY(&(proxySettings->serverAddrInfoList)) ||
      (!(proxySettings->remoteAddrInfo)))
  {
    goto fail;
  }

  return proxySettings;

fail:
  printUsageAndExit();
  return NULL;
}