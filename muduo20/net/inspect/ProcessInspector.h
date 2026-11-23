#pragma once

#include "muduo20/base/ProcessInfo.h"
#include "muduo20/net/inspect/Inspector.h"

#include <string_view>

namespace muduo20::net {

class ProcessInspector {
  public:
    void registerCommands(Inspector *ins);

    static std::string overview(HttpRequest::Method,
                                const Inspector::ArgList &);
    static std::string pid(HttpRequest::Method, const Inspector::ArgList &);
    static std::string procStatus(HttpRequest::Method,
                                  const Inspector::ArgList &);
    static std::string openedFiles(HttpRequest::Method,
                                   const Inspector::ArgList &);
    static std::string threads(HttpRequest::Method, const Inspector::ArgList &);

  private:
    static const std::string username_;
};

namespace inspect {

std::string uptime(Timestamp now, Timestamp start, bool showMicroseconds);
long getLong(const std::string &content, std::string_view key);
ProcessInfo::CpuTime getCpuTime(std::string_view data);

} // namespace inspect

} // namespace muduo20::net
