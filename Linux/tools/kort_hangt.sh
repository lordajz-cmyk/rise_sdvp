#!/bin/bash
# kort_hangt.sh — körs på Pi:n när styrkortet slutat svara (Car_Client: "Write timeout").
# Samlar in data via ST-Link INNAN kortet startas om, och startar sedan om det:
#   - lever processorn? (PC och systemklockan läses två gånger med 1 s mellanrum)
#   - USB-kontrollerns status (OTG_FS: suspend, avbrott)
#   - avbrottsmasker (PRIMASK/BASEPRI) och felregister
#   - Car_Clients senaste utskrift
# Allt sparas i ~/kort_hangt_<tid>.txt.
#
#   bash ~/rise_sdvp/Linux/tools/kort_hangt.sh           samla in och starta om kortet
#   bash ~/rise_sdvp/Linux/tools/kort_hangt.sh --bara-las   samla in, starta inte om
set -u
UT="$HOME/kort_hangt_$(date +%Y-%m-%d_%H-%M-%S).txt"
ELF="$HOME/rise_sdvp/Embedded/RC_Controller/build/fw_mactrac.elf"
OCD=(sudo openocd -f board/stm32f4discovery.cfg -c "reset_config trst_only combined")

CH=""
if [ -f "$ELF" ] && command -v arm-none-eabi-nm >/dev/null; then
  CH=$(arm-none-eabi-nm "$ELF" | awk '$3=="ch"{print "0x"$1}')
fi

las() {
  local cmd="init; halt 2000"
  for r in pc lr sp primask basepri; do cmd="$cmd; echo \"$r [capture \\\"reg $r\\\"]\""; done
  cmd="$cmd; echo [capture \"mdw 0xE000ED28 3\"]"               # CFSR, HFSR, DFSR
  cmd="$cmd; echo [capture \"mdw 0x50000014 1\"]"               # OTG_FS GINTSTS
  cmd="$cmd; echo [capture \"mdw 0x50000808 1\"]"               # OTG_FS DSTS (bit0 = suspend)
  cmd="$cmd; echo [capture \"mdw 0x50000804 1\"]"               # OTG_FS DCTL
  [ -n "$CH" ] && cmd="$cmd; echo [capture \"mdw $CH 48\"]"    # ChibiOS: ch (systemklocka m.m.)
  cmd="$cmd; resume; exit"
  "${OCD[@]}" -c "$cmd" 2>&1 | grep -v -E "^(Info|Open On|Licensed|For bug|\s+http|srst_only|trst_only)"
}

{
  echo "=== $(date) — styrkortet svarar inte ==="
  echo "--- ELF: $ELF  (ch = ${CH:-okänd})"
  echo "--- Avläsning 1"; las
  sleep 1
  echo "--- Avläsning 2 (1 s senare: har PC och klockan i 'ch' ändrats lever processorn)"; las
  echo "--- USB på Pi:n"; ls -la /dev/vehicle /dev/ublox 2>&1; sudo dmesg -T | tail -15
  echo "--- Car_Client (senaste skärmen)"
  screen -S car -X hardcopy -h /tmp/car_hangt.txt 2>/dev/null; sleep 1
  grep -v "^\s*$" /tmp/car_hangt.txt 2>/dev/null | tail -60
} > "$UT" 2>&1
echo "Sparat: $UT"

if [ "${1:-}" != "--bara-las" ]; then
  echo "Startar om styrkortet och Car_Client..."
  "${OCD[@]}" -c "init; reset run; exit" >/dev/null 2>&1
  sleep 12
  sudo systemctl restart car_client.service
  sleep 40
  echo "Firmware: $(python3 "$(dirname "$0")/fw_version.py" 2>/dev/null || echo 'svarar inte')"
fi
