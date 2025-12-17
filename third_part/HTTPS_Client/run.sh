#!/bin/bash
function build_ios(){
  make clean
  export CC=clang
  export CROSS_TOP=$XCODE_PATH/Developer/Platforms/iPhoneOS.platform/Developer
  export CROSS_SDK=iPhoneOS.sdk
  export PATH="$XCODE_PATH/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin:$PATH"
  # 设置最小依赖版本
  export IPHONEOS_DEPLOYMENT_TARGET=8.0
  echo  'current path: '`pwd`
  echo 'cmake====>'
  cmake -DCMAKE_BUILD_TYPE=Release .. -GXcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    "-DCMAKE_OSX_ARCHITECTURES=armv7;arm64" \
    -DCMAKE_OSX_SYSROOT=iphoneos\
    -DCMAKE_OSX_DEPLOYMENT_TARGET=9.0 \
    -DCMAKE_INSTALL_PREFIX=`pwd`/_install \
    -DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO \
    -DCMAKE_IOS_INSTALL_COMBINED=YES \
    -DENABLE_BITCODE=NO

   echo 'build====>'
   cmake --build . --config Release
   echo  'current path: '`pwd`
  #  cp mbedtls/library/Release-iphoneos/*  Release-iphoneos/
  #  mv `pwd`/ios/Release/librtcProbe.a `pwd`/ios/librtcProbe.a
  #  rm -r `pwd`/ios/Release/
}
function build_mac(){
   echo  'current path: '`pwd`
   echo 'cmake====>'
   cmake -DCMAKE_BUILD_TYPE=Release ..
   echo 'build====>'
   cmake --build .
  #  cp mbedtls/library/mac/*  mac/

}
function clear_build(){
      echo 'current path: '`pwd`
    echo ' 进入build文件夹'
    cd build/
    echo  'current path: '`pwd`
    echo "当前目录下文件或文件夹："`ls`
    if [ -f CMakeCache.txt ]; then
        echo "开始清除 上次编译文件" 
        rm -r  CMake*
        rm -r  cmake*
        rm -r  https*
        rm -r  Make*
        rm -r  mbed*
        rm -r  Release*
        rm -r  mac    
    fi
}
function ar_file(){
  sysName=`uname  -a`
  dir_path=`pwd`/Release-iphoneos
  if [ $1 = '-mac' ]
  then
    dir_path=`pwd`/mac
    echo  'dir_path: '$dir_path
  fi
  echo  '参数和系统名: '$1' '$sysName
  file_name="libhttps_client_all.a" # 合成.a的名字；根据实际情况修改
  # Mac 系统下编译的iOS库和macOS库都可以使用libtool编译
  if [ "${sysName}"='Darwin' ];
  then
    cd ${dir_path}
    xcrun -r libtool -no_warning_for_no_symbols  -static -o ${file_name} *.a
    return
  fi
  
  lib_path="${dir_path}/lib_folder" # 解压.a和合成.a绝对路径；根据实际情况修改
  #创建库文件目录
  if [[ ! -d "${lib_path}" ]]
  then
    mkdir -p "${lib_path}" # 使用双引号防止存在空格导致错误
  fi

  cd "${lib_path}"

  # 查找文件并解压
  for file in $(ls "${dir_path}"|tr " " "?") # 解决名字带空格的问题
  do
  if [[ "${file}" =~ ".a" ]]
  then
  ar x "${dir_path}/${file}" #解压文件所在的路径 如果是在上级目录，可以用../${file}
  fi
  done

  # 合并文件
  ar cru "${file_name}" *.o
  ranlib "${file_name}"

  # 删除解压出来的文件
  for file in $(ls|tr " " "?")
  do
  if [ "${file}" != "${file_name}" ] # 不是我们最终的.a文件，就删掉
  then
  rm -f "${file}"
  fi
  done

  # 打开文件夹
  open "${dir_path}"
}
# 当前执行命令的根目录
SHELL_PATH=`pwd`
if ! [ -x "$(command -v cmake)" ]; then
  echo 'Error: cmake is not installed.' >&2
  echo '请安装 cmake'
  #   if ! [ -x "$(command -v brew)" ]; then
  #     echo 'Error: brew is not installed.' >&2
  #     echo '开始安装 brew 如果长时间无响应，需要访问外网'
  #     /bin/zsh -c "$(curl -fsSL https://gitee.com/cunkai/HomebrewCN/raw/master/Homebrew.sh)"
  #   fi
  # echo '开始安装 cmake 如果长时间无响应，需要访问外网'
  # export HOMEBREW_NO_AUTO_UPDATE=true
  # brew install cmake
  exit -1
fi

if [ $1 = '-mac' ]
then
  if ! [ -x "$(command -v xcrun)" ]; then
    echo 'Error: xcrun is not installed.' >&2
    echo '请安装 xcrun'
    exit -1
  fi
  clear_build
  echo " build_mac=====>"
  build_mac
  # ar_file $1
   
  echo " 运行测试程序=====>"
  cd mac/
  ./main
    #  测试完成删除可执行程序
  rm -r main
  cd "$SHELL_PATH"
  echo  'current path: '`pwd`' 编译完成！！！'
elif [ $1 =  '-ios' ]
then
  if ! [ -x "$(command -v xcrun)" ]; then
    echo 'Error: xcrun is not installed.' >&2
    echo '请安装 xcrun'
    exit -1
  fi
  clear_build
  echo " build_ios=====>"
  build_ios
  # ar_file $1
  cd "$SHELL_PATH"
  echo  'current path: '`pwd`' 编译完成！！！'
else
  echo "Pleasemake sure the positon variable is ios  or android."
fi