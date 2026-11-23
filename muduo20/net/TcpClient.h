#pragma once

#include "muduo20/base/noncopyable.h"
#include "muduo20/net/Callbacks.h"
#include "muduo20/net/Connector.h"
#include "muduo20/net/InetAddress.h"
#include "muduo20/net/TcpConnection.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace muduo20::net {

class TcpClient : muduo20::noncopyable {
  public:
    TcpClient(EventLoop *loop, const InetAddress &serverAddr,
              std::string nameArg);
    ~TcpClient();

    void connect();
    void disconnect();
    void stop();

    [[nodiscard]] TcpConnectionPtr connection() const {
        std::scoped_lock lock(mutex_);
        return connection_;
    }

    [[nodiscard]] EventLoop *getLoop() const { return loop_; }
    [[nodiscard]] bool retry() const {
        return retry_.load(std::memory_order_acquire);
    }
    void enableRetry() { retry_.store(true, std::memory_order_release); }

    [[nodiscard]] const std::string &name() const { return name_; }

    template <typename F>
        requires CallbackBindable<F, ConnectionCallback>
    void setConnectionCallback(F &&cb) {
        connectionCallback_ = ConnectionCallback(std::forward<F>(cb));
    }

    template <typename F>
        requires CallbackBindable<F, MessageCallback>
    void setMessageCallback(F &&cb) {
        messageCallback_ = MessageCallback(std::forward<F>(cb));
    }

    template <typename F>
        requires CallbackBindable<F, WriteCompleteCallback>
    void setWriteCompleteCallback(F &&cb) {
        writeCompleteCallback_ = WriteCompleteCallback(std::forward<F>(cb));
    }

  private:
    void newConnection(int sockfd);
    void removeConnection(const TcpConnectionPtr &conn);

    EventLoop *loop_;
    std::shared_ptr<Connector> connector_;
    const std::string name_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    std::atomic<bool> retry_{false};
    std::atomic<bool> connect_{true};
    int nextConnId_{1};
    mutable std::mutex mutex_;
    TcpConnectionPtr connection_;
};

} // namespace muduo20::net
