#!/usr/bin/env bash
# Set up (or repair) everything this project needs, in a fresh checkout or a new
# cloud session. Idempotent: finished steps are skipped. Downloads are pinned: tarballs
# by SHA256, git repos by commit, Python packages by requirements.txt, npm packages by
# config/npm/package-lock.json.
#
#   ./setup.sh                 toolchain, decomp tools, mGBA, REA + Ghidra, generate asm, make compare + make test
#   ./setup.sh --no-ghidra     skip Ghidra/JDK/Rizin (~500 MB of downloads; REA is installed but has no provider)
#   ./setup.sh --ghidra-functions   also re-run the Ghidra function export (discovery input)
#   PYTHON=python3.12 ./setup.sh    make .venv with this Python (3.11 or newer; by default the first
#                              of python3, python3.14 ... python3.11 that is new enough)
#
# Installs into tools/ (gitignored) and .venv/; missing system packages with apt-get, dnf or pacman.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$PWD"

WITH_GHIDRA=1
GHIDRA_FUNCTIONS=0
for arg in "$@"; do
    case "$arg" in
        --no-ghidra) WITH_GHIDRA=0 ;;
        --ghidra-functions) GHIDRA_FUNCTIONS=1 ;;
        -h|--help) sed -n '2,11p' "$0"; exit 0 ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

# ---- pinned versions ---------------------------------------------------------
BASEROM_SHA1=6025ef597db2684ae064ffe22b4fd1d37941f887   # Kurukuru Kururin (Europe), CRC32 c4dab956
AGBCC_REPO=https://github.com/pret/agbcc.git
AGBCC_REV=da598c1d918402c42c0c0d7128ba14567f3175e9
M2C_REPO=https://github.com/matt-kempster/m2c.git
M2C_REV=708d2d2cb2698f091a92492b328f73b24209f72d
PERMUTER_REPO=https://github.com/simonlindholm/decomp-permuter.git
PERMUTER_REV=8556c81d80d1c1af98a858c8f4dc951357f29139
OBJDIFF_VERSION=3.8.2
OBJDIFF_SHA256=e5445089a6f707e30cb5e02992451b604619cedfd2660ee2373a86bfefd9e718
NODE_FALLBACK=22.22.0           # installed only if the system node is missing or too old
NODE_SHA256=9aa8e9d2298ab68c600bd6fb86a6c13bce11a4eca1ba9b39d79fa021755d7c37
GHIDRA_VERSION=12.1.4
GHIDRA_ZIP=ghidra_12.1.4_PUBLIC_20260921.zip
GHIDRA_SHA256=ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db
RIZIN_VERSION=0.7.4
RIZIN_SHA256=db51f7c2def9c5c09df4e4102e9e294a030c77bd92a319ced6ec216293064d63
KURUTOOLS_REV=855a8174615577f1ecf6f3b5725f65d62ec11646
KURURINTAS_REV=299ea46f23b1ad39c1d9eefdbc8810c7c2be10df
MGBA_REPO=https://github.com/mgba-emu/mgba.git
MGBA_REV=26b7884bc25a5933960f3cdcd98bac1ae14d42e2   # tag 0.10.5: libmgba for make test (tests/gameplay)

step() { printf '\n=== %s\n' "$*"; }
have() { command -v "$1" >/dev/null 2>&1; }
SUDO=""; [ "$(id -u)" -ne 0 ] && have sudo && SUDO="sudo"

clone_at() {  # clone_at <repo> <dir> <rev>
    local repo="$1" dir="$2" rev="$3"
    if [ ! -d "$dir/.git" ]; then
        git clone -q "$repo" "$dir"
    fi
    if [ "$(git -C "$dir" rev-parse HEAD)" != "$rev" ]; then
        git -C "$dir" fetch -q origin "$rev" 2>/dev/null || git -C "$dir" fetch -q origin
        git -C "$dir" checkout -q "$rev"
    fi
}

