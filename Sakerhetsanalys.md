# 🛡️ Säkerhets- och Riskanalys: Drängen, RobAnt & Macbot Styrsystem

Denna analys täcker både **cybersäkerhet (nätverk och mjukvara)** och **fysisk/funktionell säkerhet (maskinsäkerhet och kontrollogik)** för styrsystemet som används i Drängen och Macbot. Den beskriver befintliga säkerhetsmekanismer, identifierade risker och konkreta rekommendationer för att minimera faror under drift.

---

## 📐 1. Systemöversikt & Angreppsytor

Systemet består av en distribuerad arkitektur med tre huvudsakliga lager:
1. **RControlStation (Laptop/Klient):** Grafiskt gränssnitt som skickar rutter och kommandon över trådlöst nätverk.
2. **Car_Client (Raspberry Pi 4):** Robotens "hjärna" som kommunicerar trådlöst med laptopen och skickar vidare data lokalt till styrsystemet via USB.
3. **RC_Controller (STM32-styrkort):** Mikrodatorn som kör realtidsoperativsystemet ChibiOS, utför reglerloopar, kör autopiloten och styr motorer/servon direkt.

```text
  [ RControlStation ]  <==== Trådlöst Nätverk (Wi-Fi/4G) ====>  [ Car_Client (Pi 4) ]
     (Styrdator)                                                   (Robot-Hjärna)
                                                                         ||
                                                                     USB-kablar
                                                                         ||
                                                                         V
                                                                [ RC_Controller ]
                                                                 (STM32-Styrkort)
```

Denna arkitektur ger upphov till tre primära angreppsytor och riskkategorier:
* **Nätverkssårbarheter:** Avlyssning, paketinjektion eller kapning av den trådlösa länken.
* **Fysiska kontrollfel:** Skenande robot till följd av signalförlust, mjukvarukrascher eller sensorbortfall.
* **Mjukvarusårbarheter:** Buffertspill eller matematiska fel (t.ex. NaN/division-med-noll) som kraschar styrsystemet live under gång.

---

## 🌐 2. Nätverks- och Cybersäkerhet (Cyber Security)

### 🔴 Risk: Okrypterad telemetri och fjärrstyrning
Huvudkommunikationen mellan `RControlStation` (laptopen) och `Car_Client` (Raspberry Pi) sker via UDP och TCP. Om denna trafik skickas över ett öppet Wi-Fi-nätverk (ute på ett fält) eller över det publika mobilnätet (4G) utan kryptering, är systemet extremt sårbart för:
* **Eavesdropping (Avlyssning):** En angripare kan läsa av robotens exakta GPS-koordinater, hastighet och sensorstatus i realtid.
* **Man-in-the-Middle (MitM) & Paketinjektion:** En angripare kan skicka falska "drive"-kommandon eller modifiera rutter i luften. Detta skulle kunna få roboten att köra av banan eller styra mot hinder.

### 🛡️ Befintligt skydd & Mitigering: WireGuard VPN
Genom att implementera **WireGuard VPN** (som beskrivs i `Wireguard_Guide.md`) elimineras dessa nätverksrisker nästan helt:
* **Kryptering:** All trafik krypteras med modern och extremt säker kryptografi (ChaCha20-Poly1305).
* **Autentisering:** Endast godkända enheter med unika kryptografiska nycklar (Public Keys) som uttryckligen har registrerats på servern kan ansluta till nätverket. Obehöriga paket kastas omedelbart på nätverksnivå.

### 🔴 Risk: Fysisk åtkomst till USB-portar
Roboten har USB-portar direkt exponerade på utsidan eller under huven (anslutna till Raspberry Pi och styrkortet). 
* **Hot:** Om en obehörig person får fysisk tillgång till roboten kan de ansluta en ST-LINK för att läsa ur eller skriva över firmwaren på STM32-chippet, eller ansluta en USB-enhet till Raspberry Pi för att köra skadlig kod.
* **Rekommendation:** Kapsla in all känslig elektronik i en låst, väderbeständig låda så att fysiska portar inte är åtkomliga utifrån utan verktyg/nyckel.

---

