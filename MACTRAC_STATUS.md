# MacTrac — status

Samarbete med SLU och Jordbruksverket. En maskin (Lövsta). Körs med **RControlStation**
(ingen robotstyrning/robotd).

## Upplägg
- Gunnars Jetson + styrkort lämnas orörda. Vi testar med **egen Pi + eget styrkort** och
  kopplar tillbaka hans när vi är klara.
- All hydraulik (armar, styrventil via ADDIO, FTR2-vinkelgivare) går via styrkortets CAN.
  Gasen är PWM från styrkortets servoutgångar. Jetson kör bara Car_Client.
- Pi:n `mactrack-slu` (användare `macbot-slu`), WireGuard 192.168.200.12:
  `git clone -b mactrac https://github.com/lordajz-cmyk/rise_sdvp.git ~/rise_sdvp`
  sedan `sudo bash wireguard.sh` och `sudo bash install_pi.sh` i ~/rise_sdvp.

## Firmware (den här grenen)
- Tagg `gunnar-mactrac` = `2d3565b` (2026-09-04), Gunnars senaste commit. Firmwaren är
  oförändrad sedan `a6b1352` (2026-08-20). Byggd med `DRANGEN_NY` avkommenterad för hand —
  troligen det som sitter i maskinen.
- Grenen `mactrac` = Gunnars version + rättningar, en commit var:
  1. `make mactrac` bygger utan handpåläggning (DRANGEN_NY av för MacTrac)
  2. Sparning av inställningar låser per variabel (ingen "Write timeout")
  3. Servo-tråden byter VESC-id under lås; ingen utskrift per spakkommando
  4. Autopilot: ingen division med noll när två ruttpunkter sammanfaller
- **Medvetet INTE med** (från master): Geminis u-blox-ändringar (UART2/RTCM-väg),
  "id 0 = alla", autopilot_sync_point-omflyttning; RobAnt-diagnostik, CMD_GET_VESC_STATUS,
  RobAnts givare.
- Car_Client, udev och rover_ublox.conf från master (verifierat på RobAnt), se commit.
- `install_pi.sh` + `wireguard.sh` rättade (udev-sökväg, Swepos valfritt, ingen resolvconf).
- Bygg: `cd Embedded/RC_Controller && make mactrac` → `build/fw_mactrac.elf`.
  Flasha ALLTID elf (inte bin), annars raderas inställningarna i EEPROM.

## Pi 5:an (mactrack-slu) — rättat 2026-09-28
- **rtkrcv startade aldrig** på Pi:n, så port 2948 (position/RTK till RControlStation)
  saknades. Två fel: `./rtkrcv` finns inte i git (bara byggd för hand på RobAnt), och
  Car_Client letade efter rtkrcv-mappen på hårdkodade sökvägar som inte finns här.
  Rättat: `start_ublox` använder `/usr/bin/rtkrcv` (Debians rtklib, 2.4.3) om `./rtkrcv`
  saknas, och Car_Client hittar `Linux/PI/rtkrcv_arm` bredvid sig själv. Ombyggt och
  omstartat på Pi:n; rtkrcv startas nu av Car_Client och tar emot u-blox (8210) och
  Swepos (1234). RTK Fix ej verifierat än (står inne utan antenn).
