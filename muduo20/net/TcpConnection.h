#pragma once

#include "muduo20/base/Timestamp.h"
#include "muduo20/base/noncopyable.h"
#include "muduo20/net/Buffer.h"
#include "muduo20/net/Callbacks.h"
#include "muduo20/net/InetAddress.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#if MUDUO20_ENABLE_LEGACY_COMPAT
#include "muduo20/base/StringPiece.h"
#endif

#include <any>
#include <chrono>
#include <memory>
#include <span>
#include <string_view>

struct tcp_info;

namespace muduo20::net {

class Channel;
class EventLoop;
class Socket;

class TcpConnection : muduo20::noncopyable,
                      public std::enable_shared_from_this<TcpConnection> {
  public:
    TcpConnection(EventLoop *loop, std::string name, int sockfd,
                  InetAddress localAddr, InetAddress peerAddr);
    ~TcpConnection();

    [[nodiscard]] EventLoop *getLoop() const { return loop_; }
    [[nodiscard]] const std::string &name() const { return name_; }
    [[nodiscard]] const InetAddress &localAddress() const { return localAddr_; }
    [[nodiscard]] const InetAddress &peerAddress() const { return peerAddr_; }
    [[nodiscard]] bool connected() const {
        return state_ == StateE::kConnected;
    }
    [[nodiscard]] bool disconnected() const {
        return state_ == StateE::kDisconnected;
    }
    [[nodiscard]] bool getTcpInfo(tcp_info *tcpi) const;
    [[nodiscard]] std::string getTcpInfoString() const;

#if MUDUO20_ENABLE_LEGACY_COMPAT
    void send(const void *message, int len);
#endif
#if MUDUO20_ENABLE_LEGACY_COMPAT
    void send(const char *message);
#endif
    void send(const std::string &message);
    void send(std::string &&message);
#if MUDUO20_ENABLE_LEGACY_COMPAT
    void send(StringPiece message);
#endif
    void send(std::string_view message);
    void send(std::span<const std::byte> message);
    void send(Buffer *message);

    void shutdown();
    void forceClose();
#if MUDUO20_ENABLE_LEGACY_COMPAT
    void forceCloseWithDelay(double seconds);
#endif
    void forceCloseWithDelay(std::chrono::microseconds delay);
    template <typename Rep, typename Period>
    void forceCloseWithDelay(std::chrono::duration<Rep, Period> delay) {
        forceCloseWithDelay(
            std::chrono::duration_cast<std::chrono::microseconds>(delay));
    }
    void setTcpNoDelay(bool on);

    void startRead();
    void stopRead();
    [[nodiscard]] bool isReading() const { return reading_; }

    void setContext(std::any context) { context_ = std::move(context); }
    [[nodiscard]] const std::any &getContext() const { return context_; }
    [[nodiscard]] std::any *getMutableContext() { return &context_; }

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

    template <typename F>
        requires CallbackBindable<F, HighWaterMarkCallback>
    void setHighWaterMarkCallback(F &&cb, size_t highWaterMark) {
        highWaterMarkCallback_ = HighWaterMarkCallback(std::forward<F>(cb));
        highWaterMark_ = highWaterMark;
    }

    template <typename F>
        requires CallbackBindable<F, CloseCallback>
    void setCloseCallback(F &&cb) {
        closeCallback_ = CloseCallback(std::forward<F>(cb));
    }

    [[nodiscard]] Buffer *inputBuffer() { return &inputBuffer_; }
    [[nodiscard]] Buffer *outputBuffer() { return &outputBuffer_; }

    void connectEstablished();
    void connectDestroyed();

  private:
    enum class StateE : std::uint8_t {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting,
    };

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();
    void sendInLoop(std::span<const std::byte> message);
    void shutdownInLoop();
    void forceCloseInLoop();
    void startReadInLoop();
    void stopReadInLoop();
    void setState(StateE state) { state_ = state; }
    [[nodiscard]] const char *stateToString() const;

    EventLoop *loop_;
    const std::string name_;
    StateE state_{StateE::kConnecting};
    bool reading_{false};
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;
    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_{static_cast<size_t>(64 * 1024 * 1024)};
    Buffer inputBuffer_;
    Buffer outputBuffer_;
    std::any context_;
};

} // namespace muduo20::net
