#!/bin/bash

CURDIR=$(basename $PWD)
if [[ $CURDIR == "scripts" ]]; then
  cd ..
fi

DIR="${BASH_SOURCE%/*}"
if [[ ! -d "$DIR" ]]; then DIR="$PWD"; fi
. "$DIR/build_overrides.sh"

export BUILD_DOCS=on

./scripts/build.sh

cd ./build/clang_debug
if [ -z ${DISABLE_DOXYGEN_BUILD+x} ]; then
    cmake --build . --target doxygen
fi
cmake --build . --target documentation