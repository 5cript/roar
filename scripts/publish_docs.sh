#!/bin/bash

CURDIR=$(basename $PWD)
if [[ $CURDIR == "scripts" ]]; then
  cd ..
fi

./scripts/build.sh

rm -rf ./gh-pages
mkdir -p gh-pages
cd gh-pages
git clone --branch gh-pages git@github.com:5cript/roar.git .
cd ..

rsync -rts ./build/clang_debug/docs/ gh-pages

# Do manually:
# cd gh-pages
# git add .
# pit push