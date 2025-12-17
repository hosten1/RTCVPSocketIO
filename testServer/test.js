// DOM Elements
const statusElement = document.getElementById('status');
const messageArea = document.getElementById('messageArea');
const messageInput = document.getElementById('messageInput');
const connectBtn = document.getElementById('connect');
const disconnectBtn = document.getElementById('disconnect');
const sendBtn = document.getElementById('send');
const sendCustomEventBtn = document.getElementById('sendCustomEvent');
const testAckBtn = document.getElementById('testAck');
const sendBinaryBtn = document.getElementById('sendBinary');
const testBinaryAckBtn = document.getElementById('testBinaryAck');
const serverUrlInput = document.getElementById('serverUrl');

// Socket instance
let socket;

// Add message to message area
function addMessage(text, type = 'system') {
    const messageDiv = document.createElement('div');
    messageDiv.className = `message ${type}`;
    messageDiv.textContent = `[${new Date().toLocaleTimeString()}] ${text}`;
    messageArea.appendChild(messageDiv);
    messageArea.scrollTop = messageArea.scrollHeight;
}

// Update connection status
function updateStatus(connected) {
    if (connected) {
        statusElement.className = 'status connected';
        statusElement.textContent = 'Connected';
        connectBtn.disabled = true;
        disconnectBtn.disabled = false;
        sendBtn.disabled = false;
        messageInput.disabled = false;
        sendCustomEventBtn.disabled = false;
        testAckBtn.disabled = false;
        sendBinaryBtn.disabled = false;
        testBinaryAckBtn.disabled = false;
        addMessage('Connected to server');
    } else {
        statusElement.className = 'status disconnected';
        statusElement.textContent = 'Disconnected';
        connectBtn.disabled = false;
        disconnectBtn.disabled = true;
        sendBtn.disabled = true;
        messageInput.disabled = true;
        sendCustomEventBtn.disabled = true;
        testAckBtn.disabled = true;
        sendBinaryBtn.disabled = true;
        testBinaryAckBtn.disabled = true;
        addMessage('Disconnected from server');
    }
}

// 生成测试用二进制数据
function generateTestBinaryData(size = 1024, pattern = 'sequential') {
    const binaryData = new Uint8Array(size);
    
    for (let i = 0; i < binaryData.length; i++) {
        if (pattern === 'sequential') {
            binaryData[i] = i % 256; // 0-255循环
        } else if (pattern === 'random') {
            binaryData[i] = Math.floor(Math.random() * 256); // 随机数据
        }
    }
    
    return binaryData;
}

// 比较二进制数据
function compareBinaryData(receivedData) {
    const expectedData = generateTestBinaryData(1024, 'sequential');
    
    if (receivedData.length !== expectedData.length) {
        addMessage(`❌ 长度不匹配: 预期 ${expectedData.length}, 实际 ${receivedData.length}`, 'system');
        return;
    }
    
    // 转换为数组比较
    const receivedArray = Array.from(receivedData);
    const expectedArray = Array.from(expectedData);
    
    let isEqual = true;
    let firstMismatch = -1;
    
    for (let i = 0; i < expectedArray.length; i++) {
        if (receivedArray[i] !== expectedArray[i]) {
            isEqual = false;
            firstMismatch = i;
            break;
        }
    }
    
    if (isEqual) {
        addMessage('✅ 二进制数据完全匹配！', 'system');
    } else {
        addMessage(`❌ 二进制数据不匹配！第一个不匹配的位置: ${firstMismatch}`, 'system');
        addMessage(`   预期值: ${expectedArray[firstMismatch]}, 实际值: ${receivedArray[firstMismatch]}`, 'system');
        
        // 打印前20个字节用于调试
        addMessage('前20个字节（预期）:', 'system');
        addMessage(Array.from(expectedData.slice(0, 20)).join(', '), 'system');
        
        addMessage('前20个字节（实际）:', 'system');
        addMessage(Array.from(receivedData.slice(0, 20)).join(', '), 'system');
    }
}

