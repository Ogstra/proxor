#!/bin/bash
set -e

cd libs

# 参数
if [ -z $cmake ]; then
  cmake="cmake"
fi
if [ -z $deps ]; then
  deps="deps"
fi

# CMake 4 removed support for cmake_minimum_required(VERSION < 3.5). yaml-cpp 0.7.0 and
# protobuf 21.4 both predate that floor, so they fail to configure on runners shipping
# CMake 4.x while still configuring fine on the CMake 3.x used locally. Only pass the
# escape hatch when it is actually needed, so older CMake does not see an unused variable.
CMAKE_COMPAT_ARGS=""
cmake_major=$($cmake --version 2>/dev/null | head -1 | sed -E 's/[^0-9]*([0-9]+).*//')
if [ -n "$cmake_major" ] && [ "$cmake_major" -ge 4 ] 2>/dev/null; then
  CMAKE_COMPAT_ARGS="-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
  echo "CMake $cmake_major detected; passing $CMAKE_COMPAT_ARGS to legacy dependencies."
fi

# libs/deps/...
mkdir -p $deps
cd $deps
if [ -z $NKR_PACKAGE ]; then
  INSTALL_PREFIX=$PWD/built
else
  INSTALL_PREFIX=$PWD/package
fi
rm -rf $INSTALL_PREFIX
mkdir -p $INSTALL_PREFIX

#### clean ####
clean() {
  rm -rf dl.zip yaml-* zxing-* protobuf
}

#### ZXing v2.0.0 ####
curl -L -o dl.zip https://github.com/nu-book/zxing-cpp/archive/refs/tags/v2.0.0.zip
unzip dl.zip

cd zxing-*
mkdir -p build
cd build

if [ "${OS:-}" = "Windows_NT" ] || [ -n "${VCINSTALLDIR:-}" ]; then
  ZXING_EXTRA_CMAKE_ARGS="-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL"
else
  ZXING_EXTRA_CMAKE_ARGS=""
fi
$cmake .. -GNinja -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=OFF -DBUILD_BLACKBOX_TESTS=OFF -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX $ZXING_EXTRA_CMAKE_ARGS $CMAKE_COMPAT_ARGS
ninja && ninja install

cd ../..

#### yaml-cpp ####
curl -L -o dl.zip https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.7.0.zip
unzip dl.zip

cd yaml-*
mkdir -p build
cd build

if [ "${OS:-}" = "Windows_NT" ] || [ -n "${VCINSTALLDIR:-}" ]; then
  YAML_EXTRA_CMAKE_ARGS="-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL"
else
  YAML_EXTRA_CMAKE_ARGS=""
fi
$cmake .. -GNinja -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX $YAML_EXTRA_CMAKE_ARGS $CMAKE_COMPAT_ARGS
ninja && ninja install

cd ../..

#### protobuf ####
git clone --recurse-submodules -b v21.4 --depth 1 --shallow-submodules https://github.com/protocolbuffers/protobuf

#备注：交叉编译要在 host 也安装 protobuf 并且版本一致,编译安装，同参数，安装到 /usr/local

mkdir -p protobuf/build
cd protobuf/build

$cmake .. -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL \
  -Dprotobuf_MSVC_STATIC_RUNTIME=OFF \
  -Dprotobuf_BUILD_TESTS=OFF \
  -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX \
  $CMAKE_COMPAT_ARGS
ninja && ninja install

cd ../..

####
clean
