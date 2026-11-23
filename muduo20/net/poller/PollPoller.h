#pragma once

#include "muduo20/net/Poller.h"

#include <vector>

struct pollfd;

namespace muduo20::net {

class PollPoller : public Poller {
public:
  explicit PollPoller(EventLoop *loop);
  ~PollPoller() override;

  [[nodiscard]] Timestamp poll(int timeoutMs,
                               ChannelList *activeChannels) override;
  void updateChannel(Channel *channel) override;
  void removeChannel(Channel *channel) override;

private:
  void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;

  using PollFdList = std::vector<struct pollfd>;
  PollFdList pollfds_;
};

} // namespace muduo20::net
