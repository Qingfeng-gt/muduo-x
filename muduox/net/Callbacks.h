//
// Created by Administrator on 2026/7/15.
//

#ifndef MUDUOX_CALLBACKS_H
#define MUDUOX_CALLBACKS_H

#include <functional>
#include <memory>

namespace muduox {

class TcpConnection;
class Buffer;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

// ---- 用户回调类型 ----

/// 连接建立/断开回调
using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;

/// 消息到达回调: (连接, 接收缓冲区)
using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*)>;

/// 写完成回调
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;

} // namespace muduox

#endif // MUDUOX_CALLBACKS_H
