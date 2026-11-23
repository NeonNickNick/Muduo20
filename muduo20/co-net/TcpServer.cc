#include "muduo20/co-net/TcpServer.h"

#include "muduo20/base/Logging.h"
#include "muduo20/base/Timestamp.h"
#include "muduo20/co-net/Acceptor.h"
#include "muduo20/co-net/Callbacks.h"
#include "muduo20/co-net/EventLoop.h"
#include "muduo20/co-net/EventLoopThreadPool.h"
#include "muduo20/co-net/SocketsOps.h"
#include "muduo20/co-net/TcpConnection.h"

#include <atomic>
#include <cassert>
#include <cstddef>
#include <format>
#include <memory>
#include <string>
#include <utility>

namespace muduo20::co_net {

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr,
                     std::string nameArg, Option option)
    : loop_(muduo20::CheckNotNull("loop", loop)),
      ipPort_(listenAddr.toIpPort()), name_(std::move(nameArg)),
      acceptor_(std::make_unique<Acceptor>(loop, listenAddr,
                                           option == Option::kReusePort)),
      threadPool_(std::make_shared<EventLoopThreadPool>(loop, name_)),
      connectionCallback_(ConnectionCallback(defaultConnectionCallback)),
      messageCallback_(MessageCallback(defaultMessageCallback)) {
    acceptor_->setNewConnectionCallback(
        [this](int sockfd, const InetAddress &peerAddr) {
            newConnection(sockfd, peerAddr);
        });
}

TcpServer::~TcpServer() {
    loop_->assertInLoopThread();
    muduo20::logTrace("TcpServer::~TcpServer [{}] destructing", name_);

    for (auto &[_, conn] : connections_) {
        TcpConnectionPtr guard(conn);
        conn.reset();
        guard->getLoop()->runInLoop([guard] { guard->connectDestroyed(); });
    }
}

void TcpServer::setThreadNum(int numThreads) {
    assert(numThreads >= 0);
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start() {
    if (started_.fetch_add(1) == 0) {
        threadPool_->start(std::move(threadInitCallback_));
        assert(!acceptor_->listening());
        loop_->runInLoop([this] { acceptor_->listen(); });
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr) {
    loop_->assertInLoopThread();
    EventLoop *ioLoop = threadPool_->getNextLoop();

    auto connName =
        std::format("{}-{}#{}", name_, ipPort_, static_cast<long>(nextConnId_));
    ++nextConnId_;

    muduo20::logInfo(
        "TcpServer::newConnection [{}] - new connection [{}] from {}", name_,
        connName, peerAddr.toIpPort());

    InetAddress localAddr(sockets::getLocalAddr(sockfd));
    auto conn = std::make_shared<TcpConnection>(ioLoop, connName, sockfd,
                                                localAddr, peerAddr);
    connections_[connName] = conn;

    conn->setConnectionCallback([this](const TcpConnectionPtr &ptr) {
        if (this->connectionCallback_) {
            this->connectionCallback_(ptr);
        }
    });
    conn->setMessageCallback(
        [this](const TcpConnectionPtr &ptr, Buffer *buf, Timestamp stamp) {
            if (this->messageCallback_) {
                this->messageCallback_(ptr, buf, stamp);
            }
        });
    conn->setWriteCompleteCallback([this](const TcpConnectionPtr &ptr) {
        if (this->writeCompleteCallback_) {
            this->writeCompleteCallback_(ptr);
        }
    });
    conn->setCloseCallback(
        [this](const TcpConnectionPtr &c) { removeConnection(c); });

    ioLoop->runInLoop([conn] { conn->connectEstablished(); });
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn) {
    loop_->runInLoop([this, conn] { removeConnectionInLoop(conn); });
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn) {
    loop_->assertInLoopThread();
    muduo20::logInfo("TcpServer::removeConnectionInLoop [{}] - connection {}",
                     name_, conn->name());

    const size_t erased = connections_.erase(conn->name());
    (void)erased;
    assert(erased == 1);

    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop([conn] { conn->connectDestroyed(); });
}

} // namespace muduo20::co_net
