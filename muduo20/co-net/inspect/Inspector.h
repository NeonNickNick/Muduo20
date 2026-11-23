#pragma once

#include "muduo20/co-net/Callbacks.h"
#include "muduo20/co-net/InetAddress.h"
#include "muduo20/co-net/TcpConnection.h"
#include "muduo20/co-net/http/HttpRequest.h"
#include "muduo20/co-net/http/HttpServer.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace muduo20::co_net {

class ProcessInspector;
class PerformanceInspector;
class SystemInspector;

class Inspector {
  public:
    using ArgList = std::vector<std::string>;
    using Callback =
        CallbackFunction<std::string(HttpRequest::Method, const ArgList &)>;

    Inspector(EventLoop *loop, const InetAddress &httpAddr,
              const std::string &name);
    ~Inspector();

    void add(const std::string &moduleName, const std::string &command,
             Callback cb, const std::string &help);
    template <typename F>
        requires CallbackBindable<F, Callback>
    void add(const std::string &moduleName, const std::string &command, F &&cb,
             const std::string &help) {
        add(moduleName, command, Callback(std::forward<F>(cb)), help);
    }
    void remove(const std::string &moduleName, const std::string &command);

  private:
    using CommandList = std::map<std::string, Callback>;
    using HelpList = std::map<std::string, std::string>;

    void start();
    void onRequest(const HttpRequest &req, HttpResponse *resp);

    HttpServer server_;
    std::unique_ptr<ProcessInspector> processInspector_;
    std::unique_ptr<PerformanceInspector> performanceInspector_;
    std::unique_ptr<SystemInspector> systemInspector_;

    std::mutex mutex_;
    std::map<std::string, CommandList> modules_;
    std::map<std::string, HelpList> helps_;
};

} // namespace muduo20::co_net
