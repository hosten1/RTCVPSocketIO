// 启用Socket.IO调试日志，并添加时间戳
process.env.DEBUG = 'engine:*,socket.io*';
process.env.DEBUG_COLORS = 'true'; // 保留彩色输出
process.env.DEBUG_FD = '1'; // 输出到stdout

// 自定义DEBUG日志格式，添加时间戳
const debug = require('debug');
const oldLog = debug.log;
debug.log = function() {
  const timestamp = new Date().toISOString();
  const args = Array.prototype.slice.call(arguments);
  args.unshift(`${timestamp} `);
  oldLog.apply(debug, args);
};

const http = require('http');
const https = require('https');
const { Server } = require('socket.io');
const fs = require('fs');
const path = require('path');

// 处理静态文件请求的通用处理函数
const handleRequest = (req, res) => {
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
};

// 创建HTTP服务器
const httpServer = http.createServer(handleRequest);

// 检查证书文件是否存在
let httpsServer = null;
let hasHttpsCert = false;

try {
  // 检查证书文件是否存在
  fs.accessSync('./key.pem', fs.constants.F_OK);
  fs.accessSync('./cert.pem', fs.constants.F_OK);
  hasHttpsCert = true;
} catch (err) {
  console.warn('⚠️  HTTPS certificate files not found, HTTPS server will not be started');
}

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

// 如果有证书，创建HTTPS服务器
if (hasHttpsCert) {
  // 创建HTTPS服务器选项
  const httpsOptions = {
    key: fs.readFileSync('./key.pem'),
    cert: fs.readFileSync('./cert.pem')
  };
  
  // 创建HTTPS服务器
  httpsServer = https.createServer(httpsOptions, handleRequest);
  
  // 将Socket.IO附加到HTTPS服务器
  io.attach(httpsServer);
}

// 添加底层连接事件监听
io.engine.on('connection', (conn) => {
    console.log('=== 底层连接建立 ===');
    console.log('连接ID:', conn.id);
    console.log('传输方式:', conn.transport.name);
    console.log('连接时间:', new Date().toISOString());
    console.log('远程地址:', conn.remoteAddress);
    console.log('Engine.IO版本:', conn.protocol); // 显示Engine.IO版本
    console.log('=== 连接信息结束 ===');
    
    // 监听连接关闭
    conn.on('close', (reason) => {
        console.log('=== 底层连接关闭 ===');
        console.log('连接ID:', conn.id);
        console.log('关闭原因:', reason);
        console.log('关闭时间:', new Date().toISOString());
        console.log('=== 关闭信息结束 ===');
    });
    
    // 监听连接错误
    conn.on('error', (error) => {
        console.error('=== 连接错误 ===');
        console.error('连接ID:', conn.id);
        console.error('错误:', error);
        console.error('=== 错误信息结束 ===');
    });
});

// 添加连接错误处理
io.engine.on('connection_error', (error) => {
    console.error('=== 连接错误 ===');
    console.error('错误类型:', error.type);
    console.error('错误消息:', error.message);
    if (error.context) {
        console.error('错误上下文:', error.context);
    }
    if (error.code) {
        console.error('错误代码:', error.code);
    }
    console.error('=== 错误信息结束 ===');
});

