#!/bin/bash
# ==============================================================================
# ⚡ flash_styrkort_macbot.sh: bygg och flasha MacTrac/MacBot-styrkortet
# ==============================================================================
# Körs PÅ Pi:n (mactrack-slu) med ST-LINK V2 kopplad till styrkortets SWD-pinnar
# och styrkortet matat via strömplinten:
#
#   cd ~/rise_sdvp && ./flash_styrkort_macbot.sh
#   ./flash_styrkort_macbot.sh --bara-bygg     bygg bara, rör ingen hårdvara
#
# Firmwaren byggs från den här grenen (mactrac): Gunnars MacTrac-firmware
# (make mactrac, IS_MACTRAC) plus våra rättningar, se MACTRAC_STATUS.md.
#
# Det bästa från båda:
# - Från Gunnar: själva MacTrac-firmwaren (hydraulik, ADDIO-ventiler, FTR2) och
#   openocd-flödet "program ... verify reset".
# - Från vårt flash_styrkort.sh: bygger från källkoden i stället för en gammal
#   förkompilerad fil, flashar ELF-filen så att styrkortets inställningar
#   (EEPROM) BEHÅLLS, kräver att du skriver FLASHA, kollar att ST-LINK finns.
#
# Gunnars upload_fw_pi används inte: den flashar en .bin (raderar inställningarna)
# och programmerar via Pi:ns GPIO-pinnar (oo_rpi4.cfg), vilket inte fungerar på Pi 5.
# ==============================================================================
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
exec "$DIR/flash_styrkort.sh" --maskin mactrac --fw-dir "$DIR/Embedded/RC_Controller" "$@"
