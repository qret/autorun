#!/bin/sh
# Our WoW64 FEX for fixed-low 32-bit games in the alias window: the revision and
# Horizon patch the DLL repository builds libwow64fex.dll from
# (horizon-dlls/tools/build-fex.sh), plus fex/alias-window.patch. It is shipped
# in the Autorun package under its own name, libwow64fex-window.dll, so the
# DLL repository's libwow64fex.dll stays exactly what it is; the runtime loads
# ours only for a game in the window (runtime.c, runtime_fex_window).
#   WINE_NX_LLVM_MINGW=... sh horizon-wine/tools/build-fex-window.sh [--source-only]
# --source-only prepares the patched checkout and stops: the FEXCore tests are
# built from it, so they test the source the module is built from.
set -eu
source_only=0
[ "${1:-}" != --source-only ] || source_only=1
horizon_wine="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
source_dir="${WINE_NX_FEX_WINDOW_DIR:-$horizon_wine/toolchains/fex-2609-window}"
build="${WINE_NX_FEX_WINDOW_BUILD_DIR:-$horizon_wine/toolchains/build-fex-2609-window}"
revision=395b132f346b1a45def246d10c52245edba1ef02
patches="$horizon_wine/fex/horizon.patch $horizon_wine/fex/alias-window.patch"

if [ -n "${WINE_NX_LLVM_MINGW:-}" ]; then
    export PATH="$WINE_NX_LLVM_MINGW/bin:$PATH"
fi
tools="git"
[ "$source_only" = 1 ] || tools="git cmake ninja python3 aarch64-w64-mingw32-clang aarch64-w64-mingw32-clang++"
for tool in $tools; do
    command -v "$tool" >/dev/null || { echo "Missing $tool; set WINE_NX_LLVM_MINGW." >&2; exit 1; }
done
if [ ! -d "$source_dir/.git" ]; then
    if [ -e "$source_dir" ]; then
        echo "Refusing to overwrite $source_dir" >&2
        exit 1
    fi
    mkdir -p "$source_dir"
    git -C "$source_dir" init -q
    git -C "$source_dir" remote add origin https://github.com/FEX-Emu/FEX.git
    git -C "$source_dir" fetch -q --depth=1 origin "$revision"
    git -C "$source_dir" checkout -q --detach FETCH_HEAD
    git -C "$source_dir" submodule update -q --init --depth=1 \
        External/fmt External/xxhash External/range-v3 External/unordered_dense \
        External/rpmalloc Source/Common/cpp-optparse
fi
if [ "$(git -C "$source_dir" rev-parse HEAD)" != "$revision" ]; then
    echo "FEX must be at $revision; no files were reset." >&2
    exit 1
fi
# The checkout is this script's alone: it is put back to the revision and the
# patches applied again, so a changed patch always takes effect.
git -C "$source_dir" checkout -q -- .
git -C "$source_dir" clean -q -fd -e '/External/' -e '/Source/Common/cpp-optparse/'
for patch in $patches; do
    git -C "$source_dir" apply "$patch"
done
source_dir="$(CDPATH= cd -- "$source_dir" && pwd)"
[ "$source_only" = 0 ] || { echo "$source_dir"; exit 0; }

cmake -S "$source_dir" -B "$build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$source_dir/Data/CMake/toolchain_mingw.cmake" \
    -DMINGW_TRIPLE=aarch64-w64-mingw32 -DCMAKE_BUILD_TYPE=Release \
    -DOVERRIDE_VERSION=FEX-2609 -DOVERRIDE_HASH="$revision" \
    -DENABLE_LTO=OFF -DBUILD_TESTING=OFF -DBUILD_FEXCONFIG=OFF \
    -DENABLE_JEMALLOC_GLIBC_ALLOC=OFF -DENABLE_OFFLINE_TELEMETRY=OFF -DTUNE_CPU=none \
    -DFEX_HORIZON=ON -DFEX_HORIZON_ABI_DIR="$horizon_wine/fex" >/dev/null
cmake --build "$build" --target wow64fex -j "${WINE_NX_JOBS:-4}"
cp "$build/Bin/libwow64fex.dll" "$build/libwow64fex-window.dll"
cp "$source_dir/LICENSE" "$build/FEX-MIT.txt"
echo "$build/libwow64fex-window.dll"
