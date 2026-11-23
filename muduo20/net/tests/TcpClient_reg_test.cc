#include "muduo20/net/EventLoop.h"
#include "muduo20/net/EventLoopThread.h"
#include "muduo20/net/TcpClient.h"
#include "muduo20/net/TcpServer.h"

#include "muduo20/base/CurrentThread.h"
#include "muduo20/base/Logging.h"

#include <gtest/gtest.h>

#include <cerrno>
#include <chrono>
#include <string_view>

#include <sys/socket.h>
#include <unistd.h>

namespace {
using namespace std::string_view_literals;

class TcpClientRegressionTest : public ::testing::Test {};

} // namespace

TEST_F(TcpClientRegressionTest, StopInSameEventIteration) {
  using namespace std::chrono_literals;

  muduo20::net::EventLoop loop;
  muduo20::net::InetAddress serverAddr("127.0.0.1"sv, 2);
  muduo20::net::TcpClient client(&loop, serverAddr, "TcpClient");

  (void)loop.runAfter(0ms, [&client] { client.stop(); });
  (void)loop.runAfter(300ms, [&loop] { loop.quit(); });
  client.connect();
  muduo20::CurrentThread::sleepUsec(100 * 1000);
  loop.loop();
  SUCCEED();
}

TEST_F(TcpClientRegressionTest, DestructInDifferentThread) {
  muduo20::Logger::setLogLevel(muduo20::Logger::LogLevel::DEBUG);
  muduo20::net::EventLoopThread loopThread;
  {
    muduo20::net::InetAddress serverAddr("127.0.0.1"sv, 1234);
    muduo20::net::TcpClient client(loopThread.startLoop(), serverAddr,
                                 "TcpClient");
    client.connect();
    muduo20::CurrentThread::sleepUsec(300 * 1000);
    client.disconnect();
  }
  muduo20::CurrentThread::sleepUsec(500 * 1000);
  SUCCEED();
}

TEST_F(TcpClientRegressionTest, DestructWhenConnectedAndUnique) {
  using namespace std::chrono_literals;

  muduo20::Logger::setLogLevel(muduo20::Logger::LogLevel::DEBUG);
  muduo20::net::EventLoop loop;
  const auto port = static_cast<uint16_t>(32000 + (::getpid() % 10000));
  muduo20::net::InetAddress listenAddr(port, true);
  muduo20::net::TcpServer server(&loop, listenAddr, "TcpClientReg2Server");
  server.setConnectionCallback([](const muduo20::net::TcpConnectionPtr &) {});
  server.setMessageCallback([](const muduo20::net::TcpConnectionPtr &,
                               muduo20::net::Buffer *, muduo20::Timestamp) {});
  server.start();

  std::unique_ptr<muduo20::net::TcpClient> client;
  std::atomic<bool> connected{false};

  (void)loop.runAfter(50ms, [&] {
    client = std::make_unique<muduo20::net::TcpClient>(
        &loop, muduo20::net::InetAddress("127.0.0.1"sv, port),
        "TcpClientReg2");
    client->setConnectionCallback(
        [&](const muduo20::net::TcpConnectionPtr &conn) {
          if (conn->connected()) {
            connected.store(true, std::memory_order_release);
            client.reset();
          }
        });
    client->connect();
  });
  (void)loop.runAfter(1s, [&loop] { loop.quit(); });
  loop.loop();

  EXPECT_TRUE(connected.load(std::memory_order_acquire));
}