download() {  # download <url> <out> <sha256>
    local url="$1" out="$2" sum="$3"
    if [ -f "$out" ] && echo "$sum  $out" | sha256sum -c --status -; then return; fi
    curl -fsSL -o "$out.part" "$url"
    echo "$sum  $out.part" | sha256sum -c --status - || { echo "checksum mismatch: $url" >&2; exit 1; }
    mv "$out.part" "$out"
}

# The builds below write their output to a log, so a failure must show the log's end: under
# set -e the script would otherwise stop right after the step's heading, with no message.
build_failed() {  # build_failed <what> <log>
    echo "$1 failed; last lines of $2:" >&2
    tail -n 40 "$2" >&2
    exit 1
}

# ---- baserom -----------------------------------------------------------------
step "Checking baserom.gba"
if [ ! -f baserom.gba ]; then
    MAIN="$(cd "$(dirname "$(git rev-parse --git-common-dir 2>/dev/null || echo .git)")" && pwd -P)"
    if [ "$MAIN" != "$ROOT" ] && [ -f "$MAIN/baserom.gba" ]; then
        cp "$MAIN/baserom.gba" baserom.gba   # a git worktree: borrow the main checkout's copy
    else
        echo "baserom.gba not found: copy your own Kuru Kuru Kururin (Europe) dump to $ROOT/baserom.gba" >&2
        echo "(git does not track it; its SHA1 must be $BASEROM_SHA1)." >&2
        exit 1
    fi
fi
if ! echo "$BASEROM_SHA1  baserom.gba" | sha1sum -c --status -; then
    echo "baserom.gba is not the dump this project needs: its SHA1 is $(sha1sum baserom.gba | cut -d' ' -f1)," >&2
    echo "the Kurukuru Kururin (Europe) cartridge's is $BASEROM_SHA1 (4 MiB, CRC32 c4dab956)." >&2
    exit 1
fi
echo "baserom.gba: OK"

# ---- Python ------------------------------------------------------------------------
# The scripts need Python 3.11 or newer (tomllib). PY: the interpreter .venv is made with.
python_ok() { "$1" -c 'import sys; sys.exit(sys.version_info < (3, 11))' >/dev/null 2>&1; }
pyver() { "$1" -c 'import sys; print("%d.%d" % sys.version_info[:2])'; }
pick_python() {
    local p
    for p in ${PYTHON:+"$PYTHON"} python3 python3.14 python3.13 python3.12 python3.11; do
        if have "$p" && python_ok "$p"; then PY="$p"; break; fi
        if [ -n "${PYTHON:-}" ]; then break; fi   # PYTHON names the only one to try
    done
    return 0
}
need_python() {
    local p="${PYTHON:-python3}"
    echo "Python 3.11 or newer is needed (the scripts read TOML with tomllib); $p is" \
         "$(if have "$p"; then "$p" -V 2>&1; else echo "not installed"; fi)." >&2
    echo "Install a newer Python (Ubuntu 22.04: sudo apt install python3.11 python3.11-venv) and run" \
         "./setup.sh again: it finds python3.11 ... python3.14 itself, or set PYTHON=/path/to/python3.x." >&2
    exit 1
}
PY=""
pick_python
if [ -z "$PY" ] && { [ -n "${PYTHON:-}" ] || have python3; }; then need_python; fi

# ---- system packages -----------------------------------------------------------
step "System packages"
# What is missing, by its Debian/Ubuntu package (apt-get installs them); on Fedora (dnf) and
# Arch (pacman) pkg_for gives the package of the same name there.
PKGS=()
have arm-none-eabi-as || PKGS+=(binutils-arm-none-eabi)
have arm-none-eabi-cpp || PKGS+=(gcc-arm-none-eabi)
have gcc || PKGS+=(build-essential)
have g++ || PKGS+=(g++)            # mGBA's CMake project enables C++ (none of it is built)
have make || PKGS+=(make)
have cmake || PKGS+=(cmake)
have git || PKGS+=(git)
have curl || PKGS+=(curl)
have unzip || PKGS+=(unzip)
have xz || PKGS+=(xz-utils)        # tar -xJf (Node fallback, Rizin)
have cmp || PKGS+=(diffutils)      # the up-to-date checks below
have ps || PKGS+=(procps)          # REA's process supervisor needs a procps `ps` on Linux
if [ -z "$PY" ]; then
    PKGS+=(python3-venv)            # no python3 at all: python3-venv brings it
