#!/usr/bin/env bash
# Builds wad.c for the PC and checks it makes the same bytes as retarget.py, for both templates.
set -euo pipefail
cd "$(dirname "$0")"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cat > "$work/main.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include "wad.h"

int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    unsigned char *wad = malloc(size);
    fread(wad, 1, size, f);
    fclose(f);
    const char *error = wad_retarget(wad, size, argv[2], argv[3]);
    if (error) {
        fprintf(stderr, "%s\n", error);
        return 1;
    }
    f = fopen(argv[4], "wb");
    fwrite(wad, 1, size, f);
    fclose(f);
    return 0;
}
EOF
cc -O2 -Wall -I../source "$work/main.c" ../source/wad.c host_crypto.c -lcrypto -o "$work/retarget"

python=../../build/.venv/bin/python
for template in ../data/wii.wad ../data/vwii.wad; do
    for target in "/apps/wiiflow/boot.dol WFLW" "/apps/a/boot.elf E0A1"; do
        set -- $target
        "$work/retarget" "$template" "$1" "$2" "$work/c.wad"
        "$python" -I ../../retarget.py "$template" "$1" "$2" "$work/py.wad" > /dev/null
        cmp "$work/c.wad" "$work/py.wad"
        echo "ok    $(basename "$template") -> $1 as $2"
    done
done
