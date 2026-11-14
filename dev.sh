#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
mode="${1:-run}"
if [[ $# -gt 0 ]]; then shift; fi
case "$mode" in
    build|run|debug|test|demo|benchmark) ;;
    *) echo "Usage: $0 [build|run|debug|test|demo|benchmark]" >&2; exit 2 ;;
esac

local_tools="${HOME}/.local/share/fxbviewer-dev"
if [[ -n "${QMAKE:-}" ]]; then
    qmake_bin="$QMAKE"
elif command -v qmake6 >/dev/null 2>&1; then
    qmake_bin="$(command -v qmake6)"
elif [[ -x "$local_tools/qt6/usr/bin/qmake6" ]]; then
    qmake_bin="$local_tools/qt6/usr/bin/qmake6"
elif command -v qmake >/dev/null 2>&1; then
    qmake_bin="$(command -v qmake)"
elif [[ -x "$local_tools/qt/usr/lib/qt5/bin/qmake" ]]; then
    qmake_bin="$local_tools/qt/usr/lib/qt5/bin/qmake"
else
    echo 'Qt development tools are required. Install Qt 6 (or Qt 5.15), or set QMAKE.' >&2
    exit 1
fi

qt_version="$("$qmake_bin" -query QT_VERSION)"
qt_libs="$("$qmake_bin" -query QT_INSTALL_LIBS)"
build_dir="${FXBVIEWER_BUILD_DIR:-${repo_dir}-build/qt${qt_version%%.*}-debug}"
qmake_args=("QMAKE_LIBDIR+=$qt_libs")
if [[ "$qt_libs" == "$local_tools/"* ]]; then
    export LD_LIBRARY_PATH="$qt_libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
mkdir -p "$build_dir"
cd "$build_dir"
if [[ "$mode" == benchmark ]]; then
    mkdir -p benchmark
    cd benchmark
    "$qmake_bin" "$repo_dir/benchmarks/benchmarks.pro" CONFIG+=release CONFIG-=debug "${qmake_args[@]}" >&2
    make -j"${JOBS:-4}" >&2
    exec ./fxbviewer_benchmark "$@"
fi
if [[ "$mode" == test ]]; then
    mkdir -p tests
    cd tests
    "$qmake_bin" "$repo_dir/tests/tests.pro" CONFIG+=debug CONFIG-=release "${qmake_args[@]}"
    make -j"${JOBS:-4}"
    export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
    exec ./fxbviewer_tests "$@"
fi
"$qmake_bin" "$repo_dir/fxbViewer.pro" CONFIG+=debug CONFIG-=release "${qmake_args[@]}"
make -j"${JOBS:-4}"
case "$mode" in
    build) echo "Built with Qt $qt_version: $build_dir/fxbViewer" ;;
    run) exec ./fxbViewer "$@" ;;
    demo) exec ./fxbViewer --demo "$@" ;;
    debug)
        exec gdb -q \
            -ex 'set debuginfod enabled off' \
            -ex 'set pagination off' \
            -ex 'set logging file gdb.log' \
            -ex 'set logging overwrite on' \
            -ex 'set logging enabled on' \
            -ex run --args ./fxbViewer
        ;;
esac
