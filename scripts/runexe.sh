#!/bin/bash

if [[ $# == 0 ]]; then
    echo "At least executable name required."
    exit 1
fi

if [[ ! -z "${MSYSTEM}" ]]; then
    eval "$1.exe ${@:2}"
else
    eval "$1 ${@:2}"
fi