#!/bin/bash

# ==============================================================================
# ⚡ flash_styrkort.sh: Kompilera och programmera Carcontroller-kortet (STM32)
# ==============================================================================
# Detta skript fokuserar ENBART på att bygga styrsystemets firmware, flasha
# det till STM32-mikrodatorn via ST-LINK V2, samt köra ett live-diagnostiktest.
# ==============================================================================

GREEN='\e[32m'
RED='\e[31m'
YELLOW='\e[33m'
BLUE='\e[34m'
BOLD='\e[1m'
NC='\e[0m' # No Color

# Ta reda på den faktiska användaren och sök efter projektmappen
REAL_USER=${SUDO_USER:-$USER}
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo -e "${BLUE}${BOLD}======================================================================${NC}"
echo -e "${BLUE}${BOLD}   ⚡  FLASHNING AV CARCONTROLLER-KORTET (STM32-MIKRODATORN)   ⚡${NC}"
echo -e "${BLUE}${BOLD}======================================================================${NC}"

# Kontrollera om utvecklingsverktygen för ARM och OpenOCD finns installerade
if ! command -v arm-none-eabi-gcc &> /dev/null || ! command -v openocd &> /dev/null; then
  echo -e "${YELLOW}⚠️ Kompilatorer för STM32 eller OpenOCD saknas på din Pi.${NC}"
  echo -e "Installerar utvecklingsverktyg nu... (kräver sudo)"
  sudo apt update && sudo apt install -y build-essential openocd gcc-arm-none-eabi
fi

# ------------------------------------------------------------------------------
# 🛠️ STEG 1: Välj maskintyp och kompilera firmware
# ------------------------------------------------------------------------------
echo -e "\n${YELLOW}${BOLD}[Steg 1/3] Välj vilken maskin du vill bygga för:${NC}"
echo -e " 1) ${BOLD}Drängen${NC}"
echo -e " 2) ${BOLD}Mactrac${NC}"
read -p "Välj maskin (1 eller 2): " M_CHOICE

FW_NAME=""
if [ "$M_CHOICE" == "1" ]; then
  FW_NAME="drangen"
elif [ "$M_CHOICE" == "2" ]; then
  FW_NAME="mactrac"
else
  echo -e "${RED}Ogiltigt val! Avbryter.${NC}"
  exit 1
fi

FW_DIR="$DIR/rise_sdvp/Embedded/RC_Controller"
echo -e "\nKompilerar firmware för ${BOLD}$FW_NAME${NC}..."

# Bygg källkoden (körs som den vanliga användaren för att undvika root-ägda filer)
if [ "$EUID" -eq 0 ]; then
  sudo -u "$REAL_USER" bash -c "cd '$FW_DIR' && make clean && make -j\$(nproc) $FW_NAME"
else
  cd "$FW_DIR" && make clean && make -j$(nproc) $FW_NAME
fi

if [ $? -eq 0 ]; then
  echo -e "${GREEN}✅ Styrkortets firmware ($FW_NAME) kompilerad framgångsrikt!${NC}"
else
  echo -e "${RED}❌ Kompileringsfel för styrkortets firmware. Kontrollera loggarna ovan.${NC}"
  exit 1
fi

# ------------------------------------------------------------------------------
# ⚡ STEG 2: Flasha styrkortet med ST-LINK V2
# ------------------------------------------------------------------------------
echo -e "\n${YELLOW}${BOLD}[Steg 2/3] Förbered programmering via ST-LINK V2...${NC}"
echo -e "${BLUE}${BOLD}Instruktioner för hårdvarukoppling:${NC}"
echo -e "1. Anslut din ${BOLD}ST-LINK V2 USB-sticka${NC} till din Raspberry Pi (eller PC)."
echo -e "2. Koppla SWD-kablarna till Carcontroller-kortets SWD-pinnar:"
echo -e "   - ${BOLD}SWCLK${NC} -> SWCLK"
echo -e "   - ${BOLD}SWDIO${NC} -> SWDIO"
echo -e "   - ${BOLD}GND${NC}   -> GND"
echo -e "   - ${BOLD}3.3V${NC}  -> 3.3V (Strömsätter styrkortet direkt från ST-LINK!)"
echo ""

read -p "Är hårdvaran inkopplad och redo? Tryck på [ENTER] för att flasha!" ReadyTrigger

# Kontrollera om ST-LINK syns på USB-bussen
lsusb | grep -i "st-link" &> /dev/null
if [ $? -ne 0 ]; then
  echo -e "${YELLOW}⚠️ Kunde inte hitta någon ST-LINK V2 ansluten via USB. Kontrollera kontakten.${NC}"
  read -p "Vill du försöka flasha ändå? Tryck [ENTER] för att köra OpenOCD." RetryTrigger
fi

echo -e "\n${BOLD}Startar flashning via OpenOCD...${NC}"

# 
if [ "$EUID" -ne 0 ]; then
  sudo openocd -f board/stm32f4discovery.cfg -c "reset_config trst_only combined" -c "program $FW_DIR/precompiled/fw_${FW_NAME}.bin verify reset exit 0x08000000"