- Pi 5 har skrivbord (labwc/Wayland, autologin `macbot-slu`).
- **Statusskärm installerad 2026-09-28** (`Linux/StatusSkarm`, `bash installera.sh`):
  helskärm i skrivbordet (autostart), 3:00-nedräkning, sju rutor. Verifierat med
  `statusskarm --text`: styrkort svarar (UDP via Car_Client, CMD_AP_GET_ROUTE_PART —
  nollställer inte kortets säkerhetstid), Car_Client, internet (WireGuard-handskakning)
  och RTK-tjänst gröna; RTK Float/Fix grå (ingen antenn); CPU ~1 %. sudoers
  `/etc/sudoers.d/statusskarm`: exakt fem kommandon (restart car_client, car_rtk,
  wg-quick@wg0, reboot, `wg show wg0 latest-handshakes`). Ingen skärm beställd än
  (förslag: Raspberry Pi Touch Display 2, 5"). Ruta "CAN lever" medvetet utelämnad.
- `Linux/Car_Client/Car_Client` i git är en x86-64-binär. På Pi:n är den ombyggd
  (aarch64) och visas som ändrad — kör aldrig `git checkout` på den filen.

## Kortet hängde vid Write — orsak och rättning (2026-09-28)
Symptom i Lövsta: första Write i Confcommon gick bra, varje senare Write hängde kortet
("Write timeout on serial port" i Car_Client, USB kvar men inget svar). En omstart av
Car_Client hjälpte inte, 12 V av/på inte heller (kortet matas via USB); bara reset över
ST-Link. Gunnar har aldrig sett det: hans äldre firmware (30.1) saknar fälten nedan.

**Orsak (firmware, `CMD_SET_MAIN_CONFIG` i commands.c):**
1. RControlStation skickar inställningarna t.o.m. aktuatorerna (packetinterface.cpp),
   men firmwaren läste vidare 160 byte till: sensorer och reglerloopar (state control).
   De bytena kom ur mottagningsbufferten, där GPS-text från tidigare paket låg kvar.
   Kortets inställning efter en Write: `sensors` = 11315, `state_controls` = 12344,
   reglerloopar med "enabled" satt och skräp i `control_type` (avläst 14:58).
2. `state_control_init()` kördes mitt i Write, *före* tolkningen, och läste den förra
   (redan trasiga) inställningen ur EEPROM. Därmed slogs reglerlooparna på, och
   huvudtråden körde dem var 10:e ms: `pid_controllers[control_type]` med index ≈ 11000
   och sensorloopar till 11315 → minnesåtkomst utanför RAM → HardFault → kortet står still.
3. Efter reset fungerar kortet, eftersom `state_control_init()` inte körs vid start. Men
   skräpet ligger kvar i EEPROM, så nästa Write hänger direkt igen.

Hänga vid ruttuppladdning gör det inte: mätningen 14:51 visade att rutt (55), rensa (57)
och start (59) kvitterades normalt. Det som hängde 15:02 var Write av Heartbeat.

**Rättat i den här grenen:**
- Write läser heartbeat, aktuatorer, sensorer och reglerloopar bara om de finns i paketet
  (saknas sensorer/loopar → 0; äldre RControlStation utan heartbeat → värdet behålls).
- `conf_general_sanitize_main_config()`: fler än 4 sensorer/loopar → 0, `enabled` bara
  0/1, okänd `control_type` → av. Körs vid Write och vid start, så skräpet som redan ligger
  i EEPROM nollställs.
- `state_control_init()` körs efter att nya värden sparats och läser `main_config`.
- PID per reglerloop (förut indexerat med `control_type`, 0–4, utanför arrayen på 4).
- `motor_get_actuators_by_activity()` och sensorfunktionerna läser `main_config` i stället
  för hela EEPROM:et (förut vid varje spakkommando; ~600 byte stack och hundratals
  EEPROM-sökningar per anrop).
- NMEA från u-blox-tråden får egen sändbuffert (skrev förut i `m_send_buffer` samtidigt
  som kommandotråden byggde svar där).

**Testa på kortet** (på Pi:n, RControlStation frånkopplad):
`python3 ~/rise_sdvp/Linux/tools/testa_write.py 3` skriver tillbaka kortets egna värden
som RControlStation gör och kontrollerar efter varje gång att kortet lever. Gammal
firmware: hänger på första Write (skräpet ligger i EEPROM). Ny: tre OK, och
sensorer/reglerloopar visas som 0.

## Autopilot, heartbeat och stopp (2026-09-28 kväll)
- **Heartbeat i sekunder** (som RControlStation visar). Förut användes värdet som ms:
  standard 3 = 3 ms stängde av autopiloten direkt. 0 = av. Nytt värde gäller direkt
  efter Write. Rekommenderat: 3 s (4G-luckor på upp till ~1 s har mätts).
- **Stopp:** när autopiloten stängs av (Stop, tappad förbindelse) eller rutten tar slut
  går gasen till neutralt och armarna stannar direkt (`motor_stop()`). Förut låg gasen
  kvar upp till 2 s och armarna upp till 10 s.
- **Stopp vid Write:** autopilot av, gas neutral, armar stopp innan inställningar sparas.
- **Autopilotens fart är öppen styrning:** gaspådrag = fart (m/s) × 0,5.
  1 km/h ≈ 0,14, 2 km/h ≈ 0,28, 3 km/h ≈ 0,42 (manuellt kör du med Max 0,35).
- RControlStation (master): nya ruttpunkter fick oinitierat minne som attribut och fart.
  Rutten 14:51 hade attribut 0x6518 = "bakre armar upp" och fart 0. Rättat
  (mapwidget.cpp). **Lägg ut nya rutter**, gamla punkter bär med sig skräpet.

## Att göra (2026-09-29, Lövsta)
1. Pi:n: `cd ~/rise_sdvp && git pull` (RControlStation frånkopplad).
2. Flasha: `./flash_styrkort_macbot.sh` (motor av, skriv FLASHA). Skriptet läser av kortet efteråt.
3. `python3 ~/rise_sdvp/Linux/tools/testa_write.py 3`: tre Write i rad utan att kortet hänger.
4. Confcommon: Heartbeat = 3, sedan Write.
5. Statusskärmen: rutan Styrkort ska vara grön.
6. Vinkelgivaren: Terminal på bilfliken, `addio_read`. FTR2-vinkeln ska ändras när du styr.
7. Hastighetsgivaren: kör en bit, farten på bilfliken ska inte stå på 0.
8. Antennen: centrerat i sidled, mät avståndet framför bakaxeln och skriv in det som GPS Ant X (Ant Y = 0), Write.
9. Autopilot: anslut och vänta på RTK Fix, kontrollera att pilen pekar rätt, V = 2 km/h,
   Delete current route, lägg cirka 10 punkter rakt fram med Shift+klick, Write route to car,
   Autopilot. Nödstoppet i handen; Stop om den svänger bort från punkt 1.
