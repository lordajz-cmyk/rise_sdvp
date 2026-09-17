# Felrapport (errorreport.md) - RTK-mottagning och Styrsystem

Denna rapport sammanställer de identifierade mjukvaru- och konfigurationsfelen i styrsystemet samt tillhörande RTCM-forwarder som orsakar att RTK-mottagningen antingen inte fungerar alls eller kontinuerligt tappar anslutningen.

---

### Fel 1: Seriell drivrutin inaktiverad i RTCM Forwarder-konfigurationen
* **Fil:** `rise_sdvp/Embedded/RTCM_Forwarder_Project/config/halconf.h` (och `halconf.h` i projektets rot)
* **Problem:** `#define HAL_USE_SERIAL` är satt till `FALSE`. Detta inaktiverar standard seriella drivrutiner helt och hållet i ChibiOS-operativsystemet. Det gör att projektet inte kan kompileras eftersom `SerialConfig` och den seriella drivrutinen `SD2` (som används för att kommunicera med u-blox) är odefinierade.

---

### Fel 2: Baudrate-missmatch på UART2 mot u-blox ZED-F9P
* **Fil:** `rise_sdvp/Embedded/RTCM_Forwarder_Project/src/main.c` och `rise_sdvp/Embedded/RC_Controller/ublox.c`
* **Problem:** RTCM Forwarder konfigurerar sin `SD2` (UART2) till `115200` baud. Men u-blox ZED-F9P-mottagaren har standardbaudrate `38400` på sin UART2 vid uppstart. Eftersom huvudkontrollern (`RC_Controller`) endast konfigurerar u-blox UART1 (till 115200) och aldrig konfigurerar UART2, uppstår en baudrate-missmatch. u-blox-mottagaren tar därmed bara emot korrupt skräpdata via UART2 och kan aldrig uppnå RTK-fix.

---

### Fel 3: Trasig och inkonsekvent Makefile i RTCM Forwarder-projektet
* **Fil:** `rise_sdvp/Embedded/RTCM_Forwarder_Project/Makefile`
* **Problem:** Makefilen anger i variabeln `CSRC` källkodsfiler som inte existerar på disken (`src/tcp_client.c`, `src/uart_writer.c`, `src/led.c`) och utelämnar de faktiska filer som finns i katalogen (`src/rtcm_forward.c`, `src/usbconf.c`). Detta gör att kompileringen kraschar direkt med felmeddelanden om saknade filer.

---

### Fel 4: Odefinierad funktion/makro `uartStreamPut` i `rtcm_forward.c`
* **Fil:** `rise_sdvp/Embedded/RTCM_Forwarder_Project/src/rtcm_forward.c`
* **Problem:** Källkoden använder funktionen `uartStreamPut(uart_stream, buffer[i])` för att skicka RTCM-byte till seriell ström. Denna funktion existerar inte i ChibiOS eller LwIP, vilket leder till ett kompileringsfel (undefined reference).

---

### Fel 5: Allvarlig risk för minnesöverskrivning (Buffer Overflow) i `ublox_send`
* **Fil:** `rise_sdvp/Embedded/RC_Controller/ublox.c` (och motsvarande kopior)
* **Problem:** I funktionen `ublox_send(unsigned char *data, unsigned int len)` kopieras data till en statisk buffert `static uint8_t buffer[1024]` med `memcpy(buffer, data, len)` utan att kontrollera om `len` överstiger 1024 bytes. Moderna RTCM3 MSM-korrektionsmeddelanden (t.ex. MSM7 för GPS/GLONASS/Galileo med många synliga satelliter samtidigt) kan lätt överstiga 1024 bytes. När detta inträffar skrivs data över i `.bss`-sektionen, vilket förstör intilliggande variabler och resulterar i slumpmässiga krascher, trådhängningar eller tappad RTK-anslutning.

---

### Fel 6: Onödig mjukvaruavkodning av RTCM i `commands.c` (Flaskhals och databortfall)
* **Fil:** `rise_sdvp/Embedded/RC_Controller/commands.c`
* **Problem:** När RTCM-data tas emot via USB (`CMD_SEND_RTCM_USB`) skickas varje byte genom mjukvaruavkodaren `rtcm3_input_data()`. Callback-funktionen `rtcm_rx` (som i sin tur anropar `ublox_send` för att mata u-blox-mottagaren) aktiveras endast om paketet lyckas avkodas och verifieras felfritt av mjukvaran. Om mjukvaruavkodaren inte stödjer meddelandetypen (vilket är fallet för t.ex. Swepos MSM4/MSM7-meddelanden 1074, 1084, 1094 som inte finns i switchen), eller om ett tillfälligt bitfel uppstår under USB-överföringen, kastas hela paketet bort. Det skickas då aldrig till u-blox som har en mycket mer robust hårdvaru-avkodare för RTCM3. Detta leder till konstanta avbrott i RTK-positioneringen.

---

### Fel 7: Division med noll i `autopilot.c` vid tidssynkronisering
* **Fil:** `rise_sdvp/Embedded/RC_Controller/autopilot.c`
* **Problem:** Hastigheten beräknas via `dist_tot / ((float)time / 1000.0)` innan koden kontrollerar om `time < min_time_diff`. Om `time` är `0` (eller mindre än 100 ms) utförs en division med noll, vilket resulterar i oändlig (`inf`) eller `NaN` (Not a Number) hastighet som sedan sparas i ruttpunkterna.

---

### Fel 8: Division med noll och NaN-propagerat styrsystem i `utils.c` vid beräkning av närmaste punkt på linjesegment
* **Fil:** `rise_sdvp/Embedded/RC_Controller/utils.c`
* **Problem:** I funktionen `utils_closest_point_line` utförs divisionen `t = ap_ab / ab2` utan att kontrollera om `ab2` (linjesegmentets kvadratiska längd) är noll. Om två intilliggande ruttpunkter råkar ha identiska koordinater (t.ex. vid användarfel eller dubblettimport) blir `ab2 = 0`, vilket leder till division med noll och att `t` samt den resulterande närmaste punkten fylldes med `NaN`-värden. Detta propagerar direkt till autopiloten under ruttföljning och kan leda till allvarliga styrsystemfel, plötsliga svängar eller krascher.
