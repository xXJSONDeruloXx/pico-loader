#!/usr/bin/env bash
set -euo pipefail

BLOCKSDS_IMAGE="skylyrac/blocksds:slim-latest"
DOTNET_IMAGE="mcr.microsoft.com/dotnet/sdk:9.0"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

run_dotnet() {
  docker run --rm \
    -v "$REPO_ROOT:/work" \
    -w /work \
    "$DOTNET_IMAGE" \
    "$@"
}

run_blocksds() {
  docker run --rm \
    -v "$REPO_ROOT:/work" \
    -w /work \
    "$BLOCKSDS_IMAGE" \
    make "$@"
}

if [ "$#" -eq 0 ]; then
  run_dotnet dotnet build tools/PicoLoaderConverter/PicoLoaderConverter.sln
  run_dotnet dotnet tools/PicoLoaderConverter/PicoLoaderConverter/bin/Debug/net9.0/PicoLoaderConverter.dll aplist -i data/aplist.csv -o data/aplist.bin
  run_dotnet dotnet tools/PicoLoaderConverter/PicoLoaderConverter/bin/Debug/net9.0/PicoLoaderConverter.dll savelist -i data/savelist.csv -o data/savelist.bin
  run_dotnet dotnet tools/PicoLoaderConverter/PicoLoaderConverter/bin/Debug/net9.0/PicoLoaderConverter.dll patchlist -i data/patchlist.json -o data/patchlist.bin
  run_blocksds loader9 loader7
else
  run_blocksds "$@"
fi
