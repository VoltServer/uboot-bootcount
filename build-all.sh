#!/bin/bash -ueE

main() {
  docker build -t bootcount-builder-armv7 \
    --build-arg TARGET_ARCH=arm-linux-gnueabihf .
  docker build -t bootcount-builder-armv8 \
    --build-arg TARGET_ARCH=aarch64-linux-gnu .

  for V in v7 v8; do
    TARBALL=$(docker run --rm -it bootcount-builder-arm$V ls -1 dist/ | tr -d '\r' | tr -d '\n')
    CONTAINER=$(docker create bootcount-builder-arm$V)
    docker cp "$CONTAINER:/opt/dist/$TARBALL" .
    docker rm "$CONTAINER"
  done
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  set -o pipefail
  main "$@"
fi
