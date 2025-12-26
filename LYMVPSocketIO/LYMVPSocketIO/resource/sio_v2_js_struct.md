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

 51-0["binaryEvent",{"text":"testData: HTML客户端发送的二进制测试数据","timestamp":1766718059043,"namespace":"/"},{"_placeholder":true,"num":0}] {
  type: 5,
  attachments: 1,
  nsp: '/',
  id: 0,
  data: [
    'binaryEvent',
    {
      text: 'testData: HTML客户端发送的二进制测试数据',
      timestamp: 1766718059043,
      namespace: '/'
    },
    { _placeholder: true, num: 0 }
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

 51-/chat,0["binaryEvent",{"text":"testData: HTML客户端发送的二进制测试数据","timestamp":1766718219141,"namespace":"/"},{"_placeholder":true,"num":0}] {
  type: 5,
  attachments: 1,
  nsp: '/chat',
  id: 0,
  data: [
    'binaryEvent',
    {
      text: 'testData: HTML客户端发送的二进制测试数据',
      timestamp: 1766718219141,
      namespace: '/'
    },
    { _placeholder: true, num: 0 }
  ]
}
```

# v2 二进制不允许在文本中
也就是这种：
```js
const data = {
            "text":textMessage,
            "timestamp":Date.now(),
            "namespace":"/",
            "binaryData":binaryData
             }
```
必须是这种：
```js
const meta = {
    text: textMessage,
    timestamp: Date.now(),
    namespace: "/"
};

// 将 Uint8Array 转成 Blob
const binaryBlob = new Blob([binaryData]);

socket.emit('binaryEvent', meta, binaryBlob, (ack) => {
    if (ack && ack.success) {
        addMessage(`✅ Binary message ACK: ${JSON.stringify(ack)}`, 'system');
    } else {
        addMessage(`❌ Binary message failed: ${JSON.stringify(ack)}`, 'system');
    }
});
```