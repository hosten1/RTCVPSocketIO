#!/bin/bash

# 设置脚本遇到错误时退出
set -e

# 调试信息：显示JAVA_HOME环境变量
echo "JAVA_HOME: $JAVA_HOME"

# 获取工作空间目录
WORKSPACE=$(pwd)
echo "工作空间: $WORKSPACE"

# 版本信息
VERSION="Release_1.0.10a."
echo "版本: $VERSION"

# 获取当前日期时间
CURRENT_DATE_TIME=$(date "+%Y%m%d%H%M")
echo "当前时间: $CURRENT_DATE_TIME"

# 获取最新的git提交ID
cd $WORKSPACE || exit 1
COMMITID="$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")"
echo "Git提交ID: $COMMITID"

# 检查并释放3000端口
PORT=3000
echo "=== 检查端口 $PORT 占用情况 ==="

# 函数：检查端口是否被占用
check_port() {
    local port=$1
    if command -v lsof > /dev/null 2>&1; then
        # macOS/Linux使用lsof
        if lsof -ti :$port > /dev/null 2>&1; then
            return 0  # 端口被占用
        fi
    elif command -v netstat > /dev/null 2>&1; then
        # 备用方法：使用netstat
        if netstat -an | grep -E ":$port.*LISTEN" > /dev/null 2>&1; then
            return 0  # 端口被占用
        fi
    elif [ -f /proc/net/tcp ]; then
        # Linux下检查/proc/net/tcp
        local hex_port=$(printf "%04X" $port)
        if grep -q ":$hex_port" /proc/net/tcp 2>/dev/null; then
            return 0  # 端口被占用
        fi
    else
        echo "警告: 无法检测端口占用状态，请确保端口 $port 可用"
        return 1  # 无法检测
    fi
    return 1  # 端口未被占用
}

# 函数：释放指定端口
release_port() {
    local port=$1
    local max_attempts=3
    local attempt=1
    
    while [ $attempt -le $max_attempts ]; do
        echo "尝试 $attempt/$max_attempts: 释放端口 $port..."
        
        # 方法1: 使用lsof获取并终止进程
        if command -v lsof > /dev/null 2>&1; then
            local pids=$(lsof -ti :$port 2>/dev/null)
            if [ -n "$pids" ]; then
                echo "找到占用进程: $pids"
                for pid in $pids; do
                    echo "终止进程 $pid..."
                    # 先尝试优雅终止
                    kill $pid 2>/dev/null || true
                    sleep 1
                    # 检查进程是否仍在运行
                    if kill -0 $pid 2>/dev/null; then
                        echo "进程 $pid 仍在运行，强制终止..."
                        kill -9 $pid 2>/dev/null || true
                    fi
                done
            fi
        fi
        
        # 方法2: 使用fuser（Linux）
        if command -v fuser > /dev/null 2>&1; then
            fuser -k $port/tcp 2>/dev/null || true
        fi
        
        # 等待一段时间让端口释放
        sleep 2
        
        # 检查端口是否已释放
        if ! check_port $port; then
            echo "✓ 端口 $port 已成功释放"
            return 0
        fi
        
        echo "端口 $port 仍被占用，等待后重试..."
        sleep 3
        attempt=$((attempt + 1))
    done
    
    echo "错误: 无法释放端口 $port，可能被系统进程占用"
    echo "请手动检查并释放端口:"
    echo "  sudo lsof -i :$port"
    echo "  sudo kill -9 [PID]"
    return 1
}

# 主流程：检查并释放端口
if check_port $PORT; then
    echo "端口 $PORT 被占用，尝试释放..."
    if release_port $PORT; then
        echo "端口释放成功"
    else
        # 如果无法释放端口，询问用户是否继续
        read -p "无法释放端口 $PORT，是否继续启动服务器？(y/n): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            echo "用户取消启动"
            exit 1
        fi
        echo "继续启动服务器..."
    fi
else
    echo "✓ 端口 $PORT 可用"
fi

# 等待确保端口完全释放
sleep 1

# 设置环境变量并启动服务器
echo "=== 启动Node.js服务器 ==="

# 检查server.js文件是否存在
if [ ! -f "server.js" ]; then
    echo "错误: server.js 文件不存在"
    echo "当前目录: $(pwd)"
    ls -la
    exit 1
fi

# 检查node命令是否可用
if ! command -v node > /dev/null 2>&1; then
    echo "错误: Node.js未安装或不在PATH中"
    exit 1
fi

# 创建日志目录
LOG_DIR="logs"
mkdir -p $LOG_DIR
LOG_FILE="$LOG_DIR/server_$(date +%Y%m%d_%H%M%S).log"
echo "日志文件: $LOG_FILE"

# 启动信息
echo "启动命令: DEBUG=engine:*,socket.io* node server.js"
echo "启动时间: $(date)"

# 设置服务器进程标题
SERVER_TITLE="node-server-$PORT"

# 启动服务器（后台运行）
{
    echo "=== 服务器启动日志 ==="
    echo "工作目录: $(pwd)"
    echo "启动命令: DEBUG=engine:*,socket.io* node server.js"
    echo "进程ID: $$"
    echo "启动时间: $(date)"
    echo ""
    
    # 设置环境变量并启动
    export DEBUG="engine:*,socket.io*"
    exec node server.js
} > "$LOG_FILE" 2>&1 &

# 获取服务器进程ID
SERVER_PID=$!
echo "服务器进程ID: $SERVER_PID"

# 设置进程标题（如果支持）
if command -v prctl > /dev/null 2>&1; then
    prctl --name="$SERVER_TITLE" --pid=$SERVER_PID 2>/dev/null || true
fi

# 检查服务器是否启动成功
echo "等待服务器启动..."
sleep 3

# 检查进程是否仍在运行
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "错误: 服务器进程已终止"
    echo "查看日志文件: $LOG_FILE"
    tail -20 "$LOG_FILE"
    exit 1
fi

# 检查端口是否已被服务器监听
if check_port $PORT; then
    echo "✓ 服务器已在端口 $PORT 上监听"
    echo "服务器启动成功!"
    
    # 显示服务器信息
    echo "=== 服务器信息 ==="
    echo "进程ID: $SERVER_PID"
    echo "端口: $PORT"
    echo "日志文件: $LOG_FILE"
    echo "版本: $VERSION$CURRENT_DATE_TIME$COMMITID"
    
    # 提供监控命令
    echo ""
    echo "监控命令:"
    echo "  查看日志: tail -f $LOG_FILE"
    echo "  查看进程: ps -p $SERVER_PID"
    echo "  停止服务器: kill $SERVER_PID"
    
    # 保存进程ID到文件（可选）
    echo $SERVER_PID > ".server.pid"
    echo "进程ID已保存到 .server.pid"
    
    # 显示服务器URL
    echo "访问地址: http://localhost:$PORT"
else
    echo "警告: 服务器进程在运行，但端口 $PORT 未监听"
    echo "查看日志文件: $LOG_FILE"
    tail -20 "$LOG_FILE"
    
    # 终止服务器进程
    kill $SERVER_PID 2>/dev/null || true
    exit 1
fi

# 等待服务器进程结束（如果在前台运行）
if [ "$1" = "--foreground" ]; then
    echo "前台运行模式，按Ctrl+C停止服务器"
    wait $SERVER_PID
    echo "服务器已停止"
    exit 0
else
    echo "服务器已在后台运行"
    echo "使用以下命令查看日志: tail -f $LOG_FILE"
    exit 0
fi