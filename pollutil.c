#include "pollutil.h"
#include "log.h"
#include "errutil.h"
#include "memutil.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

struct PollState
{
  int kqueueFD;
  size_t numReadFDs;
  size_t numWriteAndTimeoutFDs;
  struct kevent* keventArray;
  size_t keventArrayCapacity;
  struct PollResult* pollResult;
};

struct PollState* newPollState()
{
  struct PollState* pollState = checkedCallocOne(sizeof(struct PollState));

  pollState->kqueueFD = kqueue();
  if (pollState->kqueueFD == -1)
  {
    proxyLog("kqueue error errno %d: %s",
             errno,
             errnoToString(errno));
    abort();
  }
  proxyLog("created kqueue (fd=%d)",
           pollState->kqueueFD);

  pollState->pollResult = newPollResult();

  return pollState;
}

static int signalSafeKevent(
  int kq, const struct kevent *changelist, int nchanges,
  struct kevent *eventlist, int nevents,
  const struct timespec *timeout)
{
  bool interrupted;
  int retVal;
  do
  {
    retVal = kevent(
      kq, changelist, nchanges,
      eventlist, nevents,
      timeout);
    interrupted = ((retVal == -1) &&
                   (errno == EINTR));
  } while (interrupted);
  return retVal;
}

static void resizeKeventArray(
  struct PollState* pollState)
{
  pollState->keventArray =
    resizeDynamicArray(
      pollState->keventArray,
      pollState->numReadFDs + pollState->numWriteAndTimeoutFDs,
      sizeof(struct kevent),
      &(pollState->keventArrayCapacity));
}

void addPollFDForRead(
  struct PollState* pollState,
  int fd,
  void* data)
{
  struct kevent event;
  int retVal;

  assert(pollState != NULL);

  EV_SET(&event, fd, EVFILT_READ, EV_ADD, 0, 0, data);

  retVal = signalSafeKevent(pollState->kqueueFD, &event, 1, NULL, 0, NULL);
  if (retVal == -1)
  {
    proxyLog("kevent add read event error fd %d errno %d: %s",
             fd,
             errno,
             errnoToString(errno));
    abort();
  }
  else
  {
    ++(pollState->numReadFDs);
    resizeKeventArray(pollState);
  }
}

void removePollFDForRead(
  struct PollState* pollState,
  int fd)
{
  struct kevent event;
  int retVal;

  assert(pollState != NULL);

  EV_SET(&event, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);

  retVal = signalSafeKevent(pollState->kqueueFD, &event, 1, NULL, 0, NULL);
  if (retVal == -1)
  {
    proxyLog("kevent remove read event error fd %d errno %d: %s",
             fd,
             errno,
             errnoToString(errno));
    abort();
  }
  else
  {
    --(pollState->numReadFDs);
  }
}

void addPollFDForWriteAndTimeout(
  struct PollState* pollState,
  int fd,
  void* data,
  uint32_t timeoutMillseconds)
{
  struct kevent events[2];
  int retVal;

  assert(pollState != NULL);

  EV_SET(&(events[0]), fd, EVFILT_WRITE, EV_ADD, 0, 0, data);
  EV_SET(&(events[1]), fd, EVFILT_TIMER, EV_ADD, 0, timeoutMillseconds, data);

  retVal = signalSafeKevent(pollState->kqueueFD, events, 2, NULL, 0, NULL);
  if (retVal == -1)
  {
    proxyLog("kevent add write and timeout events error fd %d errno %d: %s",
             fd,
             errno,
             errnoToString(errno));
    abort();
  }
  else
  {
    ++(pollState->numWriteAndTimeoutFDs);
    resizeKeventArray(pollState);
  }
}

void removePollFDForWriteAndTimeout(
  struct PollState* pollState,
  int fd)
{
  struct kevent events[2];
  int retVal;

  assert(pollState != NULL);

  EV_SET(&(events[0]), fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
  EV_SET(&(events[1]), fd, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);

  retVal = signalSafeKevent(pollState->kqueueFD, events, 2, NULL, 0, NULL);
  if (retVal == -1)
  {
    proxyLog("kevent remove write and timeout events error fd %d errno %d: %s",
             fd,
             errno,
             errnoToString(errno));
    abort();
  }
  else
  {
    --(pollState->numWriteAndTimeoutFDs);
  }
}

const struct PollResult* blockingPoll(
  struct PollState* pollState)
{
  int retVal;
  struct ReadyFDInfo* readyFDInfo;
  const struct kevent* readyKEvent;
  const struct kevent* endReadyKEvent;

  assert(pollState != NULL);

  if ((pollState->numReadFDs +
       pollState->numWriteAndTimeoutFDs) <= 0)
  {
    proxyLog("blockingPool called with no events registered");
    abort();
  }

  retVal = signalSafeKevent(
    pollState->kqueueFD,
    NULL, 0,
    pollState->keventArray, pollState->keventArrayCapacity,
    NULL);

  if (retVal == -1)
  {
    proxyLog("kevent wait error errno %d: %s",
             errno,
             errnoToString(errno));
    abort();
  }

  setPollResultNumReadyFDs(
    pollState->pollResult,
    retVal);

  readyFDInfo = pollState->pollResult->readyFDInfoArray;

  readyKEvent = pollState->keventArray;
  endReadyKEvent = readyKEvent + retVal;

  for (; readyKEvent != endReadyKEvent; ++readyFDInfo, ++readyKEvent)
  {
    readyFDInfo->data = readyKEvent->udata;
    readyFDInfo->readyForRead = (readyKEvent->filter == EVFILT_READ);
    readyFDInfo->readyForWrite = (readyKEvent->filter == EVFILT_WRITE);
    readyFDInfo->readyForTimeout = (readyKEvent->filter == EVFILT_TIMER);
  }

  return pollState->pollResult;
}
