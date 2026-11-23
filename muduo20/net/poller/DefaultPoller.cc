#include "muduo20/net/Poller.h"

#include "muduo20/net/poller/EPollPoller.h"
#include "muduo20/net/poller/PollPoller.h"

#include <cstdlib>

using namespace muduo20::net;

std::unique_ptr<Poller> Poller::newDefaultPoller(EventLoop *loop) {
  if (::getenv("MUDUO20_USE_POLL") != nullptr) {
    return std::make_unique<PollPoller>(loop);
  }
  return std::make_unique<EPollPoller>(loop);
}
