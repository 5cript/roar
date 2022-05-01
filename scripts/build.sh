#!/bin/bash

IS_MSYS2_CLANG=off
COMPILER=clang++
CCOMPILER=clang
LINKER=lld
THREADS=8
BUILD_TYPE=Debug
BUILD_DOCS=on

DIR="${BASH_SOURCE%/*}"
if [[ ! -d "$DIR" ]]; then DIR="$PWD"; fi
. "$DIR/build_overrides.sh"

while getopts b:j: opts; do
   case ${opts} in
      b) BUILD_TYPE=${OPTARG} ;;
      j) THREADS=${OPTARG} ;;
   esac
done

# Go to build dir
CURDIR=$(basename $PWD)
if [[ $CURDIR == "scripts" ]]; then
  cd ..
fi
BUILD_DIR="build/${CCOMPILER}_${BUILD_TYPE,,}"
mkdir -p build
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

export CXX=$COMPILER
export CC=$CCOMPILER

CMAKE_GENERATOR="Unix Makefiles"
if [[ ! -z "${MSYSTEM}" ]]; then
  CMAKE_GENERATOR="MSYS Makefiles"
  if [[ $CCOMPILER == clang ]]; then
    IS_MSYS2_CLANG=on
  fi
fi

CMAKE_CXX_FLAGS=""
if [[ ! -z "${MSYSTEM}" ]]; then
  if [[ $CCOMPILER == clang ]]; then
    CMAKE_CXX_FLAGS="-fuse-ld=lld"
  fi
fi

cmake \
  -G"${CMAKE_GENERATOR}" \
  -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
  -DCMAKE_CXX_FLAGS=$CMAKE_CXX_FLAGS \
  -DCMAKE_CXX_COMPILER=$COMPILER \
  -DCMAKE_C_COMPILER=$CCOMPILER \
  -DCMAKE_LINKER=$LINKER \
  -DCMAKE_CXX_STANDARD=20 \
  -DROAR_BUILD_EXAMPLES=on \
  -DROAR_BUILD_DOCUMENTATION=$BUILD_DOCS \
  -DROAR_BUILD_TESTS=on \
  -DENABLE_SANITIZER_THREAD=on \
  -DPROMISE_BUILD_EXAMPLES=off \
  ../..

cd ../..
node ./scripts/copy_compile_commands.js
cd ${BUILD_DIR}

cmake --build . -- -j$THREADS

