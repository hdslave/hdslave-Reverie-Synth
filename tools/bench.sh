#!/bin/sh
# Instructions a 128-frame block takes, counted by callgrind in Melodee's profiler image (its build image plus valgrind),
# for each bench mode. A block at 48 kHz is 2,667 us; on the MPC's 1.8 GHz Cortex-A55 that is about 4.8 million cycles,
# so the share printed assumes about one instruction a cycle (the A55 is in-order).
#   tools/bench.sh [SECONDS] [MODES...]
set -eu
cd "$(dirname "$0")/.."
CONTEXT=${DOCKER_CONTEXT:-colima-melodee}
SECONDS_RUN=${1:-4}
shift 2>/dev/null || true
MODES=${*:-worst pad typical silent paint motion}
docker --context "$CONTEXT" run --rm --platform linux/arm64 -v "$PWD:/work" -w /work melodee-profiler sh -c "
    make -s build/bench >/dev/null
    blocks=\$(awk -v s=$SECONDS_RUN 'BEGIN { printf \"%d\", s * 48000 / 128 }')
    count() { valgrind --tool=callgrind --callgrind-out-file=/dev/null build/bench \$1 \$2 2>&1 | grep -o 'refs: *[0-9,]*' | tr -d 'refs: ,'; }
    for mode in $MODES; do
        if [ \$mode = paint ] || [ \$mode = motion ]; then
            # The page's own cost: a paint run less a pad run of the same length, over its frames (25 a second).
            p=\$(count \$mode $SECONDS_RUN); t=\$(count pad $SECONDS_RUN)
            awk -v p=\$p -v t=\$t -v n=\$blocks -v m=\$mode 'BEGIN { frames = int ((n + 14) / 15); printf \"%-8s %9.0f instructions a painted frame (25 frames a second)\\n\", m, (p - t) / frames }'
            continue
        fi
        refs=\$(count \$mode $SECONDS_RUN); base=\$(count \$mode 0)
        awk -v r=\$refs -v b=\$base -v n=\$blocks -v m=\$mode 'BEGIN { per = (r - b) / n; printf \"%-8s %9.0f instructions a block, %5.2f %% of a block on one A55 core at 1 IPC\\n\", m, per, per / 4800000 * 100 }'
    done"
