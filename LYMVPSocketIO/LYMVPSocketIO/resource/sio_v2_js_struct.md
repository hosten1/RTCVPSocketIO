=== 新Socket.IO连接 ===
Socket ID: 3dK_3SaPfBC2Dlg-AAAK
命名空间: /
连接时间: 2025-12-24T05:49:27.940Z
传输方式: websocket
Engine.IO版本: 3
=== Socket.IO连接信息结束 ===

# Socket.IO事件 默认命名空间å
```
 31[{"success":true,"message":"Welcome received from HTML client","clientId":"NujsYruFO2dDrZEDAAAB","namespace":"/"}] {
  type: 3,
  nsp: '/',
  id: 1,
  data: [
    {
      success: true,
      message: 'Welcome received from HTML client',
      clientId: 'NujsYruFO2dDrZEDAAAB',
      namespace: '/'
    }
  ]
}

20["customEvent",{"testIndex":0,"message":"ACK Test 0","timestamp":1766546691824}] {
  type: 2,
  nsp: '/',
  id: 0,
  data: [
    'customEvent',
    { testIndex: 0, message: 'ACK Test 0', timestamp: 1766546691824 }
  ]
}

20["binaryEvent","testData: HTML客户端发送的二进制测试数据",1766553800174,{'0': 0,...}] {
  type: 2,
  nsp: '/',
  id: 0,
  data: [
    'binaryEvent',
    'testData: HTML客户端发送的二进制测试数据',
    1766553800174,
    {
      '0': 0,
     ...
    }
  ]
}
```

# Socket.IO事件 命名空间/chat

```
  3/chat,0[{"success":true,"message":"Welcome received from HTML client","clientId":"/chat#u_ADFxmrI_nPI2c2AAAE","namespace":"/chat"}] {
  type: 3,
  nsp: '/chat',
  id: 0,
  data: [
    {
      success: true,
      message: 'Welcome received from HTML client',
      clientId: '/chat#u_ADFxmrI_nPI2c2AAAE',
      namespace: '/chat'
    }
  ]
}

2/chat,1["customEvent",{"testIndex":1,"message":"ACK Test 1","timestamp":1766553396268}] {
  type: 2,
  nsp: '/chat',
  id: 1,
  data: [
    'customEvent',
    { testIndex: 1, message: 'ACK Test 1', timestamp: 1766553396268 }
  ]
}

 2/chat,10["binaryEvent","testData: HTML客户端发送的二进制测试数据",1766553445364,{"0":0,...}] {
  type: 2,
  nsp: '/chat',
  id: 10,
  data: [
    'binaryEvent',
    'testData: HTML客户端发送的二进制测试数据',
    1766553445364,
    {
      '0': 0,
      ...
    }
  ]
}
```