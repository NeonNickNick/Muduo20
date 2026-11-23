set(co_net_SRCS
  Acceptor.cc
  boilerplate.cc
  Buffer.cc
  Channel.cc
  Connector.cc
  EventLoop.cc
  EventLoopThread.cc
  EventLoopThreadPool.cc
  InetAddress.cc
  Poller.cc
  Socket.cc
  SocketsOps.cc
  TcpClient.cc
  TcpConnection.cc
  TcpServer.cc
  Timer.cc
  TimerQueue.cc
  ZlibStream.cc
  http/HttpContext.cc
  http/HttpResponse.cc
  http/HttpServer.cc
  inspect/Inspector.cc
  inspect/PerformanceInspector.cc
  inspect/ProcessInspector.cc
  inspect/SystemInspector.cc
  poller/DefaultPoller.cc
  poller/EPollPoller.cc
  poller/PollPoller.cc
)

add_library(muduo20_co_net ${co_net_SRCS})
add_library(muduo20::co_net ALIAS muduo20_co_net)

target_include_directories(muduo20_co_net
  BEFORE PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(muduo20_co_net PUBLIC muduo20_base)

install(TARGETS muduo20_co_net
  EXPORT muduo20Targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY .
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/muduo20/co-net
  FILES_MATCHING PATTERN "*.h"
)
