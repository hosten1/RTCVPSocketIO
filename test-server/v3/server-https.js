const https = require('https');
const fs = require('fs');
const { Server } = require('socket.io');

const options = {
  key: fs.readFileSync('key.pem'),
  cert: fs.readFileSync('cert.pem'),
  rejectUnauthorized: false
};

const app = (req, res) => {
  res.writeHead(200);
  res.end('Socket.IO v3 HTTPS Test Server Running');
};

const server = https.createServer(options, app);
const io = new Server(server, {
  transports: ['websocket', 'polling'],
  pingInterval: 25000,
  pingTimeout: 20000,
  cors: {
    origin: "*",
    methods: ["GET", "POST"]
  }
});

const PORT = 3004;

let connectedClients = 0;

io.on('connection', (socket) => {
  const clientId = socket.id;
  connectedClients++;
  console.log(`[v3-https] Client connected: ${clientId}, total: ${connectedClients}`);

  socket.emit('welcome', {
    message: 'Welcome to Socket.IO v3 HTTPS test server',
    clientId: clientId,
    timestamp: Date.now()
  });

  socket.on('ping', (data) => {
    console.log(`[v3-https] Received ping from ${clientId}:`, data);
    socket.emit('pong', {
      timestamp: Date.now(),
      received: data
    });
  });

  socket.on('echo', (data, ack) => {
    console.log(`[v3-https] Received echo from ${clientId}:`, typeof data);
    if (typeof ack === 'function') {
      ack({
        echoed: data,
        timestamp: Date.now(),
        server: 'v3-https'
      });
    }
  });

  socket.on('disconnect', (reason) => {
    connectedClients--;
    console.log(`[v3-https] Client disconnected: ${clientId}, reason: ${reason}, total: ${connectedClients}`);
  });
});

server.listen(PORT, () => {
  console.log(`[v3-https] Socket.IO v3 HTTPS test server listening on port ${PORT}`);
  console.log(`[v3-https] WebSocket + Polling transports enabled`);
});
