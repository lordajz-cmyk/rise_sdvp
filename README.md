# 🛰️ RISE SDVP - Optimerat & Helautomatiserat Styrsystem (lordajz-cmyk & gemini branch) 

Välkommen till den optimerade och helautomatiska releasen av **RISE SDVP** styrsystemet för autonoma fordon och jordbruksrobotar. Denna version är helt fristående, åtgärdar kritiska mjukvarubuggar och introducerar ett kraftfullt installationsramverk som gör driftsättning av nya maskiner till en lek.

---

## 🚀 Kritiska Förbättringar i denna Release (Bugfixes)

De manuella handpåläggningarna och instabiliteten från originalkoden är nu ett minne blott. Följande kritiska förbättringar har implementerats:

1. **RTK-stabilitet & Ingen Flaskhals:** RTCM-data skickas nu rå direkt till u-blox-mottagaren (vilket utnyttjar hårdvaruavkodning för MSM7-meddelanden för full GNSS-precision: GPS, Galileo, BeiDou, GLONASS).
2. **Eliminerat Buffertspill:** Ökat bufferten i `ublox_send` från 1024 till 2048 bytes samt lagt till en hård storlekskontroll som helt förhindrar minneskorruption.
3. **Synkroniserad Baudrate:** Styrkortet konfigurerar automatiskt u-blox UART2 till `115200` baud vid uppstart så att dataströmmarna matchar perfekt.
4. **Säkrad Autopilot-matematik:** 
   * Skydd mot division-med-noll i `autopilot.c` vid tidssynkronisering.
   * Skydd mot NaN-förstörande beräkningar i `utils.c` (`utils_closest_point_line`) vilket eliminerar autopilot-krascher nära ruttens brytpunkter.
5. **Robust Webbserver:** Flask-webbservern har uppdaterats till att binda mot `0.0.0.0` istället för en hårdkodad IP, vilket gör att den startar smärtfritt på alla typer av nätverk.

---

## 📦 Helautomatiserad Installation (Quick Start)

Du behöver inte längre installera bibliotek manuellt, konfigurera udev-regler eller bygga programmen steg för steg. Allt görs med våra smarta installationsskript!

### 🚜 1. Installera Allt (Raspberry Pi + Styrkort)
Om du är inkopplad mot din Raspberry Pi och vill göra en komplett installation (bygga klienten, konfigurera udev, ställa in Swepos RTK och ladda upp mjukvaran till styrkortet):
```bash
sudo ./install_allt.sh
```

---

## 🛠️ De Smarta Skripten (Skript-by-Skript)

Detta arkiv innehåller sex helt nya, interaktiva skript som gör grovjobbet åt dig:

### `install_pi.sh` (Raspberry Pi-konfigurering)
Konfigurerar robotens Raspberry Pi 4 från grunden.
* Installerar alla nödvändiga Linux-paket och Qt6-miljö (med graciös fallback till Qt5).
* Installerar dubbla udev-regler för USB-portar (stödjer både `/dev/car`/`/dev/ublox` och Benjamin Vedders referensnamn `/dev/vehicle`/`/dev/rtk` samtidigt).
* Bygger `Car_Client` och sätter upp en interaktiv Swepos RTK-konfiguration för automatisk uppkoppling vid start.
* Lägger till autostart i en bakgrunds-`screen` (`screen -r car` för att visa live).

### `install_dator.sh` (Utvecklingsdator / Laptop)
Installerar och bygger kontrollstationens gränssnitt (**RControlStation**) på din laptop.
* Detekterar automatiskt din Ubuntu-version (t.ex. 22.04 eller 24.04).
* Installerar rätt beroenden och kompilerar hela applikationen med CMake.

### `flash_styrkort.sh` (STM32-programmering & Skrivbordstest)
Kompilerar styrkortets mjukvara (Drängen eller Mactrac) och laddar upp den till styrkortet.
* Bygger mjukvaran och flashar styrkortet via en **ST-LINK V2** med OpenOCD.
* **Skrivbordstest:** Direkt efter lyckad flashning startas en interaktiv live-strömning av satellitdata (NMEA) direkt i terminalfönstret för direkt hårdvaruverifiering!

### `wireguard.sh` (Sömlös VPN-anslutning)
Ett intelligent installationsskript som sätter upp WireGuard VPN på din robot eller laptop.
* Genererar kryptonycklar automatiskt.
* Låter dig skriva in valfritt maskinnamn och valfri IP-adress i VPN-nätverket (`192.168.200.X`) med intelligenta standardval.
* Skapar den färdiga `/etc/wireguard/wg0.conf` och aktiverar automatisk start vid boot.

### `wireguard_admin.sh` (VPN-Server Administration)
Körs på servern (`192.168.200.1`) för att administrera nätverket.
* Initierar en helt ny VPN-server från grunden samt sätter upp IP-forwarding/NAT-routing automatiskt.
* Registrerar nya klienter (peers) live utan att störa befintliga anslutningar (`wg syncconf`).
* Listar, hanterar och raderar registrerade användare enkelt.

---

## 📚 Tillgängliga Guider (Dokumentation)

För djupgående detaljer och manualer, läs våra nyskapade guider i roten av detta projekt:
* 📜 **`installationsguide.md`:** ASCII-kopplingsschema för ST-LINK V2, SWD-pinouts och trådlös SSH-anslutning.
* 📜 **`Sakerhetsanalys.md`:** Djupgående analys av cybersäkerhet (VPN), realtidssäkerhet (failsafe-kontroller i `timeout.c`), kodintegritet och fysiska nödstopp.
* 📜 **`Wireguard_Guide.md`:** Komplett guide för hur du sätter upp en helt ny WireGuard-server hemma på en Ubuntu Server-maskin.
* 📜 **`fyi.md` & `GEMINI.md`:** Projektets långtidsminne, loggbok och färdplan.
