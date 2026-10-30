#!/usr/bin/env bash
#
# mayhem/build.sh — builds the sanitized fuzz harnesses (+ standalone reproducers) and the
# normal-flags KAT probe that mayhem/test.sh runs. Runs in the commit image as `mayhem` in /mayhem,
# offline-safe and re-runnable: everything it needs is in the tree or installed in the image.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
B="$SRC/mayhem/build"          # gitignored by mayhem/.gitignore
GEN="$B/gen"
mkdir -p "$B/fuzz" "$B/test" "$GEN"

# 1) Generated inputs: upstream builds libcore.pb.{h,cc} with its vendored spb-protoc (no network).
if [ ! -x "$B/spb/src/spb-proto-compiler/spb-protoc" ]; then
  cmake -S 3rdparty/simple-protobuf -B "$B/spb" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" -DCMAKE_C_FLAGS= -DCMAKE_CXX_FLAGS= \
        -DSPB_PROTO_USE_CLANG_FORMAT=OFF >/dev/null
  cmake --build "$B/spb" -j"$MAYHEM_JOBS" --target spb-protoc >/dev/null
fi
"$B/spb/src/spb-proto-compiler/spb-protoc" --cpp_out "$GEN" -I "$SRC" "$SRC/core/gen/libcore.proto"

QT_CFLAGS="$(pkg-config --cflags Qt6Widgets Qt6Network)"
QT_LIBS="$(pkg-config --libs Qt6Widgets Qt6Network)"
INCS="-I$SRC -I$SRC/mayhem/stubs -I$GEN -I$SRC/3rdparty/simple-protobuf/include -I$SRC/3rdparty/SQLiteCpp/include"

# 2) The project code the harnesses reach: configs (outbounds, share links, clash/vpn importers),
#    the global helpers, the database layer and its entities, quirc, and the vendored SQLite.
CXX_SRCS=(
  src/configs/AutoSelectorPlan.cpp src/configs/ParseDiagnostics.cpp src/configs/generate.cpp
  src/configs/sub/*.cpp src/configs/common/*.cpp src/configs/outbounds/*.cpp
  src/global/*.cpp src/database/*.cpp src/database/entities/*.cpp
  src/sys/linux/systemChecks.cpp
  3rdparty/QrDecoder.cpp 3rdparty/qrcodegen.cpp
  3rdparty/SQLiteCpp/src/{Backup,Column,Database,Exception,Savepoint,Statement,Transaction}.cpp
  mayhem/engine_stubs.cpp
)
C_SRCS=( 3rdparty/quirc/*.c 3rdparty/SQLiteCpp/src/sqlite3.c )
GEN_SRCS=( "$GEN/core/gen/libcore.pb.cc" )

# compile_all <outdir> <flags...>: incremental, parallel; archives the objects as <outdir>/libproject.a
# (linking the archive pulls in only what a harness reaches)
compile_all() {
  local out="$1"; shift
  local flags="$*" f o
  if [ "$(cat "$out/flags.txt" 2>/dev/null)" != "$flags" ]; then rm -f "$out"/*.o; printf '%s' "$flags" > "$out/flags.txt"; fi
  : > "$out/jobs.txt"
  for f in "${CXX_SRCS[@]}" "${GEN_SRCS[@]}"; do
    o="$out/$(echo "${f#"$SRC"/}" | tr / _).o"
    [ "$o" -nt "$f" ] || printf '%s\t%s\t%s\n' "$CXX" "$f" "$o" >> "$out/jobs.txt"
  done
  for f in "${C_SRCS[@]}"; do
    o="$out/$(echo "${f#"$SRC"/}" | tr / _).o"
    [ "$o" -nt "$f" ] || printf '%s\t%s\t%s\n' "$CC" "$f" "$o" >> "$out/jobs.txt"
  done
  FLAGS="$flags" INCS="$INCS" QT_CFLAGS="$QT_CFLAGS" \
  xargs -a "$out/jobs.txt" -d '\n' -P "$MAYHEM_JOBS" -I{} bash -c '
      IFS=$'"'"'\t'"'"' read -r cc src obj <<<"{}"
      case "$src" in
        *.c) std="" ;;
        *)   std="-std=c++20" ;;
      esac
      $cc $std -fPIC -w -DQT_MESSAGELOGCONTEXT $FLAGS $INCS $QT_CFLAGS -c "$src" -o "$obj"' 
  rm -f "$out/libproject.a"
  ar rcs "$out/libproject.a" $(for f in "${CXX_SRCS[@]}" "${C_SRCS[@]}" "${GEN_SRCS[@]}"; do echo "$out/$(echo "${f#"$SRC"/}" | tr / _).o"; done)
}

# 3) Sanitized fuzz build (SANITIZER_FLAGS + DWARF<=3 on the project objects and the harnesses).
FUZZ_FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -fno-omit-frame-pointer -fsanitize=fuzzer-no-link"
compile_all "$B/fuzz" "$FUZZ_FLAGS"
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -fPIC -c mayhem/lsan_off.cc -o "$B/fuzz/lsan_off.o"
$CC  $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/fuzz/standalone_main.o"

for h in fuzz_subscription fuzz_quirc; do
  $CXX -std=c++20 -fPIC -w -DQT_MESSAGELOGCONTEXT $FUZZ_FLAGS $INCS $QT_CFLAGS -c "mayhem/$h.cpp" -o "$B/fuzz/harness_$h.o"
  $CXX $FUZZ_FLAGS $LIB_FUZZING_ENGINE "$B/fuzz/harness_$h.o" "$B/fuzz/lsan_off.o" -Wl,--start-group "$B/fuzz/libproject.a" -Wl,--end-group \
       $QT_LIBS -ldl -lpthread -o "$SRC/$h"
  $CXX $FUZZ_FLAGS "$B/fuzz/standalone_main.o" "$B/fuzz/harness_$h.o" "$B/fuzz/lsan_off.o" -Wl,--start-group "$B/fuzz/libproject.a" -Wl,--end-group \
       $QT_LIBS -ldl -lpthread -o "$SRC/$h-standalone"
done

# 4) Test build: the same project code with the project's NORMAL flags + the KAT probe.
if [ -f mayhem/kat.cpp ]; then
  TEST_FLAGS="-O1 $COVERAGE_FLAGS"
  compile_all "$B/test" "$TEST_FLAGS"
  $CXX -std=c++20 -fPIC -w -DQT_MESSAGELOGCONTEXT $TEST_FLAGS $INCS $QT_CFLAGS -c mayhem/kat.cpp -o "$B/test/kat.o"
  $CXX $TEST_FLAGS "$B/test/kat.o" -Wl,--start-group "$B/test/libproject.a" -Wl,--end-group $QT_LIBS -ldl -lpthread -o "$SRC/mayhem/build/kat"
fi
