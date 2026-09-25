#!/bin/bash
set -e

cd libs
. "$(dirname "$0")/build_deps_fetch.sh"

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
cmake_major=$($cmake --version 2>/dev/null | head -1 | tr -dc '0-9.' | cut -d. -f1)
if [ -n "$cmake_major" ] && [ "$cmake_major" -ge 4 ] 2>/dev/null; then
  CMAKE_COMPAT_ARGS="-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
  echo "CMake $cmake_major detected; passing $CMAKE_COMPAT_ARGS to legacy dependencies."
fi

# Pinned by sha256. These same three archives are also verified by
# packaging/flatpak/io.github.Ogstra.Proxor.yml, and
# libs/tests/test-dependency-pins.sh fails the build if the two ever drift.
ZXING_URL="https://github.com/nu-book/zxing-cpp/archive/refs/tags/v2.0.0.tar.gz"
ZXING_SHA256="12b76b7005c30d34265fc20356d340da179b0b4d43d2c1b35bcca86776069f76"
YAMLCPP_URL="https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.7.0.tar.gz"
YAMLCPP_SHA256="43e6a9fcb146ad871515f0d0873947e5d497a1c9c60c58cb102a97b47208b7c3"
PROTOBUF_URL="https://github.com/protocolbuffers/protobuf/releases/download/v21.4/protobuf-all-21.4.tar.gz"
PROTOBUF_SHA256="6c5e1b0788afba4569aeebb2cfe205cb154aa01deacaba0cd26442f3b761a836"

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
  # dl.zip and the bare protobuf/ dir are pre-this-change leftovers that a
  # libs/deps tree from before this plan may still hold; keep removing them
  # so clean actually cleans an old tree, not just a fresh one.
  rm -rf dl-*.tar.gz dl.zip zxing-* yaml-* protobuf protobuf-21.4
}

#### ZXing v2.0.0 ####
fetch_verified "$ZXING_URL" "$ZXING_SHA256" dl-zxing.tar.gz
tar xzf dl-zxing.tar.gz

cd zxing-cpp-2.0.0
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
fetch_verified "$YAMLCPP_URL" "$YAMLCPP_SHA256" dl-yaml.tar.gz
tar xzf dl-yaml.tar.gz

cd yaml-cpp-yaml-cpp-0.7.0
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
# protobuf-all is the -all variant precisely because it carries the vendored
# third_party/ (including googletest) that used to come from the old
# submodule-recursive checkout, so nothing else has to be fetched separately.
fetch_verified "$PROTOBUF_URL" "$PROTOBUF_SHA256" dl-protobuf.tar.gz
tar xzf dl-protobuf.tar.gz

#备注：交叉编译要在 host 也安装 protobuf 并且版本一致,编译安装，同参数，安装到 /usr/local

mkdir -p protobuf-21.4/build
cd protobuf-21.4/build

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
