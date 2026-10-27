#!/bin/bash
# Build tiga varian program uji + hasil rekonstruksi.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
clang -O0 -g -o build/first_test main.c                                   # dengan simbol & debug info
clang -O0 -o build/first_test_stripped main.c && strip build/first_test_stripped  # tanpa nama fungsi (seperti Pinball.exe tanpa PDB)
clang -O2 -o build/first_test_O2 main.c && strip build/first_test_O2      # dioptimasi
clang -O0 -o build/rebuilt rebuilt.c                                       # ditulis ulang dari hasil decompile
for arg in "" 1000 7; do
  a=$(./build/first_test $arg); b=$(./build/rebuilt $arg)
  echo "arg='$arg' asli: $a | rebuilt: $b | $([ "$a" = "$b" ] && echo SAMA || echo BEDA)"
done