elif ! "$PY" -c "import venv, ensurepip" 2>/dev/null; then
    PKGS+=("python$(pyver "$PY")-venv")
fi
JDK=openjdk-21-jdk                  # Ghidra 12.1 needs a full JDK 21+
if [ "$WITH_GHIDRA" = 1 ] && ! { have javac && javac -version 2>&1 | grep -qE '^javac (2[1-9]|[3-9][0-9])([^0-9]|$)'; }; then
    PKGS+=("$JDK")
fi
pkg_for() {  # pkg_for dnf|pacman <Debian package>: the package(s) on Fedora or Arch
    case "$1:$2" in
        dnf:binutils-arm-none-eabi) echo arm-none-eabi-binutils-cs ;;
        dnf:gcc-arm-none-eabi) echo arm-none-eabi-gcc-cs ;;
        dnf:build-essential) echo gcc make ;;
        dnf:g++) echo gcc-c++ ;;
        dnf:xz-utils) echo xz ;;
        dnf:procps) echo procps-ng ;;
        dnf:python3*-venv) echo python3 ;;
        dnf:"$JDK") echo java-21-openjdk-devel ;;
        pacman:binutils-arm-none-eabi) echo arm-none-eabi-binutils ;;
        pacman:gcc-arm-none-eabi) echo arm-none-eabi-gcc ;;
        pacman:build-essential | pacman:g++) echo gcc make ;;
        pacman:xz-utils) echo xz ;;
        pacman:procps) echo procps-ng ;;
        pacman:python3*-venv) echo python ;;
        pacman:"$JDK") echo jdk21-openjdk ;;
        *) echo "$2" ;;             # make, cmake, git, curl, unzip, diffutils
    esac
}
install_failed() {  # install_failed <packages...>
    echo "could not install: $*" >&2
    case " ${PKGS[*]} " in *" $JDK "*)
        echo "(Ghidra needs a JDK 21 or newer, which some releases do not package, e.g. Debian 12:" \
             "install one with javac on PATH, or run ./setup.sh --no-ghidra, which needs none)" >&2 ;;
    esac
    exit 1
}
if [ ${#PKGS[@]} -eq 0 ]; then
    echo "all present"
elif have apt-get; then
    echo "installing: ${PKGS[*]}"
    $SUDO env DEBIAN_FRONTEND=noninteractive apt-get update -qq
    $SUDO env DEBIAN_FRONTEND=noninteractive apt-get install -y -qq "${PKGS[@]}" || install_failed "${PKGS[@]}"
elif have dnf || have pacman; then
    MGR=$(have dnf && echo dnf || echo pacman)
    NATIVE=()
    for p in "${PKGS[@]}"; do
        for q in $(pkg_for "$MGR" "$p"); do
            case " ${NATIVE[*]:-} " in *" $q "*) ;; *) NATIVE+=("$q") ;; esac
        done
    done
    echo "installing: ${NATIVE[*]}"
    if [ "$MGR" = dnf ]; then
        $SUDO dnf install -y -q "${NATIVE[@]}" || install_failed "${NATIVE[@]}"
    else
        $SUDO pacman -S --needed --noconfirm "${NATIVE[@]}" || {
            echo "(if the mirrors no longer have these versions: sudo pacman -Syu, then ./setup.sh again)" >&2
            install_failed "${NATIVE[@]}"; }
    fi
else
    echo "missing (as Debian/Ubuntu packages): ${PKGS[*]}" >&2
    echo "setup.sh installs packages with apt-get, dnf or pacman, and found none of them:" \
         "install these yourself and run ./setup.sh again." >&2
    exit 1
