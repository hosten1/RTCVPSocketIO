# README

This folder contains a modified/adapted subset of the libwebrtc library, which is used by mediasoup for transport congestion purposes.

* libwebrtc branch: m77
* libwebrtc commit: 2bac7da1349c75e5cf89612ab9619a1920d5d974

The file `libwebrtc/mediasoup_helpers.h` includes some utilities to plug mediasoup classes into libwebrtc.


# 240104 libWebRTC中新增`USE_MEDIASOUP_ClASS`宏开关，用于控制是否使用mediasoup的一些类
# build
 工程目录下执行：sudo npm install

# 其中RtpTransportControllerSend构造函数的第一个参数 Clock::GetRealTimeClock()
 引入头文件`system_wrappers/include/clock.h`后,  调用：`Clock::GetRealTimeClock()`;这个放回定义为：
 ```cpp
  static Clock* GetRealTimeClock();
 ```
 其实现方法根据不同平台创建不同的Clock；
 # macos 平台