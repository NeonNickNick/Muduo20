#include "muduo20/co-net/Poller.h"

#include "muduo20/co-net/Channel.h"
#include "muduo20/co-net/EventLoop.h"

using namespace muduo20::co_net;

Poller::Poller(EventLoop *loop) : ownerLoop_(loop) {}

Poller::~Poller() = default;

bool Poller::hasChannel(Channel *channel) const {
  assertInLoopThread();
  const auto it = channels_.find(channel->fd());
  return it != channels_.end() && it->second == channel;
}

void Poller::assertInLoopThread() const { ownerLoop_->assertInLoopThread(); }
