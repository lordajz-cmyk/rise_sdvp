# GEMINI.md - Loggbok & Utvecklingsplan för RTK-problem

> ### 🛑 VIKTIGT MANDAT FÖR ALLA FRAMTIDA AI-SESSIONER (ROUTINE)
> Denna fil (`GEMINI.md`) och dess systerfil (`fyi.md`) fungerar som projektets primära långtidsminne och loggbok för denna workspace.
> **Varje framtida session MÅSTE löpande och dynamiskt uppdatera båda dessa filer** med alla genomförda källkodsändringar, nyfunna buggar, nyligen tillagda skript och framsteg. Detta är en obligatorisk rutin för att bibehålla fullständig kontinuitet över sessioner.

Denna fil innehåller en kontinuerlig logg över undersökningar, fynd, strategiska förslag samt planerade och utförda åtgärder för att lösa problemen med att RTK inte fungerar eller att anslutningen kontinuerligt tappas.

---

## 🔍 Vad jag hittat (Findings)

Under min djupgående orientering och genomgång av kodbasen hittade jag **åtta kritiska fel och brister** spridda över robotens huvudkontroller (`RC_Controller`) och den separata nätverksforwardern (`RTCM_Forwarder_Project`).

1. **Trasig RTCM-forwarder-kod:** Projektet `RTCM_Forwarder_Project` kunde inte kompileras på grund av en avstängd seriell drivrutin i `halconf.h`, odefinierade anrop till `uartStreamPut` (ska vara `chSequentialStreamPut`), samt en Makefile som refererade till källkodsfiler som inte existerade på disken (`tcp_client.c`, `uart_writer.c`, etc.).
2. **Baudrate-missmatch för RTCM-forwarding:** Forwardern skickade korrektioner till u-blox UART2 med `115200` baud, medan u-blox standard UART2-inställning var `38400` baud. Denna missmatch gjorde dataströmen korrupt.
3. **Kritiskt buffertspill i `ublox_send` (Huvudkontrollern):** RTCM-paket kopierades till en statisk buffert på endast 1024 bytes utan storlekskontroll. Moderna, fullständiga Swepos-meddelanden (t.ex. MSM7-meddelanden med observationsdata för många satellitsystem) är ofta större än 1024 bytes, vilket orsakade buffertspill i RAM-minnet (.bss-sektionen) och ledde till slumpmässiga krascher och tappad RTK-anslutning under drift.
4. **Mjukvaruflakshals för RTCM-data:** `RC_Controller` krävde och tvingade att inkommande USB RTCM-paket måste avkodas felfritt av den egna mjukvaran (`rtcm3_simple.c`) innan de vidarebefordrades till u-blox. Eftersom mjukvaruavkodaren saknar stöd för moderna MSM4/MSM7-meddelanden (som används av Swepos för t.ex. Galileo och BeiDou), eller om enstaka byte-förluster uppstod, kastades paketen bort istället för att skickas till u-blox-mottagaren som har en inbyggd hårdvaruavkodare med fullt stöd.
5. **Pre-existerande bygg-konflikt för Mactrac-plattformen:** Vid sammanställning av `mactrac`-target blev både `IS_DRANGEN` och `IS_MACTRAC` definierade samtidigt vilket orsakade redefinition av hjulinställningar och byggstopp.
6. **Division med noll i `autopilot.c` (Tidssynkronisering):** Beräkningen `dist_tot / ((float)time / 1000.0)` gjordes före tidsvalideringen, vilket kunde orsaka division med noll om `time` var `0`.
7. **Division med noll och NaN-förstörelse i `utils.c` (Closest Point):** Beräkningen `t = ap_ab / ab2` saknade skydd mot `ab2 == 0`. Om ruttens punkter låg på samma plats blev närmaste punkt `NaN`, vilket förstörde autopilotens positionering och styrning.

---

## 💡 Föreslagna lösningar (Proposed Solutions)

Följande lösningar har tagits fram, implementerats och framgångsrikt kompilerats:

