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

## Att göra
1. Besök i Lövsta: foton, Confcommon-skärmbilder, CAN-terminering, Jetson-läsrunda
   (checklista: robot-control/besok_mactrac_checklista.md).
2. Flasha vårt kort med `gunnar-mactrac` först → ställ in enligt skärmbilderna → testa.
3. Sedan `mactrac`-grenen → testa igen.
4. Pi:n i ordning, bytet på plats, testordning: styrning och armar före drivning.
