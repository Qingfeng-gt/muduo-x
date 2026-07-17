//
// Created by Administrator on 2026/7/15.
//

#include "Poller.h"
#ifdef _WIN32
#include "muduox/net/poller/IocpPoller.h"
#else
#include "muduox/net/poller/EpollPoller.h"
#endif

namespace muduox {

std::unique_ptr<Poller> Poller::create(EventLoop* loop) {
#ifdef _WIN32
    return std::make_unique<IocpPoller>(loop);
#else
    return std::make_unique<EpollPoller>(loop);
#endif
}

} // namespace muduox