1. **Säkerställ mjukvarukompilering för Forwardern:** Fixa Makefilen, aktivera `HAL_USE_SERIAL` och byt ut `uartStreamPut` mot `chSequentialStreamPut`.
2. **Synka seriell hastighet (Baudrate):** Sätt u-blox UART2 till `115200` baud från huvudkontrollern (`ublox_cfg_prt_uart2`) så dataströmmarna matchar perfekt.
3. **Fixa buffertspillet (Säkerhet & Stabilitet):** Öka bufferten i `ublox_send` till `2048` bytes och lägg till en storlekskontroll (`len = len > 2048 ? 2048 : len;`) för att helt eliminera risken för minneskorruption.
4. **Implementera Rå (Raw) Vidarebefordran av RTCM:** Modifiera `commands.c` så därmed att RTCM-data skickas direkt och oavkortat till u-blox via `ublox_send` så fort det tas emot via USB. Kör fortfarande avkodaren `rtcm3_input_data()` parallellt enbart för att extrahera basstationspositionen (`1005`/`1006`) till ENU-referensen.
5. **Korrigerat konflikt vid bygge av Mactrac:** Omslöt `DRANGEN_NY` i `conf_general.h` för att undvika dubbeldefinition av hjuldiameter och pulser per varv.
6. **Säkra autopilotens matematik:** Sätt tidsvalideringen i `autopilot.c` före hastighetsberäkningen för att eliminera risken för division med noll.
7. **Säkra `utils_closest_point_line` matematik:** Sätt en säkerhetskontroll i `utils.c` som tvingar parametern `t` till `0.0f` om linjesegmentet är punktformat (`ab2 <= 1e-6f`).

---

## 🛠️ Logg: Vad jag har gjort (What I Have Done)

### Fas 1: Källkodsanalys & Rättningar (Tidigare fas)
- [x] Orienterat mig i arbetskatalogen och kartlagt hela RTK/RTCM-dataflödet.
- [x] Identifierat och analyserat dolda matematiska brister och sårbarheter (division-by-zero, NaN-förstörelse, byggkonflikter).
- [x] Skapat en dedikerad felrapport (`errorreport.md`) och åtgärdsrapport (`errorfix.md`) som i detalj beskriver felen och vad som gjorts.
- [x] **Aktiverat den seriella maskinvarudrivrutinen (`HAL_USE_SERIAL = TRUE`) och nätverkshändelser (`CH_CFG_USE_EVENTS = TRUE`) i forwardern.**
- [x] **Fixat Makefilen i `RTCM_Forwarder_Project` så det bygger utan fel.**
- [x] **Ersatt den odefinierade funktionen `uartStreamPut` med den korrekta `chSequentialStreamPut` i `rtcm_forward.c`.**
- [x] **Ökat sändningsbufferten i `ublox_send` till 2048 bytes samt lagt till en hård storlekskontroll mot buffertspill.**
- [x] **Skapat funktionen `ublox_cfg_prt_uart2` och lagt till automatisk konfiguration av u-blox UART2 till 115200 baud vid uppstart.**
- [x] **Ändrat styrsystemets USB-RTCM-mottagning till direkt råvidarebefordran, vilket helt bypassar mjukvaruavkodaren som dörrvakt.**
- [x] **Löst en pre-existerande bygg-konflikt i `conf_general.h` som hindrade `mactrac`-firmware från att byggas.**
- [x] **Åtgärdat division-med-noll bugg i `autopilot.c` vid tidssynkronisering.**
- [x] **Åtgärdat division-med-noll och NaN-propageringsrisk i `utils.c` vid beräkning av närmaste ruttpunkt.**
- [x] **Framgångsrikt provbyggt både `drangen` och `mactrac` firmware-målen utan ett enda kompilerings- eller länkningsfel.**

