cmake_minimum_required(VERSION 3.16)
project(muduo-cross VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 平台检测
if(WIN32)
    set(MUDUO_PLATFORM "windows")
    set(PLATFORM_LIBS ws2_32 mswsock)
    set(MUDUO_POLLER_SRC muduox/net/IocpPoller.cpp)
    add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN)
elseif(APPLE)
    set(MUDUO_PLATFORM "macos")
    set(PLATFORM_LIBS pthread)
   set(MUDUO_POLLER_SRC muduox/net/EpollPoller.cpp)
else()
    set(MUDUO_PLATFORM "linux")
    set(PLATFORM_LIBS pthread)
   set(MUDUO_POLLER_SRC muduox/net/EpollPoller.cpp)
endif()

message(STATUS "muduo-cross platform: ${MUDUO_PLATFORM}")

add_library(muduo_net STATIC
        muduox/net/InetAddress.cpp
        muduox/net/Socket.cpp
        muduox/net/SocketOps.cpp
        muduox/net/Channel.cpp
        muduox/net/EventLoop.cpp
        muduox/net/Poller.cpp
        muduox/net/Acceptor.cpp
        muduox/net/TcpConnection.cpp
        muduox/net/TcpServer.cpp
        muduox/net/Buffer.cpp
        ${MUDUO_POLLER_SRC}
)

target_include_directories(muduo_net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(muduo_net PUBLIC ${PLATFORM_LIBS})

add_executable(echo_server examples/echo/main.cpp)
target_link_libraries(echo_server muduo_net)

add_executable(echo_client examples/echo/client.cpp)
target_link_libraries(echo_client muduo_net)