//
// Created by Administrator on 2026/7/15.
//

#include "Poller.h"
#include "EpollPoller.h"

namespace muduox {

std::unique_ptr<Poller> Poller::create(EventLoop* loop) {
#ifdef _WIN32
    // TODO: return std::make_unique<IocpPoller>(loop);
    return nullptr;  // Windows 暂不支持
#else
    return std::make_unique<EpollPoller>(loop);
#endif
}

} // namespace muduox
