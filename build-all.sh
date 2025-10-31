#!/usr/bin/env bash
set -ueE

main() {
  mkdir -p dist
  docker buildx bake "$@"
  ls -lh dist/
}

if [[ ${BASH_SOURCE[0]} == "$0" ]]; then
  set -o pipefail
  main "$@"
fi
