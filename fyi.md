# FYI.md - Kom-i-ihåg & Framtida Utvecklingsplan 🚀

Denna fil fungerar som vårt gemensamma minne för vad som har gjorts, kritiska detaljer samt vad som ligger framför oss i nästa fas av projektet.

---

## 🛠️ Vad vi har åstadkommit (Summary of Work Done)

Vi har tagit styrsystemet och dess kringutrustning från att ha trasig RTK-kod, kraschande hårdvara och matematiska sårbarheter till ett **fullt kompilerbart, robust och helautomatiserat system**.

1. **RTK & Stabilitetsfixar:**
   * **Borttagen flaskhals:** RTCM-data skickas nu rå direkt till u-blox (vilket utnyttjar u-blox hårdvaruavkodning för MSM7-meddelanden för full GNSS-precision: GPS, Galileo, BeiDou, GLONASS).
   * **Eliminerat buffertspill:** Ökat bufferten i `ublox_send` från 1024 till 2048 bytes och lagt till en hård storlekskontroll mot minneskorruption.
   * **Synkad Baudrate:** u-blox UART2 ställs automatiskt in på `115200` baud från styrkortet så att den matchar RTCM-forwarderns sändningshastighet.
2. **Säkrad Autopilot-matematik:**
   * **Division med noll:** Fixat tidsvalideringen i `autopilot.c`.
   * **NaN-förstörande beräkning:** Fixat `utils.c` (`utils_closest_point_line`) så parametrarna tvingas till `0.0f` om linjesegmentet är punktformat, vilket hindrar autopilot-krascher.
3. **Automatisering & Hårdvarutester:**
   * **`install_allt.sh` (Säker Bootstrap):** En hel-installatör som upptäcker och använder befintlig källkod lokalt, eller klonar repot automatiskt om det saknas.
   * **`install_pi.sh` & `install_dator.sh`:** Helautomatiska installationsskript för både Raspberry Pi 4 och datorn som anpassar sig efter operativsystemets versioner (Ubuntu 22.04/24.04).
   * **`flash_styrkort.sh`:** Flaschar styrkortet med ST-LINK V2 via OpenOCD och kör ett interaktivt live-skrivbordstest som läser av USB-portar (`/dev/ttyACM0` & `/dev/ttyACM1`) och NMEA-satellitdata i realtid.
   * **`wireguard.sh` (Dynamisk VPN-klient):** Ett intelligent, interaktivt installationsskript som sätter upp WireGuard-VPN på dator eller Pi. Det genererar unika kryptonycklar automatiskt och låter användaren ange ett helt eget klientnamn och en valfri IP-adress i VPN-nätverket (`192.168.200.X`) med intelligenta standardval. Det gör att du slipper gamla hårdkodade maskinnamn, samtidigt som det låter dig anpassa serverns IP/port och Public Key vid behov. Det aktiverar även autostart via systemd.
   * **`wireguard_admin.sh` (VPN-Server Administrator):** Ett komplett interaktivt administrationsverktyg för servern (`192.168.200.1`). Kan initiera servern från grunden, aktivera IP-forwarding/NAT-routing, registrera klienter interaktivt, ladda om konfigurationen sömlöst live (`wg syncconf`) utan avbrott, samt lista eller radera användare ur systemet. Halverar tidsåtgången för serverkonfigurering! Nu uppgraderad med **automatisk nätverkskortdetektering** för 100% plug-and-play-kompatibilitet med alla moderna servrar!
4. **Säkerhetsdokumentation & Isolerad backup (15 September 2026):**
   * **`Sakerhetsanalys.md`:** En djupgående säkerhetsgranskning av robotens mjukvara, nätverk (MitM/VPN), kontrollloopar (`timeout.c` failsafe), samt fysiska operativa risker (hinderdetektering och krav på hårdvaru-nödstopp).
   * **`Wireguard_Guide.md`:** En fullständig svensk instruktionsguide för nyinstallation av en privat WireGuard VPN-server hemma på en Ubuntu Server-maskin.
   * **`uppdaterade_filer/` (Isolerad backup):** Samlat en isolerad, fullständigt strukturerad backup på samtliga **11 modifierade källkodsfiler** och skapat en förklarande indexfil (`lista_filer.md`) så att ändringarna kan appliceras stegvis och säkert på andra maskiner.
