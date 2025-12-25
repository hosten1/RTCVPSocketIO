
2025-12-24T05:53:03.650Z    engine:socket flushing buffer to transport +1ms
=== 新Socket.IO连接 ===
Socket ID: oB3VnxaBDzk5iW-tAAAN
命名空间: /
连接时间: 2025-12-24T05:53:03.650Z
传输方式: websocket
Engine.IO版本: 4
=== Socket.IO连接信息结束 ===

# Socket.IO服务端接收数据 namespace: /
```
 311[{"success":true,"message":"Welcome received from HTML client","clientId":"oB3VnxaBDzk5iW-tAAAN","namespace":"/"}] {
  type: 3,
  nsp: '/',
  id: 11,
  data: [
    {
      success: true,
      message: 'Welcome received from HTML client',
      clientId: 'oB3VnxaBDzk5iW-tAAAN',
      namespace: '/'
    }
  ]
}

20["customEvent",{"testIndex":0,"message":"ACK Test 0","timestamp":1766555697133}] {
  type: 2,
  nsp: '/',
  id: 0,
  data: [
    'customEvent',
    { testIndex: 0, message: 'ACK Test 0', timestamp: 1766555697133 }
  ]
}

51-10["binaryEvent",{"binaryData":{"_placeholder":true,"num":0},"text":"testData: HTML客户端发送的二进制测试数据","timestamp":1766555717318}] {
  type: 5,
  attachments: 1,
  nsp: '/',
  id: 10,
  data: [
    'binaryEvent',
    {
      binaryData: [Object],
      text: 'testData: HTML客户端发送的二进制测试数据',
      timestamp: 1766555717318
    }
  ]
}
```


=== 新Socket.IO连接 ===
Socket ID: do6ZadaZ41sCLeQtAAAP
命名空间: /chat
连接时间: 2025-12-24T05:56:02.754Z
传输方式: websocket
Engine.IO版本: 4
=== Socket.IO连接信息结束 ===

```
 3/chat,1[{"success":true,"message":"Welcome received from HTML client","clientId":"do6ZadaZ41sCLeQtAAAP","namespace":"/chat"}] {
  type: 3,
  nsp: '/chat',
  id: 1,
  data: [
    {
      success: true,
      message: 'Welcome received from HTML client',
      clientId: 'do6ZadaZ41sCLeQtAAAP',
      namespace: '/chat'
    }
  ]
}

27["customEvent",{"testIndex":7,"message":"ACK Test 7","timestamp":1766545450167}] {
  type: 2,
  nsp: '/',
  id: 7,
  data: [
    'customEvent',
    { testIndex: 7, message: 'ACK Test 7', timestamp: 1766545450167 }
  ]
}

 51-/chat,0["binaryEvent",{"binaryData":{"_placeholder":true,"num":0},"text":"testData: HTML客户端发送的二进制测试数据","timestamp":1766555826027}] {
  type: 5,
  attachments: 1,
  nsp: '/chat',
  id: 0,
  data: [
    'binaryEvent',
    {
      binaryData: [Object],
      text: 'testData: HTML客户端发送的二进制测试数据',
      timestamp: 1766555826027
    }
  ]
}
```