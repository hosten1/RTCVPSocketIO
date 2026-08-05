const express = require('express');
const http = require('http');
const { Server } = require('socket.io');

const app = express();
const server = http.createServer(app);
const io = new Server(server, {
  transports: ['websocket', 'polling'],
  pingInterval: 25000,
  pingTimeout: 20000,
  cors: {
    origin: "*",
    methods: ["GET", "POST"]
  }
});

const PORT = 3003;

app.get('/', (req, res) => {
  res.send('Socket.IO v3 Test Server Running');
});

let connectedClients = 0;
let messageCount = 0;

io.on('connection', (socket) => {
  const clientId = socket.id;
  connectedClients++;
  console.log(`[v3] Client connected: ${clientId}, total: ${connectedClients}`);

  socket.emit('welcome', {
    message: 'Welcome to Socket.IO v3 test server',
    clientId: clientId,
    timestamp: Date.now()
  });

  socket.on('ping', (data) => {
    console.log(`[v3] Received ping from ${clientId}:`, data);
    socket.emit('pong', {
      timestamp: Date.now(),
      received: data
    });
  });

  socket.on('echo', (data, ack) => {
    console.log(`[v3] Received echo from ${clientId}:`, typeof data);
    if (typeof ack === 'function') {
      ack({
        echoed: data,
        timestamp: Date.now(),
        server: 'v3'
      });
    } else {
      socket.emit('echo_response', {
        echoed: data,
        timestamp: Date.now(),
        server: 'v3'
      });
    }
  });

  socket.on('chat message', (msg) => {
    messageCount++;
    console.log(`[v3] Chat message from ${clientId}: ${JSON.stringify(msg)}, count: ${messageCount}`);
    io.emit('chat message', {
      from: clientId,
      content: msg,
      timestamp: Date.now(),
      messageId: messageCount
    });
  });

  socket.on('binary test', (data, callback) => {
    console.log(`[v3] Binary test from ${clientId}, data type: ${typeof data}`);
    if (Buffer.isBuffer(data)) {
      console.log(`[v3] Binary size: ${data.length} bytes`);
      if (typeof callback === 'function') {
        callback({
          success: true,
          size: data.length,
          firstBytes: Array.from(data.slice(0, 5))
        });
      }
    } else {
      console.log(`[v3] Not a buffer:`, data);
    }
  });

  socket.on('binary with json', (jsonData, binaryData, callback) => {
    console.log(`[v3] Binary with JSON from ${clientId}`);
    console.log(`[v3]   JSON:`, jsonData);
    if (binaryData && Buffer.isBuffer(binaryData)) {
      console.log(`[v3]   Binary size: ${binaryData.length} bytes`);
    }
    if (typeof callback === 'function') {
      callback({
        success: true,
        jsonReceived: jsonData,
        binarySize: binaryData ? binaryData.length : 0,
        timestamp: Date.now()
      });
    }
  });

  socket.on('join room', (room) => {
    console.log(`[v3] ${clientId} joining room: ${room}`);
    socket.join(room);
    socket.to(room).emit('user joined', {
      user: clientId,
      room: room,
      timestamp: Date.now()
    });
    socket.emit('room joined', {
      room: room,
      timestamp: Date.now()
    });
  });

  socket.on('leave room', (room) => {
    console.log(`[v3] ${clientId} leaving room: ${room}`);
    socket.leave(room);
    socket.to(room).emit('user left', {
      user: clientId,
      room: room,
      timestamp: Date.now()
    });
  });

  socket.on('room message', (data) => {
    const { room, message } = data;
    console.log(`[v3] Room message from ${clientId} to ${room}:`, message);
    socket.to(room).emit('room message', {
      from: clientId,
      room: room,
      content: message,
      timestamp: Date.now()
    });
  });

  socket.on('error test', (data) => {
    console.log(`[v3] Error test from ${clientId}`);
    throw new Error('Test error from server');
  });

  socket.on('empty args', (data, ack) => {
    console.log(`[v3] Empty args test from ${clientId}`);
    if (typeof ack === 'function') {
      ack({ success: true, argsType: typeof data });
    }
  });

  socket.on('special chars', (data, ack) => {
    console.log(`[v3] Special chars test from ${clientId}`);
    if (typeof ack === 'function') {
      ack({
        success: true,
        received: data,
        echo: `你好，世界! ${data} 🎉`
      });
    }
  });

  socket.on('large data', (data, ack) => {
    console.log(`[v3] Large data test from ${clientId}, size: ${JSON.stringify(data).length}`);
    if (typeof ack === 'function') {
      ack({
        success: true,
        receivedSize: JSON.stringify(data).length,
        isObject: typeof data === 'object'
      });
    }
  });

  socket.on('multiple args', (...args) => {
    console.log(`[v3] Multiple args test from ${clientId}, count: ${args.length}`);
    const ack = args[args.length - 1];
    if (typeof ack === 'function') {
      const actualArgs = args.slice(0, -1);
      ack({
        success: true,
        argCount: actualArgs.length,
        args: actualArgs
      });
    }
  });

  socket.on('null and undefined', (data, ack) => {
    console.log(`[v3] Null/undefined test from ${clientId}`);
    if (typeof ack === 'function') {
      ack({
        success: true,
        received: data,
        isNull: data === null
      });
    }
  });

  socket.on('nested object', (data, ack) => {
    console.log(`[v3] Nested object test from ${clientId}`);
    if (typeof ack === 'function') {
      ack({
        success: true,
        level1: data,
        deep: data && data.level1 && data.level1.level2 && data.level1.level2.value
      });
    }
  });

  socket.on('disconnect', (reason) => {
    connectedClients--;
    console.log(`[v3] Client disconnected: ${clientId}, reason: ${reason}, total: ${connectedClients}`);
  });
});

const chatNamespace = io.of('/chat');
chatNamespace.on('connection', (socket) => {
  console.log(`[v3] Client connected to /chat: ${socket.id}`);
  
  socket.emit('chat welcome', {
    namespace: '/chat',
    message: 'Welcome to chat namespace',
    timestamp: Date.now()
  });

  socket.on('chat message', (msg) => {
    console.log(`[v3] /chat message from ${socket.id}:`, msg);
    chatNamespace.emit('chat message', {
      from: socket.id,
      content: msg,
      timestamp: Date.now()
    });
  });

  socket.on('disconnect', () => {
    console.log(`[v3] Client disconnected from /chat: ${socket.id}`);
  });
});

const newsNamespace = io.of('/news');
newsNamespace.on('connection', (socket) => {
  console.log(`[v3] Client connected to /news: ${socket.id}`);
  
  socket.emit('news welcome', {
    namespace: '/news',
    message: 'Welcome to news namespace'
  });
});

server.listen(PORT, () => {
  console.log(`[v3] Socket.IO v3 test server listening on port ${PORT}`);
  console.log(`[v3] WebSocket + Polling transports enabled`);
  console.log(`[v3] Namespaces: / (default), /chat, /news`);
  console.log(`[v3] Events available:`);
  console.log(`[v3]   - ping/pong (basic event)`);
  console.log(`[v3]   - echo (with ACK)`);
  console.log(`[v3]   - chat message (broadcast)`);
  console.log(`[v3]   - binary test (binary data)`);
  console.log(`[v3]   - binary with json (mixed)`);
  console.log(`[v3]   - join room / leave room / room message`);
  console.log(`[v3]   - error test (triggers server error)`);
});
