#pragma once

#include "muduo20/base/noncopyable.h"
#include "muduo20/co-net/Callbacks.h"
#include "muduo20/co-net/InetAddress.h"
#include "muduo20/co-net/TcpConnection.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace muduo20::co_net {

class Acceptor;
class EventLoop;
class EventLoopThreadPool;

class TcpServer : muduo20::noncopyable {
  public:
    using ThreadInitCallback = CallbackFunction<void(EventLoop *)>;
    enum class Option : uint8_t { kNoReusePort, kReusePort };

    TcpServer(EventLoop *loop, const InetAddress &listenAddr,
              std::string nameArg, Option option = Option::kNoReusePort);
    ~TcpServer();

    [[nodiscard]] const std::string &ipPort() const { return ipPort_; }
    [[nodiscard]] const std::string &name() const { return name_; }
    [[nodiscard]] EventLoop *getLoop() const { return loop_; }

    void setThreadNum(int numThreads);
    void setThreadInitCallback(ThreadInitCallback cb) {
        threadInitCallback_ = std::move(cb);
    }
    template <typename F>
        requires CallbackBindable<F, ThreadInitCallback>
    void setThreadInitCallback(F &&cb) {
        threadInitCallback_ = ThreadInitCallback(std::forward<F>(cb));
    }
    [[nodiscard]] std::shared_ptr<EventLoopThreadPool> threadPool() {
        return threadPool_;
    }

    void start();

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
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop *loop_;
    const std::string ipPort_;
    const std::string name_;
    std::unique_ptr<Acceptor> acceptor_;
    std::shared_ptr<EventLoopThreadPool> threadPool_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    ThreadInitCallback threadInitCallback_;
    std::atomic<int> started_{0};
    int nextConnId_{1};
    ConnectionMap connections_;
};

} // namespace muduo20::co_net
