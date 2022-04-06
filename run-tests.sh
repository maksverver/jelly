#!/bin/sh

set -e -o pipefail

if [ $# = 0 ]; then
    echo "Usage: $0 solver..."
    exit 1
fi

for bin in "$@"; do
    if [ ! -x "./${bin}" ]; then
        echo "Missing executable: '${bin}'"
        exit 1
    fi
done

for solution in solutions/*.txt
do
    level=levels/"$(basename "${solution}")"
    for bin in "$@"; do
        echo "Verifying ${solution} with ${bin}..."
        "./${bin}" "${level}" | diff - "${solution}"
    done
done
