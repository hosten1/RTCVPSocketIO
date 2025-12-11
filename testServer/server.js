const http = require('http');
const { Server } = require('socket.io');
const fs = require('fs');
const path = require('path');

// 创建HTTP服务器
const httpServer = http.createServer((req, res) => {
  // 处理静态文件请求
  let filePath = '.' + req.url;
  if (filePath === './') {
    filePath = './test.html';
  }

  const extname = String(path.extname(filePath)).toLowerCase();
  const contentType = {
    '.html': 'text/html',
    '.js': 'text/javascript',
    '.css': 'text/css',
    '.json': 'application/json',
    '.png': 'image/png',
    '.jpg': 'image/jpg',
    '.gif': 'image/gif',
    '.svg': 'image/svg+xml',
    '.wav': 'audio/wav',
    '.mp4': 'video/mp4',
    '.woff': 'application/font-woff',
    '.ttf': 'application/font-ttf',
    '.eot': 'application/vnd.ms-fontobject',
    '.otf': 'application/font-otf',
    '.wasm': 'application/wasm'
  }[extname] || 'application/octet-stream';

  fs.readFile(filePath, (error, content) => {
    if (error) {
      if (error.code === 'ENOENT') {
        res.writeHead(404, { 'Content-Type': 'text/html' });
        res.end('<h1>404 Not Found</h1>', 'utf-8');
      } else {
        res.writeHead(500);
        res.end('Sorry, check with the site admin for error: ' + error.code + ' ..\n');
      }
    } else {
      res.writeHead(200, { 'Content-Type': contentType });
      res.end(content, 'utf-8');
    }
  });
});

// 创建Socket.IO服务器，支持跨域和所有传输方式
const io = new Server(httpServer, {
  cors: {
    origin: '*',
    methods: ['GET', 'POST'],
    credentials: true
  },
  transports: ['websocket', 'polling'],
  allowEIO3: true
});

// 添加底层连接事件监听
io.engine.on('connection', (conn) => {
    console.log('=== 底层连接建立 ===');
    console.log('连接ID:', conn.id);
    console.log('传输方式:', conn.transport.name);
    console.log('连接时间:', new Date().toISOString());
    console.log('远程地址:', conn.remoteAddress);
    console.log('=== 连接信息结束 ===');
});

// 添加连接错误处理
io.engine.on('connection_error', (error) => {
    console.error('=== 连接错误 ===');
    console.error('错误类型:', error.type);
    console.error('错误消息:', error.message);
    if (error.context) {
        console.error('错误上下文:', error.context);
    }
    console.error('=== 错误信息结束 ===');
});

// 监听连接事件
io.on('connection', (socket) => {
    console.log('=== 新客户端连接 ===');
    console.log('客户端ID:', socket.id);
    console.log('连接时间:', new Date().toISOString());
    console.log('传输方式:', socket.conn.transport.name);
    console.log('=== 连接信息结束 ===');
    
    // 发送欢迎消息
    socket.emit('welcome', { message: 'Welcome to Socket.IO server!', socketId: socket.id });
    
    // 广播用户连接事件给所有客户端
    io.emit('userConnected', { socketId: socket.id, timestamp: new Date().toISOString() });
  
  // 监听聊天消息
  socket.on('chatMessage', (data, callback) => {
    console.log('Chat message from', socket.id, ':', data);
    
    // 回复ACK
    if (callback) {
      callback({ status: 'received', timestamp: new Date().toISOString() });
    }
    
    // 广播消息给所有客户端
    io.emit('chatMessage', {
      sender: socket.id,
      message: data.message,
      timestamp: new Date().toISOString()
    });
  });
  
  // 监听断开连接事件
  socket.on('disconnect', (reason) => {
    console.log('Client disconnected:', socket.id, 'Reason:', reason);
    // 广播用户断开连接事件给所有客户端
    io.emit('userDisconnected', { socketId: socket.id, reason: reason, timestamp: new Date().toISOString() });
  });
  
  // 监听自定义事件
  socket.on('customEvent', (data, callback) => {
    console.log('Custom event:', data);
    
    // 检查callback是否为函数
    if (typeof callback === 'function') {
      callback({ success: true, response: `Processed: ${JSON.stringify(data)}` });
    } else {
      console.log('No callback provided for customEvent');
    }
  });
  
  // 定期发送心跳消息
  const interval = setInterval(() => {
    socket.emit('heartbeat', { timestamp: new Date().toISOString() });
  }, 5000);
  
  // 清理定时器
  socket.on('disconnect', () => {
    clearInterval(interval);
  });
});

// 启动服务器
const PORT = process.env.PORT || 3000;
httpServer.listen(PORT, '0.0.0.0', () => {
  console.log(`Socket.IO server running at http://0.0.0.0:${PORT}`);
  console.log(`WebSocket endpoint: ws://0.0.0.0:${PORT}`);
  console.log(`Accessible at: http://localhost:${PORT} and http://127.0.0.1:${PORT}`);
});
