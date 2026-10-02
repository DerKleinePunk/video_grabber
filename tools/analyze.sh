#!/usr/bin/env bash
# Static analysis and tests, the same steps as CI (.github/workflows/ci.yml).
#
#   tools/analyze.sh
#
# Tools come from the PATH or from these variables:
#   FLUTTER       flutter binary (default: flutter)
#   CLANG_TIDY    clang-tidy (default: clang-tidy)
#   CLANG_FORMAT  clang-format (default: clang-format)
#   CPPCHECK      cppcheck (default: cppcheck)
# The platform view plugin (native/src/grabber_view.cc) needs the
# ivi-homescreen headers; it is checked only when both are set:
#   IHS_SOURCE_DIR  ivi-homescreen checkout
#   IHS_BUILD_DIR   shell build directory (for the generated ihs_export.h)
set -euo pipefail

cd "$(dirname "$0")/.."
FLUTTER=${FLUTTER:-flutter}
CLANG_TIDY=${CLANG_TIDY:-clang-tidy}
CLANG_FORMAT=${CLANG_FORMAT:-clang-format}
CPPCHECK=${CPPCHECK:-cppcheck}
BUILD=${BUILD:-build/analyze}

step() { printf '\n== %s\n' "$*"; }

step "dart format"
"$FLUTTER" pub get >/dev/null
dart_bin="$(dirname "$(command -v "$FLUTTER")")/dart"
"$dart_bin" format --output=none --set-exit-if-changed lib test

step "flutter analyze"
"$FLUTTER" analyze --fatal-infos

step "flutter test"
"$FLUTTER" test

step "clang-format"
mapfile -t native_files < <(git ls-files 'native/*.cc' 'native/*.h')
"$CLANG_FORMAT" --dry-run --Werror "${native_files[@]}"

step "native build and tests"
cmake -S native -B "$BUILD" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null
cmake --build "$BUILD"
ctest --test-dir "$BUILD" --output-on-failure

step "clang-tidy"
mapfile -t core_files < <(git ls-files 'native/src/*.cc' 'native/tools/*.cc' \
  'native/test/*.cc' | grep -v 'native/src/grabber_view.cc')
"$CLANG_TIDY" --quiet -p "$BUILD" "${core_files[@]}"
if [[ -n "${IHS_SOURCE_DIR:-}" && -n "${IHS_BUILD_DIR:-}" ]]; then
  "$CLANG_TIDY" --quiet native/src/grabber_view.cc -- -std=c++17 \
    -Inative/src -I"$IHS_SOURCE_DIR/shared/include" \
    -I"$IHS_BUILD_DIR/shared/include" -I/usr/include/libdrm
else
  echo "grabber_view.cc skipped (set IHS_SOURCE_DIR and IHS_BUILD_DIR)"
fi

step "cppcheck"
"$CPPCHECK" --enable=warning,performance,portability --platform=unix64 \
  --library=posix --inline-suppr --quiet --std=c++17 \
  --check-level=exhaustive --error-exitcode=1 -I native/src \
  native/src native/tools native/test

step "all checks passed"
