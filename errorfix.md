# Åtgärdsrapport (errorfix.md) - RTK-mottagning och Styrsystem

Denna rapport dokumenterar de utförda korrigeringarna för att lösa felen i styrsystemet och RTCM-forwardern, i syfte att säkerställa stabil och oavbruten RTK-mottagning samt robust autopilotfunktion.

---

### Åtgärd 1: Aktiverat seriell drivrutin i RTCM Forwarder-konfigurationen
* **Fil:** `rise_sdvp/Embedded/RTCM_Forwarder_Project/config/halconf.h` (och tillhörande dubblettkopior)
* **Fel:** `#define HAL_USE_SERIAL` var inaktiverad (`FALSE`), vilket förhindrade kompilering då seriella drivrutiner och `SD2` behövdes.
* **Vad som gjordes:** Ändrade till `#define HAL_USE_SERIAL TRUE` för att fullt ut aktivera den seriella maskinvarudrivrutinen i ChibiOS.

---

### Åtgärd 2: Löst Baudrate-missmatch på UART2 mot u-blox ZED-F9P
* **Fil:** `rise_sdvp/Embedded/RC_Controller/ublox.c` (och motsvarande kopior)
* **Fel:** RTCM-forwardern sänder på UART2 i `115200` baud, medan u-blox startar upp i standard `38400` baud på den porten utan att någonsin konfigureras av huvudkontrollern.
* **Vad som gjordes:** Skapat och implementerat funktionen `ublox_cfg_prt_uart2` som under uppstartens initieringssekvens skickar en explicit `UBX-CFG-PRT`-konfiguration till u-blox mottagare. Denna ställer in dess UART2-port till `115200` baud samt aktiverar RTCM3-mottagning på porten. Detta matchar nu perfekt vad forwarder-kortet sänder med.

---

### Åtgärd 3: Korrigerat trasig Makefile i RTCM Forwarder-projektet
* **Fil:** `rise_sdvp/Embedded/RTCM_Forwarder_Project/Makefile`
* **Fel:** Makefilen försökte bygga icke-existerande filer och utelämnade de faktiska källkodsfiler som fanns i katalogen. Dessutom saknades inkluderingsregler för LwIP-nätverksstacken.
* **Vad som gjordes:**
  1. Uppdaterat `CSRC` i Makefilen så att den kompilerar de existerande filerna `src/rtcm_forward.c` och `src/usbconf.c`.
  2. Lagt till inkluderingsinstruktionen för ChibiOS LwIP-bindings (`include $(CHIBIOS)/os/various/lwip_bindings/lwip.mk`).
  3. Lagt till `$(LWSRC)` under `CSRC` och `$(LWINC)` under `INCDIR`.
  4. Aktiverat nätverkshändelser genom att sätta `#define CH_CFG_USE_EVENTS TRUE` i `chconf.h`.

---

### Åtgärd 4: Ersatt den odefinierade funktionen `uartStreamPut` med standard API
* **Fil:** `rise_sdvp/Embedded/RTCM_Forwarder_Project/src/rtcm_forward.c`
* **Fel:** Koden använde en ogiltig metod `uartStreamPut` som inte existerar i ChibiOS eller LwIP-biblioteket.
* **Vad som gjordes:** Ersatte raden med den korrekta ChibiOS-funktionen `chSequentialStreamPut((BaseSequentialStream*)uart_stream, buffer[i]);` som på ett säkert sätt strömmar RTCM-byte sekventiellt över den seriella drivrutinen.

---

### Åtgärd 5: Åtgärdat och säkrat mot Buffer Overflow i `ublox_send`
* **Fil:** `rise_sdvp/Embedded/RC_Controller/ublox.c` (och motsvarande kopior)
* **Fel:** Det fanns en allvarlig risk för minnesöverskrivning om inkommande RTCM3-paket översteg den statiska buffertens storlek på 1024 bytes. Stora MSM7-meddelanden med observationsdata från flera satellitsystem samtidigt kan lätt överstiga denna gräns, vilket orsakade minneskorruption i `.bss`-sektionen med slumpmässiga krascher som följd.
* **Vad som gjordes:**
  1. Ökat storleken på den interna statiska sändningsbufferten från `1024` till `2048` bytes.
  2. Implementerat en hård gräns- och säkerhetskontroll:
     ```c
     if (len > 2048) {
         len = 2048;
     }
     ```
     Detta eliminerar helt risken för buffertspill i RAM-minnet och garanterar systemets stabilitet även under tider med extremt hög satellitaktivitet och stora RTCM3-paket.

