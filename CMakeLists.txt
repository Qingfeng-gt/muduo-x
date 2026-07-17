cmake_minimum_required(VERSION 3.16)
project(muduo-cross VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 平台检测
if(WIN32)
    set(MUDUO_PLATFORM "windows")
    set(PLATFORM_LIBS ws2_32 mswsock)
    set(MUDUO_POLLER_SRC muduox/net/poller/IocpPoller.cpp)
    add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN)
elseif(APPLE)
    set(MUDUO_PLATFORM "macos")
    set(PLATFORM_LIBS pthread)
   set(MUDUO_POLLER_SRC muduox/net/poller/EpollPoller.cpp)
else()
    set(MUDUO_PLATFORM "linux")
    set(PLATFORM_LIBS pthread)
    set(MUDUO_POLLER_SRC muduox/net/poller/EpollPoller.cpp)
endif()

message(STATUS "muduo-cross platform: ${MUDUO_PLATFORM}")

add_library(muduo_net STATIC
        muduox/net/tcp/InetAddress.cpp
        muduox/net/tcp/Socket.cpp
        muduox/net/tcp/SocketOps.cpp
        muduox/net/core/Channel.cpp
        muduox/net/core/EventLoop.cpp
        muduox/net/core/Poller.cpp
        muduox/net/tcp/Acceptor.cpp
        muduox/net/tcp/TcpConnection.cpp
        muduox/net/tcp/TcpServer.cpp
        muduox/net/tcp/Buffer.cpp
        muduox/base/logging/LogFile.cpp
        muduox/base/logging/AsyncLogging.cpp
        muduox/base/logging/Logging.cpp
        ${MUDUO_POLLER_SRC}
)

target_include_directories(muduo_net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(muduo_net PUBLIC ${PLATFORM_LIBS})

add_executable(echo_server examples/echo/main.cpp)
target_link_libraries(echo_server muduo_net)

add_executable(echo_client examples/echo/client.cpp)
target_link_libraries(echo_client muduo_net)