fi
[ -n "$PY" ] || pick_python
[ -n "$PY" ] || need_python
echo "Python: $PY ($("$PY" -V 2>&1))"

mkdir -p tools tools/bin tools/downloads

# ---- Node (for asmlift and REA) ---------------------------------------------------
step "Node.js"
node_ok() {
    have node || return 1
    local v; v="$(node -p 'process.versions.node')"
    local major="${v%%.*}" minor; minor="$(echo "$v" | cut -d. -f2)"
    { [ "$major" = 22 ] && [ "$minor" -ge 19 ]; } || { [ "$major" = 24 ] && [ "$minor" -ge 11 ]; } || [ "$major" -ge 26 ]
}
if [ -x tools/node/bin/node ]; then export PATH="$ROOT/tools/node/bin:$PATH"; fi
if ! node_ok; then
    NODE_TGZ="node-v$NODE_FALLBACK-linux-x64.tar.xz"
    download "https://nodejs.org/dist/v$NODE_FALLBACK/$NODE_TGZ" "tools/downloads/$NODE_TGZ" "$NODE_SHA256"
    rm -rf tools/node && mkdir -p tools/node
    tar -xJf "tools/downloads/$NODE_TGZ" -C tools/node --strip-components=1
    rm -f "tools/downloads/$NODE_TGZ"
    export PATH="$ROOT/tools/node/bin:$PATH"
fi
if [ -x tools/node/bin/node ]; then
    # the asmlift/rea shims are `#!/usr/bin/env node`: make tools/bin enough on PATH
    for b in node npm npx; do ln -sf ../node/bin/$b tools/bin/$b; done
fi
echo "node $(node --version)"

# ---- agbcc -----------------------------------------------------------------------
step "agbcc ($AGBCC_REV)"
if [ -x tools/agbcc/bin/agbcc ] && [ "$(cat tools/agbcc/.rev 2>/dev/null)" = "$AGBCC_REV" ]; then
    echo "up to date"
else
    clone_at "$AGBCC_REPO" tools/agbcc-src "$AGBCC_REV"
    (cd tools/agbcc-src && ./build.sh >../agbcc-build.log 2>&1) || build_failed "agbcc build" tools/agbcc-build.log
    # a relative path: install.sh does not quote it, and $ROOT may hold a space
    (cd tools/agbcc-src && ./install.sh ../.. >>../agbcc-build.log 2>&1) || build_failed "agbcc install" tools/agbcc-build.log
    echo "$AGBCC_REV" > tools/agbcc/.rev
    echo "built (log: tools/agbcc-build.log)"
fi

# ---- Python: Luvdis, m2c, capstone, permuter deps ------------------------------------
step "Python venv"
# .venv/bin/python3 links to the Python it was made with: when that Python is upgraded (a new
# distribution release, a rolling update), the venv runs the new version against the old one's
# site-packages, without pip or any package, while requirements.txt is unchanged. Such a venv
# (or one made by a Python older than 3.11, or not by the Python that PYTHON names) is made again.
venv_made() { sed -n -E 's/^version(_info)? *= *([0-9]+\.[0-9]+).*/\2/p' .venv/pyvenv.cfg 2>/dev/null; }
venv_ok() {
    [ -x .venv/bin/python3 ] && python_ok .venv/bin/python3 &&
        [ "$(venv_made)" = "$(pyver .venv/bin/python3)" ] && .venv/bin/python3 -c "import pip" 2>/dev/null &&
        { [ -z "${PYTHON:-}" ] || [ "$(venv_made)" = "$(pyver "$PY")" ]; }
}
if { [ -e .venv ] || [ -L .venv ]; } && ! venv_ok; then
    echo "making .venv again: it was made with Python $(venv_made), and runs" \
         "$(.venv/bin/python3 -V 2>&1 || echo "no Python (.venv/bin/python3 is missing)")"
    rm -rf .venv
