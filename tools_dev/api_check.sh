#!/bin/bash
# api_check.sh - catches "header declares it, source lost it" corruption.
# For each (header, source) pair, every non-static function declaration in the
# header must appear as a definition in the source file.
cd "$(dirname "$0")/.." || exit 1
FAIL=0
check_pair() {
    local hdr="$1" src="$2"
    # extract function names: lines like ... Type Name(args);
    for fn in $(grep -oE '[A-Za-z_][A-Za-z0-9_]*\(' "$hdr" | sed 's/($//; s/(//' | sort -u); do
        case "$fn" in
            if|while|for|switch|return|sizeof|defined) continue ;;
        esac
        if ! grep -qE "(${fn})[[:space:]]*\(" "$src"; then
            echo "MISSING DEFINITION: $fn (declared in $hdr, absent from $src)"
            FAIL=1
        fi
    done
}
check_pair client/src/world.h client/src/world.c
check_pair client/src/player.h client/src/player.c
check_pair client/src/hunter.h client/src/hunter.c
check_pair client/src/soundfx.h client/src/soundfx.c
check_pair client/src/starfield.h client/src/starfield.c
check_pair client/src/asteroid.h client/src/asteroid.c
if [ "$FAIL" = "0" ]; then echo "API CHECK OK"; else echo "API CHECK FAILED"; exit 1; fi
