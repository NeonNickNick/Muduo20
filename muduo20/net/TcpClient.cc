#include "muduo20/net/TcpClient.h"

#include "muduo20/base/Logging.h"
#include "muduo20/base/Timestamp.h"
#include "muduo20/net/Buffer.h"
#include "muduo20/net/Callbacks.h"
#include "muduo20/net/Connector.h"
#include "muduo20/net/EventLoop.h"
#include "muduo20/net/InetAddress.h"
#include "muduo20/net/SocketsOps.h"
#include "muduo20/net/TcpConnection.h"

#include <atomic>
#include <bits/chrono.h>
#include <cassert>
#include <format>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace muduo20::net {
using namespace std::chrono_literals;

namespace detail {

void removeConnection(EventLoop *loop, const TcpConnectionPtr &conn) {
    loop->runInLoop([conn] { conn->connectDestroyed(); });
}

void removeConnector(
    [[maybe_unused]] const std::shared_ptr<Connector> &connector) {
    // Intentionally empty: keep connector alive until delayed callback fires.
}

} // namespace detail

TcpClient::TcpClient(EventLoop *loop, const InetAddress &serverAddr,
                     std::string nameArg)
    : loop_(muduo20::CheckNotNull("loop", loop)),
      connector_(std::make_shared<Connector>(loop, serverAddr)),
      name_(std::move(nameArg)),
      connectionCallback_(ConnectionCallback(defaultConnectionCallback)),
      messageCallback_(MessageCallback(defaultMessageCallback)) {
    connector_->setNewConnectionCallback(
        [this](int sockfd) { newConnection(sockfd); });

    muduo20::logInfo("TcpClient::TcpClient[{}] - connector {}", name_,
                     static_cast<const void *>(get_pointer(connector_)));
}

TcpClient::~TcpClient() {
    muduo20::logInfo("TcpClient::~TcpClient[{}] - connector {}", name_,
                     static_cast<const void *>(get_pointer(connector_)));
    connector_->stop();

    TcpConnectionPtr conn;
    {
        std::scoped_lock lock(mutex_);
        conn = connection_;
    }

    if (conn) {
        assert(loop_ == conn->getLoop());
        if (loop_->isInLoopThread()) {
            conn->setCloseCallback([](const TcpConnectionPtr &) {});
            conn->connectDestroyed();
        } else {
            loop_->runInLoop([conn, loop = loop_] {
                conn->setCloseCallback([loop](const TcpConnectionPtr &c) {
                    detail::removeConnection(loop, c);
                });
            });
            conn->forceClose();
        }
    } else {
        (void)loop_->runAfter(1s, [connector = connector_] {
            detail::removeConnector(connector);
        });
    }
}

void TcpClient::connect() {
    muduo20::logInfo("TcpClient::connect[{}] - connecting to {}", name_,
                     connector_->serverAddress().toIpPort());
    connect_.store(true, std::memory_order_release);
    connector_->start();
}

void TcpClient::disconnect() {
    connect_.store(false, std::memory_order_release);
    std::scoped_lock lock(mutex_);
    if (connection_) {
        connection_->shutdown();
    }
}

void TcpClient::stop() {
    connect_.store(false, std::memory_order_release);
    connector_->stop();
}

void TcpClient::newConnection(int sockfd) {
    loop_->assertInLoopThread();
    const InetAddress peerAddr(sockets::getPeerAddr(sockfd));
    const std::string connName =
        std::format("{}:{}#{}", name_, peerAddr.toIpPort(), nextConnId_++);

    const InetAddress localAddr(sockets::getLocalAddr(sockfd));
    auto conn = std::make_shared<TcpConnection>(loop_, connName, sockfd,
                                                localAddr, peerAddr);

    conn->setConnectionCallback([this](const TcpConnectionPtr &c) {
        if (this->connectionCallback_) {
            this->connectionCallback_(c);
        }
    });
    conn->setMessageCallback(
        [this](const TcpConnectionPtr &c, Buffer *b, Timestamp t) {
            if (this->messageCallback_) {
                this->messageCallback_(c, b, t);
            }
        });
    conn->setWriteCompleteCallback([this](const TcpConnectionPtr &c) {
        if (this->writeCompleteCallback_) {
            this->writeCompleteCallback_(c);
        }
    });
    conn->setCloseCallback(
        [this](const TcpConnectionPtr &c) { removeConnection(c); });

    {
        std::scoped_lock lock(mutex_);
        connection_ = conn;
    }
    conn->connectEstablished();
}

void TcpClient::removeConnection(const TcpConnectionPtr &conn) {
    loop_->assertInLoopThread();
    assert(loop_ == conn->getLoop());

    {
        std::scoped_lock lock(mutex_);
        assert(connection_ == conn);
        connection_.reset();
    }

    loop_->runInLoop([conn] { conn->connectDestroyed(); });
    if (retry_.load(std::memory_order_acquire) &&
        connect_.load(std::memory_order_acquire)) {
        muduo20::logInfo("TcpClient::removeConnection[{}] - reconnecting to {}",
                         name_, connector_->serverAddress().toIpPort());
        connector_->restart();
    }
}

} // namespace muduo20::net