fi
[ -d .venv ] || "$PY" -m venv .venv
if ! cmp -s requirements.txt .venv/.requirements; then
    .venv/bin/pip install -q -r requirements.txt
    cp requirements.txt .venv/.requirements
fi
clone_at "$M2C_REPO" tools/m2c "$M2C_REV"
if [ "$(cat .venv/.m2c-rev 2>/dev/null)" != "$M2C_REV" ]; then
    .venv/bin/pip install -q --no-deps --no-build-isolation ./tools/m2c   # build backend from requirements.txt
    echo "$M2C_REV" > .venv/.m2c-rev
fi
clone_at "$PERMUTER_REPO" tools/decomp-permuter "$PERMUTER_REV"
echo "luvdis $(.venv/bin/pip show luvdis | sed -n 's/^Version: //p'), m2c @ ${M2C_REV:0:7}, permuter @ ${PERMUTER_REV:0:7}"

# ---- objdiff-cli, asmlift --------------------------------------------------------------
step "objdiff-cli $OBJDIFF_VERSION; asmlift and rea-agents (config/npm)"
download "https://github.com/encounter/objdiff/releases/download/v$OBJDIFF_VERSION/objdiff-cli-linux-x86_64" \
    tools/objdiff-cli "$OBJDIFF_SHA256"
chmod +x tools/objdiff-cli
mkdir -p tools/npm
if ! cmp -s config/npm/package-lock.json tools/npm/package-lock.json || [ ! -d tools/npm/node_modules ]; then
    cp config/npm/package.json config/npm/package-lock.json tools/npm/
    npm ci -s --prefix tools/npm >/dev/null
fi
ln -sf ../npm/node_modules/.bin/asmlift tools/bin/asmlift
ln -sf ../npm/node_modules/.bin/rea tools/bin/rea
ln -sf ../objdiff-cli tools/bin/objdiff-cli

# ---- research repositories (naming references) -----------------------------------------
step "Research repositories (KuruTools, KururinTAS)"
mkdir -p tools/research
clone_at https://github.com/E-Sh4rk/KuruTools.git tools/research/KuruTools "$KURUTOOLS_REV"
clone_at https://github.com/E-Sh4rk/KururinTAS.git tools/research/KururinTAS "$KURURINTAS_REV"

# ---- mGBA (libmgba, headless) for make test --------------------------------------------
step "mGBA ${MGBA_REV:0:7} (libmgba for tests/gameplay)"
# CMake 4 refuses projects that declare cmake_minimum_required < 3.5 (mGBA's pinned revision
# does): CMAKE_POLICY_VERSION_MINIMUM lets it configure; older CMake ignores the variable.
mgba_failed() { build_failed "mGBA $1" tools/mgba-build.log; }
if [ -f tools/mgba/lib/libmgba.a ] && [ "$(cat tools/mgba/.rev 2>/dev/null)" = "$MGBA_REV" ]; then
    echo "up to date"
else
    clone_at "$MGBA_REPO" tools/mgba-src "$MGBA_REV"
    # Only the GBA core as a static library: no frontends, no optional dependencies.
    cmake -S tools/mgba-src -B tools/mgba-src/build -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$ROOT/tools/mgba" -DCMAKE_INSTALL_LIBDIR=lib \
        -DBUILD_SHARED=OFF -DBUILD_STATIC=ON -DBUILD_LTO=OFF -DBUILD_QT=OFF -DBUILD_SDL=OFF \
        -DBUILD_GL=OFF -DBUILD_GLES2=OFF -DBUILD_GLES3=OFF -DBUILD_LIBRETRO=OFF \
        -DBUILD_PYTHON=OFF -DBUILD_SUITE=OFF -DBUILD_CINEMA=OFF -DBUILD_PERF=OFF \
        -DBUILD_UPDATER=OFF -DBUILD_ROM_TEST=OFF -DM_CORE_GB=OFF -DUSE_DEBUGGERS=OFF \
        -DUSE_EDITLINE=OFF -DUSE_GDB_STUB=OFF -DUSE_FFMPEG=OFF -DUSE_ZLIB=OFF -DUSE_MINIZIP=OFF \
        -DUSE_PNG=OFF -DUSE_LIBZIP=OFF -DUSE_SQLITE3=OFF -DUSE_ELF=OFF -DUSE_LUA=OFF \
        -DUSE_LZMA=OFF -DUSE_DISCORD_RPC=OFF -DUSE_EPOXY=OFF -DENABLE_SCRIPTING=OFF \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        >tools/mgba-build.log 2>&1 || mgba_failed configure
    cmake --build tools/mgba-src/build -j"$(nproc)" >>tools/mgba-build.log 2>&1 || mgba_failed build
    rm -rf tools/mgba
    cmake --install tools/mgba-src/build >>tools/mgba-build.log 2>&1 || mgba_failed install
    echo "$MGBA_REV" > tools/mgba/.rev
    echo "built (log: tools/mgba-build.log)"