// 检查Socket.IO是否正确加载
function checkSocketIO() {
    if (typeof io === 'undefined') {
        addMessage('❌ Socket.IO library not loaded correctly!', 'system');
        connectBtn.disabled = true;
        return false;
    } else {
        addMessage('✅ Socket.IO library loaded successfully', 'system');
        return true;
    }
}

// 页面加载完成后检查Socket.IO
window.addEventListener('load', () => {
    checkSocketIO();
});

// Connect to server
connectBtn.addEventListener('click', () => {
    const serverUrl = serverUrlInput.value;
    
    addMessage(`Connecting to ${serverUrl}...`);
    
    try {
        if (typeof io === 'undefined') {
            throw new Error('Socket.IO library not loaded');
        }
        
        socket = io(serverUrl, {
            transports: ['polling'],
            timeout: 5000
        });
        
        // Connection event
        socket.on('connect', () => {
            updateStatus(true);
            addMessage(`Connected with socket ID: ${socket.id}`);
        });
        
        // Disconnect event
        socket.on('disconnect', (reason) => {
            updateStatus(false);
            addMessage(`Disconnected: ${reason}`);
        });
        
        // Welcome message
        socket.on('welcome', (data, callback) => {
            addMessage(`Welcome message: ${JSON.stringify(data)}`, 'received');
            
            // 发送ACK响应
            if (callback && typeof callback === 'function') {
                callback({ success: true, message: 'Welcome received from HTML client', clientId: socket.id });
            }
        });
        
        // User connected event
        socket.on('userConnected', (data, callback) => {
            addMessage(`User joined: ${data.socketId}`, 'system');
            
            // 发送ACK响应
            if (callback && typeof callback === 'function') {
                callback({ success: true, message: 'UserConnected received from HTML client', clientId: socket.id });
            }
        });
        
        // User disconnected event
        socket.on('userDisconnected', (data) => {
            addMessage(`User left: ${data.socketId}`, 'system');
        });
        
        // Chat message event
        socket.on('chatMessage', (data) => {
            const message = `${data.sender}: ${data.message}`;
            addMessage(message, 'received');
        });
        
        // Heartbeat event
        socket.on('heartbeat', (data) => {
            addMessage(`Heartbeat: ${data.timestamp}`, 'system');
        });
        
        // Binary event
        socket.on('binaryEvent', (data) => {
            // 延迟处理，确保Socket.IO完成数据处理
            setTimeout(() => {
                console.log('Full data object:', data);

                if (data.binaryData) {
                    // 检查是否是Uint8Array
                    if (data.binaryData instanceof Uint8Array) {
                        console.log('BinaryData is a Uint8Array');
                        compareBinaryData(data.binaryData);
                    }
                    // 检查是否是ArrayBuffer
                    else if (data.binaryData instanceof ArrayBuffer) {
                        console.log('BinaryData is an ArrayBuffer');
                        compareBinaryData(new Uint8Array(data.binaryData));
                    }
                    // 检查是否是类数组对象（可能是Buffer或其他二进制数据类型）
                    else if (typeof data.binaryData === 'object' && 'length' in data.binaryData && typeof data.binaryData[0] === 'number') {
                        console.log('BinaryData is an array-like object');
                        compareBinaryData(new Uint8Array(data.binaryData));
                    }
                    // 检查是否是普通对象（可能是占位符）
                    else if (typeof data.binaryData === 'object' && data.binaryData._placeholder) {
                        addMessage('Still waiting for binary data...', 'system');
                    }
                    // 其他类型
                    else {
                        addMessage(`Unknown binaryData type: ${typeof data.binaryData}`, 'system');
                        console.log('BinaryData:', data.binaryData);
                    }
                }
            }, 300);
        });
        
        // Error event
        socket.on('error', (error) => {
            addMessage(`Error: ${error.message}`, 'system');
        });
        
    } catch (error) {
        addMessage(`Connection error: ${error.message}`, 'system');
        addMessage(`Socket.IO status: ${typeof io}`, 'system');
    }
});

// Disconnect from server
disconnectBtn.addEventListener('click', () => {
    if (socket) {
        socket.disconnect();
    }
});

