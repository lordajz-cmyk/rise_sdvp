#!/bin/bash

# ==============================================================================
# 🖥️ install_dator.sh: Konfigurera din styrdator/laptop (Linux) för RControlStation
# ==============================================================================
# Detta skript installerar alla paket, tillägg och bibliotek (inklusive Qt6, SDL2,
# GDAL och Eigen) på din laptop/dator, och bygger sedan kontrollprogrammet RControlStation.
# Skriptet känner automatiskt av din Ubuntu-version och väljer rätt paketnamn!
# Det har även avancerad felrapportering för saknade paket.
# ==============================================================================

GREEN='\e[32m'
RED='\e[31m'
YELLOW='\e[33m'
BLUE='\e[34m'
BOLD='\e[1m'
NC='\e[0m' # No Color

# Säkerställ att skriptet körs som root (sudo) för att kunna installera paket på laptopen
if [ "$EUID" -ne 0 ]; then
  echo -e "${RED}${BOLD}Fel:${NC} Detta skript måste köras med sudo! Kör: ${BOLD}sudo ./install_dator.sh${NC}"
  exit 1
fi

REAL_USER=${SUDO_USER:-$USER}
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo -e "${BLUE}${BOLD}======================================================================${NC}"
echo -e "${BLUE}${BOLD}  🖥️  INSTALLATION AV UTCECKLINGSMILJÖ PÅ LAPTOP/DATOR (LINUX)  🖥️${NC}"
echo -e "${BLUE}${BOLD}======================================================================${NC}"
echo -e "Detta skript sätter upp alla program och tillägg som behövs på din dator"
echo -e "för att köra och bygga det grafiska kontrollgränssnittet RControlStation."
echo -e "Faktisk användare: ${BOLD}$REAL_USER${NC}"
echo -e "Källkodsmapp:     ${BOLD}$DIR${NC}\n"

# ------------------------------------------------------------------------------
# 📦 STEG 1: Installera alla bibliotek och paket runt systemet (Qt6, GDAL, SDL2, etc.)
# ------------------------------------------------------------------------------
echo -e "${YELLOW}${BOLD}[Steg 1/2] Analyserar ditt system och väljer rätt paketnamn...${NC}"

# Paketnamn varierar mellan Ubuntu 22.04 (Jammy) och nyare (Noble/Resolute)
SERIALPORT_PKG="qt6-serialport-dev"
CHARTS_PKG="qt6-charts-dev"

UBUNTU_CODENAME=$(lsb_release -sc 2>/dev/null)
if [ "$UBUNTU_CODENAME" = "jammy" ]; then
  echo -e "Detekterade Ubuntu 22.04 (Jammy). Anpassar paketnamn för äldre system..."
  SERIALPORT_PKG="libqt6serialport6-dev"
  CHARTS_PKG="libqt6charts6-dev"
else
  echo -e "Detekterade ett nyare system ($UBUNTU_CODENAME). Använder moderna standardpaketnamn..."
fi

echo -e "\n${YELLOW}${BOLD}Installerar systempaket och utvecklingstillägg...${NC}"
echo -e "Detta inkluderar Qt6, GDAL (för kartor), SDL2 (för spelkontroll/joystick) samt matematiska bibliotek."

# Säkerställ att universe-arkivet är aktiverat (krävs för GDAL och vissa Qt6-paket i Ubuntu).
# add-apt-repository finns i software-properties-common, som saknas på minimala installationer.
command -v add-apt-repository >/dev/null 2>&1 || { apt update; apt install -y software-properties-common; }
add-apt-repository -y universe

apt update

# Definiera listan på alla paket som behövs
PACKAGES=(
    build-essential
    cmake
    git
    qt6-base-dev
    qt6-base-private-dev
    "$SERIALPORT_PKG"
    "$CHARTS_PKG"
    libsdl2-dev
    libgdal-dev
    libeigen3-dev
    libsqlite3-dev
    libssl-dev
    libcurl4-openssl-dev
    zlib1g-dev
    libpng-dev
    libjpeg-dev
    wireguard
    wireguard-tools
    resolvconf
)

