#!/bin/bash

CURDIR=$(basename $PWD)
if [[ $CURDIR == "scripts" ]]; then
  cd ..
fi

rm -rf ./gh-pages
mkdir -p gh-pages
cd gh-pages
git clone --branch gh-pages git@github.com:5cript/roar.git .
cd ..

./scripts/build_docs.sh
rsync -rts ./build/clang_debug/docs/ gh-pages

# Do manually:
# cd gh-pages
# git add .
# git commit -m "Update documentation."
# pit push