// Send message
sendBtn.addEventListener('click', () => {
    const message = messageInput.value.trim();
    if (message && socket) {
        addMessage(`Sent: ${message}`, 'sent');
        
        // Send with ACK
        socket.emit('chatMessage', { message: message }, (ack) => {
            addMessage(`ACK received: ${JSON.stringify(ack)}`, 'system');
        });
        
        messageInput.value = '';
    }
});

// Send message on Enter key
messageInput.addEventListener('keypress', (e) => {
    if (e.key === 'Enter') {
        sendBtn.click();
    }
});

// Send custom event
sendCustomEventBtn.addEventListener('click', () => {
    if (socket) {
        const customData = {
            timestamp: new Date().toISOString(),
            random: Math.random(),
            message: 'Custom event from HTML client'
        };
        
        addMessage(`Sending custom event: ${JSON.stringify(customData)}`, 'sent');
        
        // Send with ACK
        socket.emit('customEvent', customData, (ack) => {
            addMessage(`Custom event ACK: ${JSON.stringify(ack)}`, 'system');
        });
    }
});

// ACK test button event listener
testAckBtn.addEventListener('click', async () => {
    if (socket) {
        addMessage('🔄 Starting concurrent ACK test...', 'system');
        
        // Test parameters
        const testCount = 10; // Test 10 concurrent ACKs
        let completedCount = 0;
        let successCount = 0;
        let failureCount = 0;
        const startTime = Date.now();
        
        for (let i = 0; i < testCount; i++) {
            const testIndex = i;
            
            // Send with ACK
            socket.emit('customEvent', {
                testIndex: testIndex,
                message: `ACK Test ${testIndex}`,
                timestamp: Date.now()
            }, (ack) => {
                completedCount++;
                
                if (ack && ack.success) {
                    successCount++;
                    addMessage(`✅ ACK ${testIndex} success: ${JSON.stringify(ack)}`, 'system');
                } else {
                    failureCount++;
                    addMessage(`❌ ACK ${testIndex} failed: ${JSON.stringify(ack)}`, 'system');
                }
                
                // All tests completed
                if (completedCount === testCount) {
                    const endTime = Date.now();
                    const duration = (endTime - startTime) / 1000;
                    
                    addMessage(`📊 Concurrent ACK test completed: Total ${testCount}, Success ${successCount}, Failed ${failureCount}, Duration ${duration.toFixed(2)}s`, 'system');
                }
            });
            
            // Small delay to avoid too many requests at once
            await new Promise(resolve => setTimeout(resolve, 5));
        }
    }
});

// Send binary message button event listener
sendBinaryBtn.addEventListener('click', () => {
    if (socket) {
        // 创建模拟二进制数据
        const binaryData = generateTestBinaryData(1024, 'sequential'); // 1KB二进制数据，顺序填充
        
        const textMessage = 'testData: HTML客户端发送的二进制测试数据';
        
        addMessage(`📤 Sending binary data: Size ${binaryData.length} bytes, Text: ${textMessage}`, 'sent');
        
        // 发送二进制消息
        socket.emit('binaryEvent', {
            binaryData: binaryData,
            text: textMessage,
            timestamp: Date.now()
        }, (ack) => {
            if (ack && ack.success) {
                addMessage(`✅ Binary message ACK: ${JSON.stringify(ack)}`, 'system');
            } else {
                addMessage(`❌ Binary message failed: ${JSON.stringify(ack)}`, 'system');
            }
        });
    }
});

// Binary ACK test button event listener
testBinaryAckBtn.addEventListener('click', async () => {
    if (socket) {
        addMessage('🔄 Starting binary ACK test...', 'system');
        
        // 创建模拟二进制数据
        const binaryData = generateTestBinaryData(512, 'random'); // 512 bytes binary data，随机填充
        
        // 发送带ACK的二进制消息
        socket.emit('binaryAckTest', {
            binaryData: binaryData,
            text: 'Binary ACK test from HTML client',
            timestamp: Date.now()
        }, (ack) => {
            if (ack && ack.result === 'success') {
                addMessage(`✅ Binary ACK test success: ${JSON.stringify(ack)}`, 'system');
            } else {
                addMessage(`❌ Binary ACK test failed: ${JSON.stringify(ack)}`, 'system');
            }
        });
    }
});