---

### Åtgärd 6: Etablerat direkt rå (raw) vidarebefordran av RTCM via USB
* **Fil:** `rise_sdvp/Embedded/RC_Controller/commands.c` (och motsvarande kopior)
* **Fel:** Tidigare krävde `commands.c` att mjukvaruavkodaren `rtcm3_input_data()` var tvungen att framgångsrikt tolka och validera ett helt paket innan det skickades vidare till u-blox via callback-funktionen `rtcm_rx`. Om paketet var av en typ som inte stöddes i mjukvaruavkodaren (t.ex. moderna MSM4/MSM7-meddelanden från Swepos) eller vid minsta bitfel under överföringen kastades hela paketet, vilket ledde till konstanta RTK-drops.
* **Vad som gjordes:**
  1. Modifierat `CMD_SEND_RTCM_USB` så att rådatan omedelbart vidarebefordras till u-blox med `ublox_send(data, len)`. Detta bypassar helt mjukvaru-avkodaren som dörrvakt och låter u-blox mottagarens extremt robusta interna hårdvaru-avkodare hantera alla meddelanden direkt utan fördröjning.
  2. Anpassat callback-funktionen `rtcm_rx` så att den inte dubbelsänder paketen när `UBLOX_EN` är aktivt.
  3. Behållit anropet till `rtcm3_input_data` parallellt enbart för att styrsystemet fortfarande ska kunna hämta ut basstationens position (`1005`/`1006`) till sin lokala ENU-koordinatbas, men utan att dess eventuella begränsningar påverkar dataströmmen till GPS-mottagaren.

---

### Åtgärd 7: Säkrat mot division med noll i `autopilot.c` vid tidssynkronisering
* **Fil:** `rise_sdvp/Embedded/RC_Controller/autopilot.c` (och motsvarande kopior)
* **Fel:** Beräkningen `dist_tot / ((float)time / 1000.0)` utfördes *före* att koden verifierade om `time` uppfyllde minimikraven (`time < min_time_diff`). Om `time` var `0` orsakade detta en direkt division med noll, vilket gav oändliga/NaN-hastigheter.
* **Vad som gjordes:** Flyttade hela valideringsblocket (`if (time < min_time_diff || dist_tot < main_config.ap_base_rad) ...`) så att det körs allra först, *före* divisionsoperationen. Detta förhindrar helt division med noll eller mycket små tider och säkrar ruttföljningens matematiska korrekthet.

---

### Åtgärd 8: Säkrat mot division med noll och NaN-propagerat styrsystem i `utils.c` (Closest Point)
* **Fil:** `rise_sdvp/Embedded/RC_Controller/utils.c` (och motsvarande kopior)
* **Fel:** I `utils_closest_point_line` gjordes beräkningen `t = ap_ab / ab2` utan att kontrollera om `ab2` (linjens längd i kvadrat) var noll. Om ruttens punkter råkade hamna på exakt samma ställe (t.ex. dubbletter), kraschade matematiken med en division med noll, vilket gav `NaN`-värden på bilens målexposition som helt förstörde autopilotens styrning.
* **Vad som gjordes:** Lagt till en explicit säkerhetsspärr:
  ```c
  float t;
  if (ab2 <= 1e-6f) {
      t = 0.0f;
  } else {
      t = ap_ab / ab2;
  }
  ```
  Detta eliminerar risken för division med noll vid beräkning av närmaste punkt mot punkt-liknande linjesegment, och tvingar parametern `t` till en stabil standardsituation (0.0).

---

### Extra: Löst kompileringsfel för Mactrac-plattformen
* **Fil:** `rise_sdvp/Embedded/RC_Controller/conf_general.h` (och motsvarande kopior)
* **Fel:** Ett pre-existerande kompileringsfel fanns på `mactrac`-plattformen där både `IS_DRANGEN` och `IS_MACTRAC` blev definierade samtidigt under bygget. Det orsakade en redefinition av `wheel_diam` och `cnts_per_rev` i `wheelspeed.c` vilket fick kompileringen att misslyckas.
* **Vad som gjordes:** Omslöt definitionen av `DRANGEN_NY` med ett villkorligt `#ifndef IS_MACTRAC` block. Nu byggs båda plattformarna (`drangen` och `mactrac`) felfritt.