## ⚙️ 3. Fysisk Funktionell Säkerhet (Functional Safety)

Fysisk säkerhet handlar om att förhindra att roboten orsakar person- eller materialskador vid oförutsedda händelser.

### 🛡️ Inbyggd Failsafe: Kommunikationsbortfall (`timeout.c`)
Vad händer om 4G-anslutningen bryts mitt under autopilotdrift? Utan fail-safes skulle roboten fortsätta köra med det senast mottagna gaspådraget (skenande robot).
* **Hur det fungerar i koden:** 
  I `RC_Controller` körs en dedikerad tråd (`timeout_thread` i `timeout.c`). Varje gång ett giltigt styrpaket tas emot via USB anropas `timeout_reset()`, vilket uppdaterar tidsstämpeln `m_last_update_time`.
* **Säkerhetsreaktion:**
  Om ingen giltig uppdatering har tagits emot inom den konfigurerade tiden (t.ex. 2 sekunder, definierat av `heartbeat_maxtime`), reagerar systemet omedelbart:
  1. Autopiloten stängs av direkt (`autopilot_set_active(false)`).
  2. Om robotens hastighet är över 2 km/h ansätts en hård bromsström (`bldc_interface_set_current_brake(m_timeout_brake_current)`).
  3. Statusen sätts till `m_has_timeout = true` vilket låser styrsystemet tills nya giltiga paket tas emot.

### 🔴 Risk: Intern kommunikationskrasch (Pi till STM32)
Det finns två "hjärtan" i roboten. Om operativsystemet på Raspberry Pi hänger sig helt, kommer den sluta skicka paket till STM32-styrkortet. STM32-styrkortets oberoende `timeout.c` kommer då att detektera detta som ett kommunikationsbortfall och bromsa roboten säkert. 

*Men vad händer om STM32-mikrodatorn kraschar eller hänger sig?*
* **Hot:** Om STM32-processorn låser sig i en loop med hög gas ut till motorstyrningen (VESC) kommer den inte att kunna köra `timeout_thread`.
* **Befintligt skydd:** STM32 har en hårdvaru-watchdog (**IWDG - Independent Watchdog**). Om processorn fryser kommer hårdvaran automatiskt att starta om styrkortet inom millisekunder. Vid omstart sätts alla motorutgångar till ett säkert nolläge.

### 🔴 Risk: Sensorbortfall och RTK-hopp
Autopiloten styr blint baserat på GPS-positionen.
* **Hot 1 (RTK-hopp):** Om GPS-mottagaren plötsligt tappar RTK-Fix (t.ex. på grund av träd eller byggnader) och går till "Single" eller "Float", kan den beräknade positionen plötsligt hoppa flera meter i sidled. Autopiloten kommer då att tro att den är utanför banan och göra en våldsam och plötslig styrmanöver för att kompensera.
* **Hot 2 (GPS-bortfall):** Om GPS-data helt slutar strömma, kan roboten tappa orienteringen.
* **Analys av skydd:** Styrsystemet har mjukvaruskydd som begränsar maximalt styrutslag och stoppar autopiloten om positionsnoggrannheten (accuracy) överstiger ett visst tröskelvärde. 
* **Rekommendation:** Säkerställ att parametern för maximal positionsavvikelse (GPS-noggrannhetstolerans) är strikt inställd i `RControlStation` och att autopiloten omedelbart deaktiveras om GPS-mottagaren rapporterar en noggrannhet sämre än 10-20 cm.

---

## 💻 4. Matematiska och Logiska Sårbarheter (Code Integrity)

Detta är mjukvarufel som vi har upptäckt och åtgärdat, men som utgör klassiska risker i autonoma fordon:

### 🛡️ Åtgärdat: Buffertspill i `ublox_send`
* **Risk:** RTCM3-paket från Swepos kopierades till en statisk buffert på endast 1024 bytes utan storlekskontroll. Moderna, fullständiga Swepos-paket (inklusive Galileo och BeiDou-data) kan vara större än så. Detta orsakade ett buffertspill i RAM-minnet (.bss-sektionen) vilket skrev över intilliggande variabler och ledde till slumpmässiga krascher av styrsystemet under drift.
* **Åtgärd:** Vi har ökat bufferten till `2048` bytes samt lagt till en hård storlekskontroll mot buffertspill i `ublox_send` (`len = len > 2048 ? 2048 : len;`).

