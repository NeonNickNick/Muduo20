#pragma once

#include "muduo20/base/noncopyable.h"
#include "muduo20/co-net/Callbacks.h"
#include "muduo20/co-net/InetAddress.h"

#include <atomic>
#include <cstdint>
#include <memory>

namespace muduo20::co_net {

class Channel;
class EventLoop;

class Connector : muduo20::noncopyable,
                  public std::enable_shared_from_this<Connector> {
  public:
    using NewConnectionCallback = CallbackFunction<void(int sockfd)>;

    Connector(EventLoop *loop, const InetAddress &serverAddr);
    ~Connector();

    template <typename F>
        requires CallbackBindable<F, NewConnectionCallback>
    void setNewConnectionCallback(F &&cb) {
        newConnectionCallback_ = NewConnectionCallback(std::forward<F>(cb));
    }

    void start();
    void restart();
    void stop();

    [[nodiscard]] const InetAddress &serverAddress() const noexcept {
        return serverAddr_;
    }

  private:
    enum class States : uint8_t { kDisconnected, kConnecting, kConnected };

    static constexpr int kMaxRetryDelayMs = 30 * 1000;
    static constexpr int kInitRetryDelayMs = 500;

    void setState(States state) noexcept { state_ = state; }

    void startInLoop();
    void stopInLoop();
    void connect();
    void connecting(int sockfd);
    void handleWrite();
    void handleError();
    void retry(int sockfd);
    [[nodiscard]] int removeAndResetChannel();
    void resetChannel();

    EventLoop *loop_;
    InetAddress serverAddr_;
    std::atomic<bool> connect_{false};
    States state_{States::kDisconnected};
    std::unique_ptr<Channel> channel_;
    NewConnectionCallback newConnectionCallback_;
    int retryDelayMs_{kInitRetryDelayMs};
};

} // namespace muduo20::co_net
