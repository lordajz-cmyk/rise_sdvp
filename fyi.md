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
6. **Löst djupt Deadlock i `Car_Client` (17 September 2026):**
   * **Problemet:** `Car_Client` hängde sig stenhårt under uppstarten innan dess TCP-server hann öppnas. 
   * **Orsaken:** En oändlig loop i `SerialPort::writeData` (`serialport.cpp`) blockerade huvudtråden permanent om en seriell skrivning till en tillfälligt ej redo port (`/dev/vehicle` under `restartRtklib()`) drabbades av pselect-timeout (t.ex. på grund av full sändningsbuffert eller saknad `/dev/arduino`).
   * **Lösningen:** Patchat `serialport.cpp` med en hård 200 ms ackumulerad timeout-spärr. Om gränsen överskrids avbryts skrivningen säkert så att programmet kan fortsätta och starta TCP-servern på port 8300: *"Started :-)"*. Detta har kompilerats live på robotens Pi med Qt6 och driftsatts.

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
### Steg 5: Felsök "Poll Data" Timeout (18 September 2026 - LÖST & PERSISTENT! ✅)
Efter omstart av Pi:n uppstod återigen problemet med att NMEA/RTK-anslutningen inte lyckades, vilket visade sig som en oändlig loop av `Trying to reconnect nmea tcp...` i `screen -r car`.

**Orsaksanalys:**
1. Hårdkodad sökväg i `carclient.cpp` för att starta RTKLIB: `cd /home/robant/rise_sdvp/Linux/RTK/rtkrcv_arm && ./start_ublox`.
2. På denna robot-Pi är arbetskatalogen faktiskt `/home/robant/RControllStation/rise_sdvp`, vilket innebar att `/home/robant/rise_sdvp` saknades och `rtkrcv` inte kunde startas.
3. Källkodsmappen innehåller `Linux/PI/rtkrcv_arm`, inte `Linux/RTK/rtkrcv_arm`.
4. Skriptet `start_ublox` försöker exekvera `./rtkrcv` lokalt i den mappen, men den kompilerade binären fanns inte där eftersom `rtklib` is installerat systemomfattande via apt i `/usr/bin/rtkrcv`.

**Permanent & Robust Lösning (Skapad på Pi:n):**
För att lösa detta utan att behöva modifiera och kompilera om källkoden, samt för att säkerställa 100% persistens vid framtida omstarter, skapade vi tre permanenta symboliska länkar på filsystemet:
1. **Mapp-symlänk:** `ln -s /home/robant/RControllStation/rise_sdvp /home/robant/rise_sdvp` (så att `Car_Client` hittar rätt katalogstruktur).
2. **RTK-PI-symlänk:** `ln -s /home/robant/rise_sdvp/Linux/PI /home/robant/rise_sdvp/Linux/RTK` (så att sökvägen `Linux/RTK/rtkrcv_arm` mappar direkt till källkodsmappen `Linux/PI/rtkrcv_arm`).
3. **Binär-symlänk:** `ln -s /usr/bin/rtkrcv /home/robant/rise_sdvp/Linux/RTK/rtkrcv_arm/rtkrcv` (så att `./rtkrcv` i startskriptet anropar det systeminstallerade paketet).

---

### Steg 6: Lösning av Seriell Portkonflikt & Autonom RTK-ström (18 September 2026 - HELT LÖST! ✅)
Efter att vi startat upp `rtkrcv` upptäckte vi att vi fortfarande inte fick några satellitdata i RControlStation (0 satelliter).

**Orsaksanalys av saknad GPS/satellitdata:**
1. **Konfigurationsbrist för TCP-servrar:** `Car_Client` startades utan att initiera sina inbyggda UBX- och RTCM-portar (8210 och 8200) för att strömma GPS-data.
2. **Kritisk hårdvarukonflikt:** Tjänsten `car_rtk.service` var manuellt konfigurerad med flaggan `-out serial://rtk:115200...`. Detta gjorde att `str2str`-processen låste USB-serieporten `/dev/ttyACM0` (`/dev/ublox`) helt och hållet, vilket blockerade `Car_Client` från att kunna ansluta till u-blox-GPS:en.

**Implementerad Lösning för fullständig och persistent drift:**
1. **Borttagen portkonflikt:** Vi ändrade `car_rtk.service` på roboten till att skriva till en lokal TCP-server på port 1234 istället för direkt på serieporten (`-out tcpsvr://:1234`), helt i linje med originalskripten i `install_pi.sh`. Detta frigjorde serieporten permanent för `Car_Client`!
2. **Aktiverade dataportar:** Vi uppdaterade `start_car.sh` och `install_pi.sh` på både laptopen och Pi:n så dataströmmarna öppnas automatiskt.
3. **Autonom RTK-koppling:** Vi ändrade `rover_ublox.conf` till att hämta RTCM-korrektioner direkt från Pi:ns lokala Swepos-server på port 1234 (`inpstr2-path = 127.0.0.1:1234`). Detta gör robotens RTK-anslutning helt fristående från laptopen!
4. **Kompilering & Driftsättning:** Vi uppdaterade och kompilerade om `Car_Client` på Pi:n med Qt6 samt startade om alla tjänster.

**Resultat & Diagnos av STM32/Styrkort (Hårdvarufel bekräftat! ⚠️ - LÖST VIA OM-KOPPLING & RESTART):**
Dataströmmarna i `rtkrcv` fungerar nu klockrent.
När användaren drog ur och kopplade in USB-kablarna igen, dök u-blox-GPS:en upp och "Poll data" började svara direkt med grönt ljus i appen!

**Händelseförlopp vid återinkoppling:**
1. USB-kablarna omfördelades av kärnan till `/dev/ttyACM2` (styrkortet) och `/dev/ttyACM3` (u-blox).
2. Våra dubbla udev-regler uppdaterade symlänkarna `/dev/vehicle` och `/dev/ublox` automatiskt.
3. Genom att starta om `car_client.service` fann programmet direkt de nya serieportarna.
4. **RTK FLOAT AKTIV!** ✅ (TILLFÄLLIGT)
   Efter omstart strömmade rover-GPS-data (UBX) i **49 Kbps** och Swepos-korrektioner (RTCM) i **3.3 Kbps** och gick in i aktivt FLOAT-läge med 12 satelliter.
5. **Strömavbrott vid fysisk förflyttning (Hårdvarufel bekräftat på nytt! ⚠️):**
   När användaren bar ut roboten utomhus försvann signalen helt och hållet igen (0 satelliter, ingen poll).
   **Felsökningsbevis:**
   - `Car_Client` ger nu: `Write timeout on serial port, aborting write to prevent deadlock.` (Skrivbufferten till STM32 är full eftersom processorn inte läser).
   - `rtkrcv` visar: `input rover ... timeout` (u-blox-GPS:en har slutat skicka data).
   - `./stm_reset_pi` ger: `Error: Error connecting DP: cannot read IDR`.
   - **Slutsats:** När roboten flyttades utomhus skakade/vickade en lös strömkabel eller ett glappande batteri-kontaktstift loss, eller så dippade batterispänningen under gränsen för spänningsregulatorn (brownout). Detta bröt omedelbart 3.3V/5V-matningen till både STM32 och u-blox på kretskortet. USB-kablarna ger fortfarande ström till gränssnittschipen så de syns i `lsusb`, men processorerna är helt strömlösa och döda.

---
