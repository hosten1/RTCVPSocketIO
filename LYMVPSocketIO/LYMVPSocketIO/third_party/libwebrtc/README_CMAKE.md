# libwebrtc CMake 构建说明

## 概述

本目录包含了将 `libwebrtc.gyp` 转换为 CMake 配置的文件，支持每个文件夹独立构建。

## 目录结构

```
libwebrtc/
├── CMakeLists.txt          # 主 CMake 配置文件
├── api/                    # API 目录
│   └── CMakeLists.txt      # API 目录的 CMake 配置
├── common_audio/           # 公共音频目录
│   └── CMakeLists.txt      # 公共音频目录的 CMake 配置
├── rtc_base/               # RTC 基础目录
│   └── CMakeLists.txt      # RTC 基础目录的 CMake 配置
├── system_wrappers/        # 系统包装器目录
│   └── CMakeLists.txt      # 系统包装器目录的 CMake 配置
└── README_CMAKE.md         # 本说明文件
```

## 使用方法

1. 确保已安装 CMake（建议版本 3.10 或更高）

2. 创建构建目录并运行 CMake：

```bash
mkdir -p build
cd build
cmake ..
```

3. 构建静态库：

```bash
make
```

## 构建失败说明

如果构建失败，提示缺少 `absl/types/optional.h` 或其他 abseil-cpp 相关的头文件，这是因为：

1. libwebrtc 代码中大量使用了 abseil-cpp 库
2. 当前目录结构中没有包含 abseil-cpp 库
3. 原 gyp 文件中提到的依赖项（abseil-cpp、jsoncpp、libuv、openssl）在当前目录中不存在

## 解决方案

要成功构建 libwebrtc，您需要：

1. 下载并安装 abseil-cpp 库
2. 将 abseil-cpp 库的包含路径添加到 CMakeLists.txt 中
3. 类似地添加其他依赖项（jsoncpp、libuv、openssl）

或者，您可以根据项目实际需求，修改 libwebrtc 代码，移除对这些依赖项的使用。

## CMake 配置说明

- 主 `CMakeLists.txt` 定义了 libwebrtc 静态库，并包含了所有子目录
- 每个子目录都有自己的 `CMakeLists.txt`，负责将该目录下的源文件添加到主库中
- 支持根据操作系统设置不同的宏定义
- 支持根据字节序设置不同的宏定义

## 注意事项

- 本 CMake 配置是基于原 `libwebrtc.gyp` 文件转换而来
- 由于缺少依赖项，直接构建可能会失败
- 您需要根据项目实际情况调整包含路径和依赖项

## 联系方式

如有任何问题，请随时联系项目维护者。
