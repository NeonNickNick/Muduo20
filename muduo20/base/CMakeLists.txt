set(base_SRCS
  CurrentThread.cc
  Date.cc
  Exception.cc
  FileUtil.cc
  LogFile.cc
  Logging.cc
  LogStream.cc
  AsyncLogging.cc
  ProcessInfo.cc
  Thread.cc
  ThreadPool.cc
  TimeZone.cc
  Timestamp.cc
)

add_library(muduo20_base ${base_SRCS})

target_include_directories(muduo20_base
  BEFORE PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# For C++20, we don't need to link pthread explicitly on many platforms
# but keeping it for compatibility
if(NOT WIN32)
  target_link_libraries(muduo20_base PUBLIC pthread ZLIB::ZLIB)
else()
  target_link_libraries(muduo20_base PUBLIC ZLIB::ZLIB)
endif()

install(TARGETS muduo20_base
  EXPORT muduo20Targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
file(GLOB HEADERS "*.h")
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/muduo20/base)
install(DIRECTORY coroutine/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/muduo20/base/coroutine FILES_MATCHING PATTERN "*.h")

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
