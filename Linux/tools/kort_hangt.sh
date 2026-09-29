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

{
  echo "=== $(date) — styrkortet svarar inte ==="
  echo "--- ELF: $ELF"
  echo "--- Processor, USB och trådar (två avläsningar med 1 s mellanrum)"
  (cd /tmp && sudo python3 "$(dirname "$0")/kort_tradar.py" "$ELF")
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
