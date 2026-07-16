//
// Created by Administrator on 2026/7/15.
//

#include "Poller.h"
#ifdef _WIN32
#include "IocpPoller.h"
#else
#include "EpollPoller.h"
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