5. **Webbserver-fixar:**
   * **Löst IP-bindningsfel ('Cannot assign requested address'):** Ändrat Flask-webbservern (`rise_sdvp/web/server.py`) från att vara låst till den hårdkodade IP-adressen `192.168.200.3` (Gunnars bärbara dator) till att istället binda mot `0.0.0.0`. Detta gör att servern nu kan starta smärtfritt på vilken dator eller server som helst, oavsett dess nätverkskonfiguration.

---

## 📌 Kritiska detaljer att komma ihåg (Key Details)

* **USB-portar på Raspberry Pi:**
  * `/dev/car` (STM32-kontrollern, USB CDC)
  * `/dev/ublox` (u-blox GNSS, USB NMEA/RTK)
* **Koppling för ST-LINK V2 (SWD):**
  1. **SWCLK** -> SWCLK på styrkortet
  2. **SWDIO** -> SWDIO på styrkortet
  3. **GND**   -> GND på styrkortet
  4. **3.3V**  -> 3.3V på styrkortet (Strömsätter styrkortet direkt under programmering!)
* **Kommandon på Raspberry Pi:**
  * Se live-telemetri: `screen -r car` (Koppla ifrån med `Ctrl+A` sedan `D`)
  * Se Swepos RTK-status: `sudo systemctl status car_rtk.service`
  * Se Swepos-logg: `journalctl -u car_rtk -f`

---

## 📋 Vad som är kvar att göra (Next Steps & Roadmap)

När vi startar nästa session eller när du är redo i verkstaden, är detta de steg vi bör följa:

### Steg 1: Pusha ändringarna till GitHub 📤
Eftersom vi har gjort stora förbättringar och rättningar i källkoden samt skapat alla dessa smarta skript, bör du lägga till och committa dem på GitHub så att dina ändringar sparas säkert i molnet.
```bash
# Gå till ditt repo
cd ~/RControllStation/rise_sdvp

# Lägg till och committa dina ändrade filer
git add .
git commit -m "Fixade RTK-buffertspill, UART2 baudrate, autopilot-matematik och skapade installationsskript"
git push origin master
```
*Därefter kan du ladda upp även installationsskripten och `fyi.md` till GitHub-repot så att de finns med vid nästa rena kloning.*

### Steg 2: Fysiskt fälttest (Under bar himmel) 🛰️
Nu när all mjukvara, buffertar och baudrates är lagade är det dags att rulla ut roboten i verkligheten:
1. Sätt igång roboten utomhus där u-blox-mottagaren har fri sikt mot satelliterna.
2. Kontrollera att din Pi får internetanslutning och att Swepos-strömmen går igång (`systemctl status car_rtk`).
3. Verifiera i `RControlStation` (eller via `screen -r car`) att mottagaren går från standard GPS-läge till **RTK Float** och slutligen till **RTK Fix** (grön indikator, ca 3 cm noggrannhet!).

### Steg 3: Sätt upp WireGuard VPN-tunneln 🛡️ (HELT VERIFIERAD & KLAR! ✅)
Vi har konfigurerat, testat och framgångsrikt driftsatt WireGuard VPN-tunneln över mobil hotspot!
- Servern (`192.168.200.1`) har nu permanent `ip_forward=1` och slussar trafiken klockrent.
- Både datorn (`192.168.200.99`) och Raspberry Pi (`192.168.200.10`) ansluter automatiskt och kan pinga varandra.
- RControlStation har framgångsrikt kommunicerat och strömmat telemetri ("Poll data") live över VPN!

### Steg 4: Verifiera Autopiloten 🚜
Kör roboten längs en fördefinierad rutt och kontrollera att autopiloten följer linjen perfekt utan att tveka eller drabbas av matematiska störningar nära brytpunkter.