### Fas 2: Paketering, Automatisering & Fysiska Tester (14 September 2026)
- [x] **Kartlagt hårdvaruarkitekturen på roboten:** Bekräftat att det handlar om **endast två fysiska kort** (1x Raspberry Pi 4 + 1x integrerat Carcontroller-styrkort med onboard u-blox GPS-mottagare). Dessa ansluts via **två USB-kablar** (Kabel 1 för styrning `/dev/car`, Kabel 2 för GPS `/dev/ublox`).
- [x] **Skapat `installationsguide.md`:** En mycket pedagogisk svensk guide med ett ASCII-kopplingsschema, fullständig pinout-tabell för ST-LINK V2 (SWCLK, SWDIO, GND, 3.3V) samt trådlös anslutningsguide (SSH/Steg 0).
- [x] **Skapat `install_dator.sh`:** Ett intelligent installationsskript för utvecklingsdatorn (laptopen). Det detekterar automatiskt systemversionen (stödjer Ubuntu 22.04 Jammy genom att mappa till `libqt6serialport6-dev`/`libqt6charts6-dev`) och bygger hela **RControlStation** med CMake till undermappen `build/cmake_linux/build/lin/RControlStation`.
- [x] **Skapat `install_pi.sh`:** Konfigurerar robotens Raspberry Pi 4 (installerar paket, udev-regler, interaktiv Swepos RTK-konfigurering för `car_rtk.service`, bygger `Car_Client` samt installerar boot-autostart i en bakgrunds-`screen`).
- [x] **Skapat `flash_styrkort.sh`:** Ett fristående skript som bygger styrkortets mjukvara (Drängen eller Mactrac) och flashar det direkt via en **ST-LINK V2** ansluten till datorn via OpenOCD.
- [x] **Skapat `install_allt.sh`:** Hel-installerare som kör både Pi- och styrkortsskripten sekventiellt för en snabb nyinstallation. Nu uppdaterat med intelligent detektering och automatisk git-kloning av repot om källkoden saknas.
- [x] **Skapat och vidareutvecklat `wireguard.sh`:** Ett intelligent, interaktivt installationsskript för WireGuard VPN på Linux (Pi eller PC). Skriptet installerar alla paket, genererar automatiskt kryptonycklar och låter användaren välja maskinens roll i nätverket (t.ex. Nya Drängen, Gamla Drängen, MacTrac EIP eller Gunnars dator). Det skapar automatiskt en fullständig, optimerad `/etc/wireguard/wg0.conf` med rätt fasta VPN-IP-adresser, portar och förkonfigurerade serveruppgifter, aktiverar autostart via systemd, samt skriver ut exakt kodsnutt som ska läggas in på servern för enkel registrering. Vips så slipper man allt manuellt konfigurationskrångel!
- [x] **Skapat administratörsverktyget `wireguard_admin.sh`:** Ett komplett, interaktivt verktyg för att administrera WireGuard-servern (körs på servern, t.ex. `192.168.200.1`). Det kan initiera en helt ny server från grunden, aktivera IP-forwarding/NAT-routing, registrera nya klienter (peers) interaktivt och ladda om WireGuard sömlöst (`wg syncconf`) live utan att störa befintliga anslutningar, samt visa och ta bort registrerade användare. Den perfekta följeslagaren för serverägaren!
- [x] **Implementerat smart paketfelrapportering:** Om en samlad paketinstallation via apt misslyckas så faller installationsskripten tillbaka till att installera paketen individuellt och spottar ut en snygg sammanfattning av exakt vilka paket som saknas.
- [x] **Implementerat interaktivt skrivbordstest i `flash_styrkort.sh`:** Direkt efter lyckad flashning kan användaren välja att ansluta de två USB-kablarna. Skriptet känner automatiskt av `/dev/ttyACM0` och `/dev/ttyACM1` samt skriver ut den senaste live-satellitdatan (NMEA) direkt i terminalfönstret för blixtsnabb hårdvaruverifiering!
- [x] **Fysiskt Verifierad Flashing:** Testkört `./flash_styrkort.sh` på datorn med inkopplad ST-LINK V2 mot styrsystemkortet (Drängen). Programmeringen raderades, skrevs, verifierades (Verified OK) och startades om helt utan fel! Target-spänningen avlästes perfekt till `3.1789 V`.
- [x] **Skrivbordstester (Bench Tests):**
  - **u-blox GPS (ttyACM1):** Verifierat att GPS-chippet startar och strömmar råa NMEA-satellitdata (`$GNGGA,,,,,,0,00,99.99,,,,,,*56`) direkt till laptopen via USB.
  - **STM32 Controller (ttyACM0):** Verifierat att ChibiOS startar upp sitt USB CDC-telemetrigränssnitt stabilt.

### Fas 3: System- och Nätverkssäkring samt Hårdvaruintegration (15 September 2026)
- [x] **Förbättrat och automatiserat nätverkssök i `wireguard_admin.sh`:** Skriptet detekterar nu automatiskt maskinens aktiva nätverkskort (t.ex. `enp3s0` eller `eno1`) istället för att använda det hårdkodade `eth0`. Detta gör serverinstallationen helt plug-and-play på valfri modern Ubuntu-server!
- [x] **Skapat `Wireguard_Guide.md`:** En komplett och lättfattlig svensk instruktionsguide för hur man steg-för-steg sätter upp en helt ny WireGuard VPN-server hemma, vidarebefordrar portar samt ansluter klienter (robot och laptop).
- [x] **Skapat `Sakerhetsanalys.md`:** En djupgående säkerhets- och riskanalys som täcker cybersäkerhet (VPN, MitM-risker), funktionell realtidssäkerhet (analys av failsafe-kontrollen i `timeout.c`), kodintegritet (matematiska skydd och buffertspillsäkring), samt fysiska operativa risker (E-stop/nödknappar och hinderdetektering).
- [x] **Skapat isolerad backup-mapp `uppdaterade_filer/`:** Samlat en ren kopia av samtliga **11 modifierade källkodsfiler** bevarande deras exakta katalogstruktur (`rise_sdvp/Embedded/...`) samt lagt till en förklarande indexfil `lista_filer.md`. Detta gör att användaren kan uppdatera sin fysiska robot säkert och kontrollerat i små steg utan risk för regressionsfel.
- [x] **Uppdaterat `installationsguide.md`:** Anpassat guiden till det nya, simplifierade trådlösa installationsflödet där installationsfiler snabbt och enkelt skickas trådlöst från utvecklingsdatorn till robotens Raspberry Pi med ett enda `scp`-kommando över routerns Wi-Fi.

