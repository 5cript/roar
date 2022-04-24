#!/bin/bash

if [[ $# == 0 ]]; then
    echo "At least executable name required."
    exit 1
fi

if [[ ! -z "${MSYSTEM}" ]]; then
    eval "gdb $1.exe ${@:2}"
else
    eval "gdb $1 ${@:2}"
fi