fi

# ---- Ghidra, Rizin, REA ---------------------------------------------------------------
if [ "$WITH_GHIDRA" = 1 ]; then
    step "Ghidra $GHIDRA_VERSION (headless)"
    if [ ! -x "tools/ghidra_${GHIDRA_VERSION}_PUBLIC/support/analyzeHeadless" ]; then
        download "https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_${GHIDRA_VERSION}_build/$GHIDRA_ZIP" \
            "tools/downloads/$GHIDRA_ZIP" "$GHIDRA_SHA256"
        unzip -q -o "tools/downloads/$GHIDRA_ZIP" -d tools
        rm -f "tools/downloads/$GHIDRA_ZIP"
    fi
    ln -sfn "ghidra_${GHIDRA_VERSION}_PUBLIC" tools/ghidra

    step "Rizin $RIZIN_VERSION"
    if [ ! -x tools/rizin/bin/rizin ]; then
        download "https://github.com/rizinorg/rizin/releases/download/v$RIZIN_VERSION/rizin-v$RIZIN_VERSION-static-x86_64.tar.xz" \
            "tools/downloads/rizin.tar.xz" "$RIZIN_SHA256"
        mkdir -p tools/rizin && tar -xJf tools/downloads/rizin.tar.xz -C tools/rizin
        rm -f tools/downloads/rizin.tar.xz
    fi
    ln -sf ../rizin/bin/rizin tools/bin/rizin

    step "REA doctor (Ghidra provider)"
    # .mcp.json starts this same copy through scripts/rea_mcp.sh.
    GHIDRA_INSTALL_DIR="$ROOT/tools/ghidra" \
    JAVA_HOME="${JAVA_HOME:-$(dirname "$(dirname "$(readlink -f "$(command -v javac)")")")}" \
        tools/bin/rea doctor --provider ghidra --json >tools/rea-doctor.json 2>&1 \
        && echo "rea doctor (ghidra): healthy" \
        || { echo "rea doctor reported a problem; see tools/rea-doctor.json" >&2; }
fi

# ---- generate + verify -----------------------------------------------------------------
if [ "$GHIDRA_FUNCTIONS" = 1 ]; then
    step "Ghidra function export (ghidra_out/functions.txt)"
    make ghidra-functions GHIDRA_INSTALL_DIR="$ROOT/tools/ghidra"
fi

step "Generating asm/ from baserom.gba"
.venv/bin/python3 scripts/generate_asm.py

step "Test save images (tests/gameplay/saves/*.sav, from their .toml and baserom.gba)"
.venv/bin/python3 tools/make_save.py   # make test builds them again for each input that boots with one

step "make compare (informational: the ROM no longer has to match byte for byte)"
make compare || echo "make compare failed (see above); a SHA1 mismatch alone is expected: the game's C is not byte-identical"

step "make test (gameplay equivalence: the required check)"
make test

cat <<EOF

=== Setup complete
Add the tools to PATH for this shell:   export PATH="$ROOT/tools/bin:\$PATH"
EOF
