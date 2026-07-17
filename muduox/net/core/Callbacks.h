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

using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;

} // namespace muduox

#endif // MUDUOX_CALLBACKS_H
