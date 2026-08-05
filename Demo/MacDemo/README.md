
# LYMVPSocketIO Mac Demo

这是一个 Mac 命令行 Demo，展示如何使用 LYMVPSocketIO 库连接 Socket.IO 服务端。

## 功能演示

- ✅ 连接/断开 Socket.IO 服务器
- ✅ Socket.IO v2 和 v3 协议支持
- ✅ 发送/接收文本消息
- ✅ 发送/接收二进制数据
- ✅ ACK 确认机制
- ✅ 广播消息
- ✅ 命名空间支持
- ✅ 房间功能

## 运行方式

### 1. 启动测试服务器

```bash
# 启动 v2 服务
cd test-server/v2
npm install
node server.js

# 启动 v3 服务（另一个终端）
cd test-server/v3
npm install
node server.js
```

### 2. 构建并运行 Demo

```bash
cd LYMVPSocketIO/LYMVPSocketIO
mkdir -p build && cd build
cmake ..
make socketio_demo

# 运行 Demo
./test/socketio_demo
```

### 3. 使用方式

启动后会显示菜单，输入数字选择功能：

```
=== Socket.IO Demo ===
1. 连接 V2 服务器 (localhost:3002)
2. 连接 V3 服务器 (localhost:3003)
3. 发送文本消息
4. 发送二进制消息
5. 发送带 ACK 的消息
6. 加入房间
7. 发送房间消息
8. 断开连接
9. 退出
请选择:
```
