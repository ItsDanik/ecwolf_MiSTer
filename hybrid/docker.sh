#!/bin/sh
# Run a command in the hybrid core toolchain container, with the current
# directory mounted at the same path. The image is built on first use.
# usage: hybrid/docker.sh <command> [arguments]
set -e
HYBRID=$(cd "$(dirname "$0")" && pwd)
docker image inspect mister-hybrid-tc > /dev/null 2>&1 || docker build -t mister-hybrid-tc "$HYBRID/toolchain"
exec docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -v "$PWD":"$PWD" -w "$PWD" mister-hybrid-tc "$@"