else
  openocd -f board/stm32f4discovery.cfg -c "reset_config trst_only combined" -c "program $FW_DIR/precompiled/fw_${FW_NAME}.bin verify reset exit 0x08000000"
fi

if [ $? -eq 0 ]; then
  echo -e "\n${GREEN}✅ Flashningen slutförd och verifierad framgångsrikt!${NC}"
else
  echo -e "\n${RED}❌ Flashningen misslyckades!${NC}"
  echo -e "Koppla ur och sätt i ST-LINK:en igen, kontrollera att kablarna"
  echo -e "sitter stabilt på styrkortets SWD-pinnar och testa igen."
  exit 1
fi

# ------------------------------------------------------------------------------
# 🔬 STEG 3: Interaktivt skrivbordstest (Live-diagnostik)
# ------------------------------------------------------------------------------
echo -e "\n${YELLOW}${BOLD}[Steg 3/3] Vill du köra ett direkt skrivbordstest via USB nu? (y/n)${NC}"
read -p "Köra diagnostiktest? (y/n): " RUN_DIAG

if [[ "$RUN_DIAG" =~ ^[Yy]$ ]] || [[ -z "$RUN_DIAG" ]]; then
  echo -e "\n${BLUE}${BOLD}Instruktioner för skrivbordstest:${NC}"
  echo -e "1. Behåll din ${BOLD}ST-LINK V2${NC} inkopplad (så att kortet får ström)."
  echo -e "2. Anslut ${BOLD}TVÅ Micro-USB-kablar${NC} mellan din dator och de två portarna på Carcontroller-kortet."
  echo ""
  read -p "Tryck på [ENTER] när kablarna är anslutna så startar vi testet!" ConnectTrigger

  echo -e "\n${BLUE}Söker efter serieportar från styrkortet...${NC}"
  sleep 2 # Vänta lite så att USB-portarna hinner registreras av Linux

  PORT0="/dev/ttyACM0"
  PORT1="/dev/ttyACM1"

  # Kontrollera om USB-portarna har skapats i systemet
  if [ -c "$PORT0" ] && [ -c "$PORT1" ]; then
    echo -e "${GREEN}✅ Framgång! Hittade båda USB-portarna ($PORT0 och $PORT1) på datorn!${NC}"
    echo -e "Detta bevisar att båda delarna av styrkortet är igång och pratar med datorn."
  elif [ -c "$PORT0" ] || [ -c "$PORT1" ]; then
    echo -e "${YELLOW}⚠️ Delvis framgång! Hittade bara en av portarna. Kontrollera att BÅDA USB-sladdarna är i.${NC}"
  else
    echo -e "${RED}❌ Testet misslyckades! Kunde inte hitta några USB-portar från styrkortet.${NC}"
    echo -e "Kontrollera att USB-sladdarna sitter i ordentligt och att kortet har ström."
  fi

  # Testa om vi kan detektera GPS-dataströmmen live på någon av portarna
  GPS_PORT=""
  for port in "$PORT0" "$PORT1"; do
    if [ -c "$port" ]; then
      # Läs 5 rader under max 1.5 sekunder och sök efter NMEA-kod ($G)
      if timeout 1.5 head -n 5 "$port" 2>/dev/null | grep -q "\$G"; then
        GPS_PORT="$port"
        break
      fi
    fi
  done

  if [ -n "$GPS_PORT" ]; then
    echo -e "\n${GREEN}✅ DETEKTERADE GPS-DATASTRÖM LIVE PÅ PORT: $GPS_PORT!${NC}"
    echo -e "Här är råa, nytagna GPS-rader från ditt inbyggda u-blox chip:"
    echo -e "${BLUE}----------------------------------------------------------------------${NC}"
    # Visa rader som innehåller GPS GGA eller liknande
    timeout 1 head -n 3 "$GPS_PORT" | grep "\$G"
    echo -e "${BLUE}----------------------------------------------------------------------${NC}"
    echo -e "Detta bekräftar att u-blox-mottagaren fungerar utmärkt och skickar data!"
  else
    echo -e "\n${YELLOW}⚠️ Kunde inte automatiskt läsa någon GPS-dataström på USB-portarna.${NC}"
    echo -e "Detta beror oftast på att u-blox-enheten är tyst eller att behörigheterna spökar."
  fi
fi

echo -e "\n${GREEN}${BOLD}======================================================================${NC}"
echo -e "${GREEN}${BOLD}🎉 ALLT KLART! STYRKORTET ÄR NYFLASHAT OCH BEVISAT FUNGERANDE! 🎉${NC}"
echo -e "${GREEN}${BOLD}======================================================================${NC}"
echo -e "Din styrsystem-firmware (${BOLD}$FW_NAME${NC}) har laddats upp till Carcontroller-kortet."
echo -e "Kortet är nu redo att monteras på roboten och anslutas till din Raspberry Pi!"
echo -e "======================================================================"