### Fas 4: Benjamin-alignment & Dynamisk VPN-flexibilitet (16 September 2026)
- [x] **Anpassat installationsskript till Benjamin Vedders referensdesign:** Uppdaterat `install_pi.sh` så det automatiskt detekterar och installerar Qt6-paket (`qt6-base-dev` etc.) och kompilerar med `qmake6` om det finns tillgängligt, med graciös fallback till Qt5 på äldre system.
- [x] **Implementerat dubbla USB-portssymlänkar i udev:** Uppdaterat udev-regelsmallarna så att de skapar både de gamla (`/dev/car` och `/dev/ublox`) och Benjamins referensnamn (`/dev/vehicle` och `/dev/rtk`) samtidigt för 100% bakåtkompatibilitet.
- [x] **Refaktorerat `wireguard.sh` till att bli helt dynamiskt:** Tagit bort de hårda alternativen för gamla robotar ("Drängen" etc.) och ersatt dem med interaktiva frågor där användaren kan skriva in valfritt namn och valfri IP-adress i VPN-nätverket (`192.168.200.X`), med intelligenta standardval.
- [x] **Konfigurerat den nya Lenovo ThinkCentre-servern:** Integrerat den nyskapade serverns Public Key och dess DuckDNS-domän (`maprosystems.duckdns.org`) som hårdkodat standardval i klient-skripten för automatiserad parkoppling.

### Fas 5: VPN-routing, Driftsättning & Telemetriverifiering (17 September 2026)
- [x] **Säkrat VPN-routing på Servern:** Identifierade att VPN-servern (`mapro-server`) blockerade trafik mellan klienter (`ip_forward=0`). Aktiverade och permanentade `net.ipv4.ip_forward=1` i `/etc/sysctl.conf` vilket omedelbart tillät laptopen och Pi:n att prata krypterat över 4G-hotspot!
- [x] **Felsökt och löst resurskonflikter på Pi:** Åtgärdade *"Address in use"* och låsta serieportar genom att stoppa `car_client.service` och rensa gamla processer (`killall Car_Client`), vilket gjorde att vi kunde starta programmet rent med rätt flaggor (`-p /dev/vehicle`).
- [x] **Verifierat Fullständig Telemetrilänk:** Testat anslutningen i fält-scenario (över mobil hotspot). RControlStation lyckades ansluta perfekt till `192.168.200.10:8300` via WireGuard VPN och strömma live-telemetri ("Poll data") med full precision!
- [x] **Löst IP-bindningskrasch i Flask-servern:** Ändrade `app.run(host='192.168.200.3')` till `host='0.0.0.0'` i `server.py` så att webbservern nu startar felfritt på alla maskiner, oavsett lokala nätverks-IP.

---

📋 Nästa steg (Next Steps)

När du står i verkstaden och ska montera ihop systemet i bilen följer du bara dessa sista enkla steg:
1. **Förbered Raspberry Pi 4:** Flash-ladda ett rent SD-kort med Pi OS/Ubuntu, sätt i det i din Pi 4 på roboten och starta upp den.
2. **Koppla in allt trådlöst (SSH):** Koppla upp din laptop mot robotens router och anslut till din Pi trådlöst via SSH (som beskrivs i `installationsguide.md` Steg 0).
3. **Kör robotinstallationen:** Öppna terminalen på din Pi, gå till projektkatalogen och starta installationsskriptet:
   ```bash
   cd ~/rise_sdvp
   sudo ./install_pi.sh
   ```
   *Skriptet installerar alla bibliotek, skapar dina Swepos-uppgifter, bygger Car_Client och sätter igång allt i bakgrunden.*
4. **Kör RControlStation på laptopen:** Gå till din dators terminal och starta igång programmet:
   ```bash
   cd ~/RControllStation/rise_sdvp/rise_sdvp/Linux/RControlStation/build/cmake_linux/build/lin
   ./RControlStation
   ```
5. **Njut av driften:** Anslut till din robot och se statusindikatorn gå till **RTK Fix** (grön indikator, ~3 cm precision) under bar himmel!
