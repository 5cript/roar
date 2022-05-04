#!/bin/bash

CURDIR=$(basename $PWD)
if [[ $CURDIR == "scripts" ]]; then
  cd ..
fi

export BUILD_DOCS=on

./scripts/build.sh

cd ./build/clang_debug
cmake --build . --target documentation