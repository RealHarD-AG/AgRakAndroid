#!/usr/bin/env bash
# RakSAMP build helper — configure and compile for a chosen platform.
#
# Usage:
#   ./build.sh <platform> [target] [options]
#
# Platforms:
#   linux   Native Linux build (GCC/Clang)          -> build/
#   win64   Windows x86_64 (MinGW-w64 cross)      -> build_win64/
#   win32   Windows i686   (MinGW-w64 cross)      -> build_win32/
#
# Targets (optional, default: all):
#   all | client | server
#
# Options:
#   -c, --clean     Remove build directory and reconfigure from scratch
#   -j, --jobs N    Parallel build jobs (default: nproc or 4)
#   -h, --help      Show this help
#
# Examples:
#   ./build.sh linux
#   ./build.sh win64 client
#   ./build.sh win32 --clean -j 8

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

PLATFORM=""
TARGET="all"
CLEAN=0
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

usage() {
	sed -n '3,22p' "$0" | sed 's/^# \{0,1\}//'
}

die() {
	echo "error: $*" >&2
	exit 1
}

while [[ $# -gt 0 ]]; do
	case "$1" in
		linux|win64|win32)
			PLATFORM="$1"
			shift
			;;
		all|client|server)
			TARGET="$1"
			shift
			;;
		-c|--clean)
			CLEAN=1
			shift
			;;
		-j|--jobs)
			[[ $# -ge 2 ]] || die "missing value for $1"
			JOBS="$2"
			shift 2
			;;
		-h|--help)
			usage
			exit 0
			;;
		*)
			die "unknown argument: $1 (try --help)"
			;;
	esac
done

[[ -n "$PLATFORM" ]] || { usage; die "platform is required (linux | win64 | win32)"; }

case "$PLATFORM" in
	linux)
		BUILD_DIR="build"
		CMAKE_EXTRA=()
		;;
	win64)
		BUILD_DIR="build_win64"
		CMAKE_EXTRA=(-DCMAKE_TOOLCHAIN_FILE="$ROOT/mingw-w64-x86_64.cmake")
		command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1 \
			|| die "x86_64-w64-mingw32-g++ not found (install: sudo apt install mingw-w64)"
		;;
	win32)
		BUILD_DIR="build_win32"
		CMAKE_EXTRA=(-DCMAKE_TOOLCHAIN_FILE="$ROOT/mingw-w64-i686.cmake")
		command -v i686-w64-mingw32-g++ >/dev/null 2>&1 \
			|| die "i686-w64-mingw32-g++ not found (install: sudo apt install mingw-w64)"
		;;
esac

case "$TARGET" in
	all)     CMAKE_TARGET=() ;;
	client)  CMAKE_TARGET=(--target RakSAMPClient) ;;
	server)  CMAKE_TARGET=(--target RakSAMPServer) ;;
esac

if [[ "$CLEAN" -eq 1 && -d "$BUILD_DIR" ]]; then
	echo ">> removing $BUILD_DIR/"
	rm -rf "$BUILD_DIR"
fi

echo ">> platform: $PLATFORM"
echo ">> build dir: $BUILD_DIR/"
echo ">> target: $TARGET"
echo ">> jobs: $JOBS"

# Client links LuaJIT shared (ffi + lua51.dll-compatible C modules). Always stage it
# before cmake configure so server-only builds still configure cleanly.
echo ">> building LuaJIT for $PLATFORM..."
chmod +x "$ROOT/tools/build_luajit.sh"
"$ROOT/tools/build_luajit.sh" "$PLATFORM" "$ROOT/$BUILD_DIR/luajit"

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
	echo ">> configuring..."
	cmake -B "$BUILD_DIR" "${CMAKE_EXTRA[@]}"
fi

echo ">> building..."
cmake --build "$BUILD_DIR" "${CMAKE_TARGET[@]}" -j "$JOBS"

# Copy default configs next to binaries (overwrite to keep templates up to date).
copy_config() {
	local src="$1" dst="$2"
	if [[ -f "$src" ]]; then
		cp -f "$src" "$dst"
		echo ">> copied $(basename "$src") -> $BUILD_DIR/"
	fi
}

if [[ "$TARGET" == "all" || "$TARGET" == "client" ]]; then
	copy_config "$ROOT/client/bin/RakSAMPClient.xml" "$BUILD_DIR/RakSAMPClient.xml"

	# Lua runtime library and script folder used by the client scripting layer.
	mkdir -p "$BUILD_DIR/lua" "$BUILD_DIR/scripts"
	cp -f "$ROOT"/client/lua/*.lua "$BUILD_DIR/lua/"
	echo ">> copied client/lua/*.lua -> $BUILD_DIR/lua/"
	for f in "$ROOT"/client/scripts/*.lua; do
		[[ -e "$f" ]] || continue
		[[ -e "$BUILD_DIR/scripts/$(basename "$f")" ]] || cp "$f" "$BUILD_DIR/scripts/"
	done

	# Ensure LuaJIT runtime sits next to the client binary.
	case "$PLATFORM" in
		linux)
			cp -f "$BUILD_DIR/luajit/libluajit-5.1.so.2" "$BUILD_DIR/" 2>/dev/null || true
			ln -sfn libluajit-5.1.so.2 "$BUILD_DIR/libluajit-5.1.so"
			;;
		win64|win32)
			cp -f "$BUILD_DIR/luajit/lua51.dll" "$BUILD_DIR/" 2>/dev/null || true
			echo ">> lua51.dll -> $BUILD_DIR/"
			;;
	esac
fi
if [[ "$TARGET" == "all" || "$TARGET" == "server" ]]; then
	copy_config "$ROOT/RakSAMPServer.xml" "$BUILD_DIR/RakSAMPServer.xml"
fi

echo ">> done."
case "$PLATFORM" in
	linux)
		[[ "$TARGET" == "server" ]] || echo "   client: $BUILD_DIR/RakSAMPClient"
		[[ "$TARGET" == "client" ]] || echo "   server: $BUILD_DIR/RakSAMPServer"
		;;
	win64|win32)
		[[ "$TARGET" == "server" ]] || echo "   client: $BUILD_DIR/RakSAMPClient.exe"
		[[ "$TARGET" == "client" ]] || echo "   server: $BUILD_DIR/RakSAMPServer.exe"
		;;
esac
