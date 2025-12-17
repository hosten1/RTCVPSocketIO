#!/bin/bash

# 构建通用框架脚本

set -e

echo "开始构建通用框架..."

# 项目名称
PROJECT_NAME="VPSocketIO"
# 项目路径
PROJECT_PATH="$PROJECT_NAME.xcodeproj"
# 输出目录
OUTPUT_DIR="build/Framework"
# 临时目录
TEMP_DIR="build/Temp"

# 清理旧构建
rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"
rm -rf "$TEMP_DIR"
mkdir -p "$TEMP_DIR"

# 构建设备版本
echo "构建设备版本..."
xcodebuild -project "$PROJECT_PATH" -scheme "$PROJECT_NAME" -configuration Release -sdk iphoneos ARCHS="arm64" ONLY_ACTIVE_ARCH=NO BUILD_DIR="$TEMP_DIR" clean build

# 构建模拟器版本
echo "构建模拟器版本..."
xcodebuild -project "$PROJECT_PATH" -scheme "$PROJECT_NAME" -configuration Release -sdk iphonesimulator ARCHS="x86_64 arm64" ONLY_ACTIVE_ARCH=NO BUILD_DIR="$TEMP_DIR" clean build

# 合并framework
echo "合并framework..."

# 设备framework路径
device_framework_path="$TEMP_DIR/Release-iphoneos/$PROJECT_NAME.framework"
# 模拟器framework路径
simulator_framework_path="$TEMP_DIR/Release-iphonesimulator/$PROJECT_NAME.framework"
# 输出framework路径
output_framework_path="$OUTPUT_DIR/$PROJECT_NAME.framework"

# 复制设备framework到输出目录
cp -R "$device_framework_path" "$output_framework_path"

# 合并二进制文件
lipo -create "$device_framework_path/$PROJECT_NAME" "$simulator_framework_path/$PROJECT_NAME" -output "$output_framework_path/$PROJECT_NAME"

# 验证合并结果
echo "验证合并结果..."
lipo -info "$output_framework_path/$PROJECT_NAME"

# 复制模拟器framework中的Modules目录（包含Swift模块信息）
if [ -d "$simulator_framework_path/Modules" ]; then
    cp -R "$simulator_framework_path/Modules" "$output_framework_path/"
fi

# 清理临时文件
rm -rf "$TEMP_DIR"

echo "通用框架构建完成！"
echo "输出路径: $output_framework_path"