// 定义共享的连接处理逻辑
const handleConnection = (socket) => {
    // 获取当前命名空间
    const namespace = socket.nsp.name;
    
    console.log('=== 新Socket.IO连接 ===');
    console.log('Socket ID:', socket.id);
    console.log('命名空间:', namespace);
    console.log('连接时间:', new Date().toISOString());
    console.log('传输方式:', socket.conn.transport.name);
    console.log('Engine.IO版本:', socket.conn.protocol);
    console.log('=== Socket.IO连接信息结束 ===');
    
    // 发送欢迎消息  同时处理ack客户端的返回
    socket.emit('welcome', { message: `Welcome to Socket.IO server!`, socketId: socket.id }, (ackData) => {
      console.log('Welcome ACK from client:', ackData);
    });
    
    // 广播用户连接事件给当前命名空间的所有客户端
    socket.nsp.emit('userConnected', { socketId: socket.id, timestamp: new Date().toISOString(), namespace: namespace });
  
  // 监听聊天消息
  socket.on('chatMessage', (data, callback) => {
    console.log(`[${namespace}] Chat message from`, socket.id, ':', data);
    
    // 回复ACK
    if (callback) {
      callback({ status: 'received', timestamp: new Date().toISOString() });
    }
    
    // 广播消息给当前命名空间的所有客户端
    socket.nsp.emit('chatMessage', {
      sender: socket.id,
      message: data.message,
      timestamp: new Date().toISOString(),
      namespace: namespace
    });
  });
  
  // 监听自定义事件
  socket.on('customEvent', (data, callback) => {
    console.log(`[${namespace}] Custom event:`, data);
    
    // 检查callback是否为函数
    if (typeof callback === 'function') {
      callback({ success: true, response: `Processed: ${JSON.stringify(data)}`, namespace: namespace });
    } else {
      console.log(`[${namespace}] No callback provided for customEvent`);
    }
  });
  
  // 监听二进制消息
  socket.on('binaryEvent', (data, callback) => {
    // 不要直接打印包含二进制数据的对象
    console.log(`[${namespace}] Binary event received from`, socket.id);
    
    // 回复ACK - 确保只返回简单的JSON数据，不包含二进制数据
    if (typeof callback === 'function') {
      console.log(`[${namespace}] Sending ACK for binaryEvent`);
      // 只返回简单的确认信息，不包含二进制数据
      let response = {
        success: true,
        timestamp: new Date().toISOString(),
        message: 'Binary data received successfully',
        receivedSize: data.binaryData ? data.binaryData.length : 0,
        sender: socket.id,
        namespace: namespace
      };
      
      try {
        callback(response);
        console.log(`[${namespace}] ACK sent successfully`);
      } catch (error) {
        console.error(`[${namespace}] Error sending ACK:`, error);
      }
    }
    
    // 只广播给除了发送者之外的其他客户端
    socket.broadcast.emit('binaryEvent', data);
  });
  
  // 监听二进制ACK测试
  socket.on('binaryAckTest', (data, callback) => {
    console.log(`[${namespace}] Binary ACK test received:`, data);
    
    // 检查是否包含二进制数据
    let binaryData = null;
    let textData = null;
    
    if (typeof data === 'object' && data !== null) {
      // 检查是否有二进制属性
      if (data.binaryData) {
        binaryData = data.binaryData;
        textData = data.text;
        console.log(`[${namespace}] Binary data size:`, binaryData.length, 'bytes');
      }
    }
    
    // 模拟处理二进制数据 - 只返回元数据，不包含原始二进制数据
    let processedData = {
      receivedSize: binaryData ? binaryData.length : 0,
      text: textData,
      processedAt: new Date().toISOString(),
      result: 'success',
      checksum: Math.random().toString(36).substring(7),
      namespace: namespace
    };
    
    // 返回ACK
    if (callback) {
      callback(processedData);
    }
  });
  
  // 监听心跳消息（可选）
  socket.on('heartbeat', (data) => {
    console.log(`[${namespace}] Heartbeat from`, socket.id, ':', data);
    
    // 直接回复心跳ACK
    socket.emit('heartbeat', { received: true, timestamp: new Date().toISOString(), namespace: namespace });
  });
  
    // 监听断开连接
    socket.on('disconnect', (reason) => {
        console.log('=== Socket.IO断开连接 ===');
        console.log('Socket ID:', socket.id);
        console.log('命名空间:', namespace);
        console.log('断开原因:', reason);
        console.log('断开时间:', new Date().toISOString());
        console.log('=== 断开连接结束 ===');
        // 广播用户断开连接事件给当前命名空间的所有客户端
        socket.nsp.emit('userDisconnected', { socketId: socket.id, reason: reason, timestamp: new Date().toISOString(), namespace: namespace });
    });
    
    // 监听断开连接原因
    socket.on('disconnecting', (reason) => {
        console.log('=== Socket.IO正在断开连接 ===');
        console.log('Socket ID:', socket.id);
        console.log('命名空间:', namespace);
        console.log('断开原因:', reason);
        console.log('断开时间:', new Date().toISOString());
        console.log('=== 正在断开连接结束 ===');
    });
    
    // 监听连接错误
    socket.on('error', (error) => {
        console.error('=== Socket.IO错误 ===');
        console.error('Socket ID:', socket.id);
        console.error('命名空间:', namespace);
        console.error('错误:', error);
        console.error('错误时间:', new Date().toISOString());
        console.error('=== 错误信息结束 ===');
    });
  
  // 为/test命名空间添加特殊的测试事件
  if (namespace === '/test') {
    socket.on('testEvent', (data, callback) => {
      console.log('[test] Special test event received:', data);
      if (callback) {
        callback({ testResult: 'passed', message: 'This is a special response for /test namespace', data: data });
      }
    });
  }
}

// 使用中间件处理所有命名空间的连接
// 这会自动允许所有命名空间
io.use((socket, next) => {
  console.log(`中间件：接收到来自命名空间 ${socket.nsp.name} 的连接请求`);
  next(); // 允许连接
});

// 监听所有命名空间的连接事件
io.on('connection', handleConnection);

// 监听自定义命名空间的连接事件
// Socket.IO 4.x 会自动处理所有命名空间，不需要显式注册
// 但我们可以显式监听特定命名空间
io.of('/chat').on('connection', handleConnection);
io.of('/game').on('connection', handleConnection);
io.of('/test').on('connection', handleConnection);

// 启动服务器
const HTTP_PORT = process.env.HTTP_PORT || 3000;
const HTTPS_PORT = process.env.HTTPS_PORT || 3443;

// 启动HTTP服务器
httpServer.listen(HTTP_PORT, '0.0.0.0', () => {
  console.log(`=== HTTP服务器已启动 ===`);
  console.log(`Socket.IO HTTP server running at http://0.0.0.0:${HTTP_PORT}`);
  console.log(`WebSocket HTTP endpoint: ws://0.0.0.0:${HTTP_PORT}`);
  console.log(`HTTP Accessible at: http://localhost:${HTTP_PORT} and http://127.0.0.1:${HTTP_PORT}`);
});

// 启动HTTPS服务器（如果有证书）
if (httpsServer) {
  httpsServer.listen(HTTPS_PORT, '0.0.0.0', () => {
    console.log(`=== HTTPS服务器已启动 ===`);
    console.log(`Socket.IO HTTPS server running at https://0.0.0.0:${HTTPS_PORT}`);
    console.log(`WebSocket HTTPS endpoint: wss://0.0.0.0:${HTTPS_PORT}`);
    console.log(`HTTPS Accessible at: https://localhost:${HTTPS_PORT} and https://127.0.0.1:${HTTPS_PORT}`);
  });
}