# Försök installera alla paket på en gång först (snabbaste vägen)
echo -e "Försöker installera alla paket på en gång..."
if apt install -y "${PACKAGES[@]}" &>/dev/null; then
  echo -e "${GREEN}✅ Alla bibliotek och tillägg installerades felfritt!${NC}\n"
else
  echo -e "${YELLOW}⚠️ Något paket gick inte att installera på en gång. Testar att installera dem individuellt för att hitta felet...${NC}"
  FAILED_PKGS=()
  
  for pkg in "${PACKAGES[@]}"; do
    # Kontrollera om paketet redan är installerat
    if dpkg -s "$pkg" &>/dev/null; then
      continue
    fi
    
    # Försök installera paketet individuellt
    if apt install -y "$pkg" &>/dev/null; then
      echo -e "  [${GREEN}OK${NC}] Installerad: $pkg"
    else
      echo -e "  [${RED}FEL${NC}] Kunde inte installera: $pkg"
      FAILED_PKGS+=("$pkg")
    fi
  done

  # Slutrapport för paketen
  if [ ${#FAILED_PKGS[@]} -eq 0 ]; then
    echo -e "${GREEN}✅ Alla tillgängliga paket installerades framgångsrikt!${NC}\n"
  else
    echo -e "\n${YELLOW}${BOLD}⚠️  INSTALLATIONS-SAMMANFATTNING AV PAKET:${NC}"
    echo -e "${RED}Följande paket kunde inte installeras på din dator:${NC}"
    for fpkg in "${FAILED_PKGS[@]}"; do
      echo -e "  - ${BOLD}$fpkg${NC}"
    done
    echo -e "\n${YELLOW}Tips: Du kan behöva installera dessa saknade paket manuellt eller kontrollera din internetanslutning.${NC}"
    echo -e "Byggprocessen kommer ändå att fortsätta, men kan misslyckas om ett kritiskt paket saknas.\n"
  fi
fi

# ------------------------------------------------------------------------------
# 🛠️ STEG 2: Kompilera RControlStation
# ------------------------------------------------------------------------------
echo -e "${YELLOW}${BOLD}[Steg 2/2] Vill du bygga och kompilera RControlStation just nu?${NC}"
read -p "Kompilera RControlStation? (y/n): " RUN_BUILD

if [[ "$RUN_BUILD" =~ ^[Yy]$ ]] || [[ -z "$RUN_BUILD" ]]; then
  # Sök dynamiskt efter RControlStation-mappen
  STATION_DIR=""
  if [ -d "$DIR/Linux/RControlStation" ]; then
    STATION_DIR="$DIR/Linux/RControlStation"
  elif [ -d "$DIR/rise_sdvp/Linux/RControlStation" ]; then
    STATION_DIR="$DIR/rise_sdvp/Linux/RControlStation"
  fi
  
  if [ -n "$STATION_DIR" ] && [ -d "$STATION_DIR" ]; then
    echo -e "\nGår till källkodsmappen: $STATION_DIR"
    chmod +x "$STATION_DIR/build_cmake_linux.sh"
    
    # RControlStation läser i första hand data.db bredvid programfilen (build/.../lin/data.db).
    # Där ligger bl.a. dosa-bindningarna, så spara den innan byggmappen rensas.
    BIN_DIR="$STATION_DIR/build/cmake_linux/build/lin"
    DB_BACKUP=""
    if [ -f "$BIN_DIR/data.db" ]; then
      DB_BACKUP=$(mktemp /tmp/rcontrolstation_data.XXXXXX.db)
      cp "$BIN_DIR/data.db" "$DB_BACKUP"
      echo -e "Sparade databasen (dosa-bindningar m.m.) inför ombygget."
    fi

    # Självläkning: Ta bort gammal CMake-cache om den finns (vanligt vid kopiering mellan olika användare/maskiner)
    if [ -d "$STATION_DIR/build" ]; then
      echo -e "${YELLOW}⚠️ Upptäckte en gammal byggmapp (beror ofta på kopiering mellan datorer).${NC}"
      echo -e "Rensar gamla cachefiler för ett rent och felfritt bygge...${NC}"
      rm -rf "$STATION_DIR/build"
    fi
    
    # Kör kompileringen som den vanliga användaren (ej root) för att slippa root-ägda byggfiler
    echo -e "Kompilerar programmet med CMake..."
    sudo -u "$REAL_USER" bash -c "cd '$STATION_DIR' && ./build_cmake_linux.sh release"
    
    if [ $? -eq 0 ]; then
      # Lägg databasen bredvid programfilen. Utan den skapar RControlStation en TOM
      # databas (tomma dosa-listor m.m.) om programmet startas utanför projektmappen.
      if [ -n "$DB_BACKUP" ]; then
        cp "$DB_BACKUP" "$BIN_DIR/data.db" && rm -f "$DB_BACKUP"
        echo -e "${GREEN}✅ Databasen återställd bredvid programfilen.${NC}"
      else
        for CAND in "$DIR/data.db" "$DIR/rise_sdvp/data.db" "$DIR/web/data.db" "$DIR/rise_sdvp/web/data.db"; do
          if [ -f "$CAND" ]; then
            cp "$CAND" "$BIN_DIR/data.db"
            echo -e "${GREEN}✅ Databas kopierad bredvid programfilen från: $CAND${NC}"
            break
          fi
        done
        if [ ! -f "$BIN_DIR/data.db" ]; then
          echo -e "Ingen tidigare databas hittades. Det är normalt vid en ny installation:"
          echo -e "RControlStation skapar en ny vid första starten, med dosan förinställd."
        fi
      fi
      chown "$REAL_USER:$REAL_USER" "$BIN_DIR/data.db" 2>/dev/null

      # Skapa en global symlänk så att programmet kan startas var som helst ifrån
      ln -sf "$STATION_DIR/build/cmake_linux/build/lin/RControlStation" /usr/local/bin/RControlStation
      
      echo -e "\n${GREEN}${BOLD}======================================================================${NC}"
      echo -e "${GREEN}${BOLD}🎉 GRATULERAR! RCONTROLSTATION ÄR NU BYGGD OCH KLAR! 🎉${NC}"
      echo -e "${GREEN}${BOLD}======================================================================${NC}"
      echo -e "RControlStation-applikationen har kompilerats utan ett enda fel."
      echo -e "En global genväg har skapats! Du kan starta programmet var som helst ifrån genom att bara köra: ${BOLD}RControlStation${NC}"
      echo -e "Programmet finns sparat på följande sökväg:"
      echo -e "  ${BOLD}$STATION_DIR/build/cmake_linux/build/lin/RControlStation${NC}"
      echo -e ""
      echo -e "💡 ${BOLD}Starta programmet:${NC} skriv ${BOLD}RControlStation${NC} i valfri terminal."
      echo -e "   Första gången skapas en ny databas med dosan förinställd:"
      echo -e "   vänster spak upp/ner = Speed Control, höger spak åt sidan = Steering Control."
      echo -e "======================================================================"
    else
      echo -e "${RED}❌ Kompileringsfel uppstod under bygget av RControlStation. Se loggarna ovan.${NC}"
      exit 1
    fi
  else
    echo -e "${RED}❌ Kunde inte hitta mappen för RControlStation på: $STATION_DIR${NC}"
    exit 1
  fi
else
  echo -e "\n${BLUE}Bygget hoppades över.${NC}"
  echo -e "Du kan kompilera programmet manuellt när som helst genom att gå till mappen:"
  echo -e "  ${BOLD}rise_sdvp/Linux/RControlStation${NC}"
  echo -e "och köra:"
  echo -e "  ${BOLD}./build_cmake_linux.sh release${NC}"
fi