### 🛡️ Åtgärdat: Division-med-noll och NaN-förstörelse
* **Risk 1 (`autopilot.c`):** Beräkningen `dist_tot / ((float)time / 1000.0)` gjordes före tidsvalideringen, vilket kunde orsaka division-med-noll och krasch/frysning om `time` var `0`.
* **Risk 2 (`utils.c`):** Beräkningen `t = ap_ab / ab2` saknade skydd mot `ab2 == 0`. Om två ruttpunkter låg på exakt samma koordinat blev närmaste punkt `NaN` (Not a Number). Detta `NaN`-värde propagerade genom alla styrsystemets ekvationer och förstörde autopilotens styrvinkelberäkning helt.
* **Åtgärd:** Vi har infört strikta kontroller: tidsvalideringen körs nu först, och i `utils.c` tvingas parametern `t` till `0.0f` om linjesegmentet är punktformat (`ab2 <= 1e-6f`).

---

## 🛑 5. Maskinsäkerhet och Operativa Risker (Fysiska Faror)

Detta är mekaniska och operativa risker som måste hanteras utanför mjukvaran.

### ⚠️ Kollisionsrisk (Avsaknad av hinderdetektering)
* **Risk:** Drängen och Macbot saknar för närvarande LiDAR, ultraljudssensorer eller stereokameror för hinderdetektering. Autopiloten kör blint längs den inspelade rutten. Om en människa, ett djur eller ett fordon blockerar vägen kommer roboten **inte** att stanna själv!
* **Krav för drift:** 
  - Roboten får **aldrig** köras helt obevakad på öppna ytor där människor rör sig.
  - Operatören måste alltid ha fri sikt över roboten och ha möjlighet att ingripa.

### ⚠️ Krav på Mekanisk Nödknapp (E-Stop)
Enligt europeisk maskinsäkerhetsstandard (t.ex. Maskindirektivet) måste alla autonoma maskiner ha en fysisk nödknapp som är:
1. **Lättåtkomlig:** Placerad på utsidan av roboten, röd och svampformad.
2. **Hårdvarubaserad:** Nödknappen får **inte** vara en mjukvaruknapp. Den måste bryta strömmen *fysiskt* direkt till drivstegen/motorstyrningen (VESC) via en kraftig kontaktor eller huvudströmbrytare. Den får inte bara signalera till en mikrodator att "snälla stanna", eftersom mikrodatorn kan ha hängt sig.

---

## 📋 6. Strategiska Säkerhetsrekommendationer

För att maximera säkerheten vid tester och framtida permanent drift rekommenderas följande åtgärder:

1. **Installera en trådlös nödknapp (Radio E-Stop):**
   Utöver den fysiska nödknappen på roboten bör operatören hålla i en trådlös nödstopps-sändare (t.ex. via en dedikerad industriradio eller en vanlig RC-sändare med failsafe) som bryter strömmen till motorerna omedelbart om kontakten bryts eller om knappen trycks in.
2. **Säkra Raspberry Pi internt:**
   Aktivera en brandvägg (t.ex. `ufw`) på robotens Raspberry Pi och konfigurera den att **endast** tillåta trafik över VPN-gränssnittet (`wg0`). Blockera alla inkommande anslutningar på det publika nätverkskortet (`eth0` / `wlan0`) förutom krypterad SSH.
3. **Konfigurera bromsströmmen strikt:**
   Säkerställ att parametern för `m_timeout_brake_current` är tillräckligt hög för att snabbt kunna stoppa roboten på fältet, men inte så hög att roboten sladdar, välter eller skadar mekaniken vid ett plötsligt kommunikationsavbrott.
4. **Inför strikta mjukvarulimits:**
   Sätt låga maxgränser för hastighet och styrvinkel under de första testerna i eftermiddag. Öka dem gradvis först när du har verifierat att fail-safes (som att stänga av Wi-Fi/VPN och se att roboten stannar omedelbart) fungerar klockrent.
