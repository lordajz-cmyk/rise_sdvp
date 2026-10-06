#!/bin/bash
# kor.sh — kör styrkortets Write (CMD_SET_MAIN_CONFIG) och reglerlooparna på datorn
# med AddressSanitizer, med MacBots inställningar från 2026-09-28 (skräp i
# sensorer/reglerloopar) och tre Write i RControlStations format.
#   bash kor.sh              testar firmware-koden i mappen ovanför
#   bash kor.sh MAPP         testar en annan RC_Controller-mapp (t.ex. äldre version)
set -e
cd "$(dirname "$0")"
SRC="$(cd "${1:-..}" && pwd)"
B=$(mktemp -d); trap 'rm -rf "$B"' EXIT
cp harness.c gen.py "$B"/; cp -r stub "$B"/
cd "$B"
python3 gen.py "$OLDPWD/kort_macbot_2026-09-28.hex"
s=$(grep -n 'case CMD_SET_MAIN_CONFIG' "$SRC/commands.c" | cut -d: -f1)
e=$(awk -v s="$s" 'NR>s && /^\t\t} break;/{print NR; exit}' "$SRC/commands.c")
sed -n "${s},${e}p" "$SRC/commands.c" | sed "s/conf_general_sanitize_main_config(/maybe_sanitize(/" > set_block.c
: > extra.c
NY=""
if grep -q "^void conf_general_sanitize_main_config" "$SRC/conf_general.c"; then
  awk '/^void conf_general_sanitize_main_config/{p=1} p{print} p&&/^}/{exit}' "$SRC/conf_general.c" >> extra.c
  NY="-DNY"
fi
awk '/^ACTUATOR\* motor_get_actuators_by_activity/{p=1} p{print} p&&/^}/{exit}' "$SRC/motor_control.c" >> extra.c
gcc -g -O0 -fsanitize=address,undefined -fno-sanitize-recover=all -fno-sanitize=shift -w $NY \
  -Istub -I"$SRC" harness.c "$SRC/state_control.c" "$SRC/sensor_control.c" "$SRC/buffer.c" -lm -o test
for p in rcs.bin gunnar.bin full.bin; do
  echo "== paket: $p"
  PAKET=$p ./test 2>&1 | grep -E "Start|Efter|Write|KLART|runtime error|ERROR" || true
done
