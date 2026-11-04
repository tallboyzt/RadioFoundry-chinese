#!/bin/bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors
# macOS build of the csky-elf cross toolchain for the hrc7000 platform (C-SKY CK803S).
#
# The upstream tools/build-csky-toolchain.sh is tested on Ubuntu 22.04 arm64. On macOS it additionally needs
# --with-system-zlib and MAKEINFO=true (its own header says so, but does not apply them), the Homebrew
# bison/flex/texinfo in PATH, and the Homebrew gmp/mpfr/mpc/isl in CPPFLAGS/LDFLAGS. This wrapper supplies those
# and then runs the upstream script. Result: $HOME/toolchains/csky-elf/bin/csky-elf-gcc.
#
# Prerequisites (Homebrew): brew install bison flex texinfo gmp mpfr mpc isl
set -e
BREW=${BREW:-/opt/homebrew}
export PATH="$BREW/opt/bison/bin:$BREW/opt/flex/bin:$BREW/opt/texinfo/bin:$BREW/opt/gmp/bin:$BREW/opt/mpfr/bin:$BREW/opt/mpc/bin:$BREW/opt/isl/bin:$PATH"
# mpc is the command, libmpc the library: only libmpc has the include/lib directories GCC's configure needs
export CPPFLAGS="$CPPFLAGS -I$BREW/opt/gmp/include -I$BREW/opt/mpfr/include -I$BREW/opt/libmpc/include -I$BREW/opt/isl/include"
export LDFLAGS="$LDFLAGS -L$BREW/opt/gmp/lib -L$BREW/opt/mpfr/lib -L$BREW/opt/libmpc/lib -L$BREW/opt/isl/lib"
# MAKEINFO=true: skip the GCC/texinfo doc builds (the docs are not needed and slow the build)
export MAKEINFO=true
# The macOS SDK predefines TARGET_OS_MAC, which binutils' bundled zlib (zutil.h) reads as "a Mac without fdopen" and
# redefines fdopen to NULL, colliding with the SDK's declaration; drop the SDK's target-OS macros for the build
export CPPFLAGS="$CPPFLAGS -UMACOS -UTARGET_OS_MAC"
# The upstream script finds platform_toolchain.sh beside itself; this wrapper is beside it, so give it the version it
# needs (the upstream is copied to /tmp to apply --with-system-zlib, which a plain env var cannot reach)
export GV=$("$(dirname "$0")/platform_toolchain.sh" hrc7000 version)
UP=$(cd "$(dirname "$0")" && pwd)/build-csky-toolchain.sh
TMP=$(mktemp /tmp/rfoundry-csky.XXXXXX)
trap 'rm -f "$TMP"' EXIT
# --with-system-zlib: the cross target is -without-headers, so no target zlib; use the host's zlib (the macOS caveat)
sed 's/--without-headers --with-newlib/--without-headers --with-newlib --with-system-zlib/' "$UP" > "$TMP"
chmod +x "$TMP"
exec "$TMP" "$@"
