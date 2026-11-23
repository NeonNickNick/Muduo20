#include "muduo20/base/Logging.h"
#include "muduo20/net/EventLoop.h"
#include "muduo20/net/InetAddress.h"
#include "muduo20/net/http/HttpRequest.h"
#include "muduo20/net/http/HttpResponse.h"
#include "muduo20/net/http/HttpServer.h"

#include <cstdlib>
#include <string_view>

namespace muduo20::net {
namespace {

bool benchmarkMode = false;

void onRequest(const HttpRequest &req, HttpResponse *resp) {
  if (req.path() == "/" || req.path() == "/hello") {
    resp->setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
    resp->setStatusMessage("OK");
    resp->setContentType("text/plain");
    resp->addHeader("Server", "muduo20-cpp20");
    resp->setBody("hello, world!\n");
    return;
  }

  if (!benchmarkMode) {
    muduo20::logWarn("Not found path={}", req.path());
  }
  resp->setStatusCode(HttpResponse::HttpStatusCode::k404NotFound);
  resp->setStatusMessage("Not Found");
  resp->setCloseConnection(true);
}

} // namespace
} // namespace muduo20::net

int main(int argc, char *argv[]) {
  using muduo20::Logger;
  using muduo20::net::EventLoop;
  using muduo20::net::HttpServer;
  using muduo20::net::InetAddress;

  int numThreads = 0;
  int port = 8000;
  if (argc > 1) {
    muduo20::net::benchmarkMode = true;
    Logger::setLogLevel(Logger::WARN);
    numThreads = std::atoi(argv[1]);
  }
  if (argc > 2) {
    port = std::atoi(argv[2]);
  }

  EventLoop loop;
  HttpServer server(&loop, InetAddress(static_cast<uint16_t>(port)), "http-bench");
  server.setHttpCallback(muduo20::net::onRequest);
  server.setThreadNum(numThreads);
  server.start();
  loop.loop();
  return 0;
}
