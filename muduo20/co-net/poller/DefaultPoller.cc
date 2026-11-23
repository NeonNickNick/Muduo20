#include "muduo20/co-net/Poller.h"

#include "muduo20/co-net/poller/EPollPoller.h"
#include "muduo20/co-net/poller/PollPoller.h"

#include <cstdlib>

using namespace muduo20::co_net;

std::unique_ptr<Poller> Poller::newDefaultPoller(EventLoop *loop) {
  if (::getenv("MUDUO20_USE_POLL") != nullptr) {
    return std::make_unique<PollPoller>(loop);
  }
  return std::make_unique<EPollPoller>(loop);
}
