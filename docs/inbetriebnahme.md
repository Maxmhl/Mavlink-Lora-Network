# Inbetriebnahme

## 0. Voraussetzungen

- Windows-PC mit Python ≥ 3.10, USB-Treiber für CP210x/CH9102 (meist
  automatisch), Mission Planner oder QGroundControl
- Firmware-Images: entweder selbst bauen (`cd firmware && pio run`) oder die
  `firmware-merged.bin` aus einem Release verwenden. Die Images liegen nach
  dem Build unter `firmware/.pio/build/<env>/firmware-merged.bin` mit
  `<env>` ∈ `heltec_v3`, `tbeam_1w`, `tbeam_v12`, `sensecap_p1`.
- Windows-Tool starten:

```bash
cd tools/config-tool
python -m venv venv && venv\Scripts\activate
pip install -r requirements.txt
python -m loramesh_tool
```

## 1. Geräte flashen (Tab „Flashen")

1. Gerät per USB anschließen, COM-Port wählen (⟳ aktualisiert die Liste).
2. Board-Profil wählen (bestimmt den esptool-Chiptyp):
   Heltec V3 / T-Beam 1W / T-Beam v1.x / SenseCAP P1.
3. `firmware-merged.bin` des passenden Environments wählen → **Firmware
   flashen**. (SenseCAP P1 / XIAO: falls der Port nicht erscheint,
   BOOT-Taste halten und USB einstecken.)
4. Für einen kompletten Neuanfang (löscht Konfiguration + PSK):
   **Flash löschen**.

## 2. Netzwerk planen

- **Zwei Schlüssel pro Netz** (Tab „Konfiguration", je einmal **Generieren**,
  sicher ablegen — Passwortmanager):
  - **Netzwerk-PSK** (Nutzdaten): auf **alle Nodes und Gateways** schreiben —
    **nicht auf Router** (das Tool sperrt das Feld bei Routern).
  - **Admin-PSK** (Fernverwaltung): auf **alle Geräte inklusive Router**
    schreiben. Ohne Admin-PSK ist ein Gerät nicht fernverwaltbar.
- **Node-IDs:** eindeutig vergeben (z. B. 1 = Gateway, 10+ = Drohnen,
  100+ = Sensoren). Die automatisch aus der MAC abgeleitete ID kann
  übernommen werden.
- **Funkparameter:** Default (869,525 MHz, SF7/125 kHz, CR 4/5, 14 dBm,
  Sync 0x2B) ist für Telemetrie richtig; muss auf allen Geräten identisch
  sein. Details/Grenzwerte: [funkparameter.md](funkparameter.md).

## 3. Geräte konfigurieren (Tab „Konfiguration")

Pro Gerät: verbinden („Verbinden & Auslesen" — das Tool resettet das Gerät
und fängt das Konfigfenster ab), Werte setzen, **Auf Gerät schreiben**
(speichert in NVS und startet neu).

| Gerät | Rolle | Topics senden | Topics empfangen | Besonderes |
|---|---|---|---|---|
| SenseCAP P1 | `router` | — | — | kein PSK; `hops` = Netz-Default |
| Drohnen-Node (z. B. Heltec V3 an ArduPilot-TELEM) | `node` | `mavlink` (+ `position` optional) | `mavlink` | FC-UART-Baud = TELEM-Baud (57600); Peer = Node-ID des Gateways |
| Wetter-Node | `node` | `weather` | — (oder `weather` zum Mithören) | BME280 an I2C wird automatisch erkannt, sonst Simulationsdaten (Flag) |
| PC-Gateway (T-Beam 1W) | `mavlink_gateway` | — | (automatisch `mavlink`) | Peer = Node-ID des Drohnen-Nodes oder 65535 (Broadcast) |

**Verkabelung Drohnen-Node ↔ Flight Controller:** FC-TELEM-TX → Node-RX,
FC-TELEM-RX → Node-TX, GND–GND. Default-Pins pro Board stehen in
`firmware/include/variants/*.h` (z. B. Heltec V3: TX 45/RX 46) und sind per
`mav_rx_pin`/`mav_tx_pin` änderbar. In ArduPilot: `SERIALx_PROTOCOL = 2`
(MAVLink 2), `SERIALx_BAUD = 57`.

**Wichtig für die Funkkapazität:** SF7/125 kHz transportiert ≈ 5 kbit/s —
die Standard-Streamraten von ArduPilot sind zu hoch. Auf der Drohne die
`SRx_*`-Parameter reduzieren (z. B. `SR1_EXT_STAT=1`, `SR1_POSITION=2`,
`SR1_ATTITUDE=2`, Rest 0) oder in Mission Planner die "Telemetry Rates"
niedrig einstellen.

## 4. Funktionstest (Tab „Monitor")

Einen beliebigen Node verbinden und Monitor starten: Die Nachbartabelle
zeigt alle gehörten Geräte mit RSSI/SNR; `weitergeleitet` zählt auf dem
Router hoch, `Auth-Fehler` > 0 deutet auf uneinheitliche PSKs hin.
(Der Monitor pausiert bei einem Gateway den MAVLink-Passthrough —
vor GCS-Nutzung Monitor stoppen.)

## 4b. Fernverwaltung (Tab „Fernverwaltung")

Ein beliebiger per USB angeschlossener Node mit Admin-PSK dient als
**Funk-Brücke** — damit lassen sich alle Geräte im Mesh (auch die
solarbetriebenen Router auf dem Dach) aus der Ferne verwalten:

1. USB-Node wählen → **Brücken-Node verbinden**.
2. **Netzwerk scannen**: Broadcast-Ping; alle Geräte mit Admin-PSK antworten
   (Node-ID, Rolle, FW, Empfangsqualität aus Gerätesicht, Uptime, Akku).
3. Gerät in der Tabelle auswählen → **Status**, **Konfig lesen**,
   **Konfig ändern…** (Funkparameter mit EU868-Prüfung; Gerät startet danach
   automatisch neu), **Neustart** oder **Werksreset** (Bestätigung nötig).

**Achtung:** Falsch gesetzte Funkparameter (Frequenz/SF/Sync) trennen das
Gerät vom Netz — danach hilft nur USB vor Ort. Schlüssel (PSK/Admin-PSK)
sind aus Sicherheitsgründen **nicht** über Funk änderbar, ein Werksreset
löscht sie mit.

## 5. Mission Planner / QGroundControl anbinden

1. Gateway (T-Beam 1W, Rolle `mavlink_gateway`) per USB an den PC.
2. Tab „MAVLink-Gateway" → **Geräte identifizieren** → der markierte
   COM-Port ist der Telemetrie-Port. **Danach das Tool schließen oder
   sicherstellen, dass kein Tab den Port offen hält** — die GCS braucht
   Exklusivzugriff.
3. **Mission Planner:** oben rechts den COM-Port wählen, Baud **115200**
   (bei nativem USB-CDC egal, 57600 geht genauso), **Connect**. Der
   Parameter-Download dauert über LoRa deutlich länger als über Funkmodems —
   Geduld oder Timeout hochsetzen.
4. **QGroundControl:** Q-Symbol → Anwendungseinstellungen → Komm-Verbindungen
   → Hinzufügen → Typ „Seriell", Port + 115200 Baud → Verbinden.
5. Rückkanal testen: Parameter ändern oder „Fly to here" — Kommandos laufen
   über den COM-Port zurück durchs Mesh zur Drohne.

## 6. Fehlersuche

| Symptom | Ursache / Abhilfe |
|---|---|
| Kein Konfig-Handshake | Gateway: Reset-Taste drücken und sofort „Verbinden" klicken (2-s-Fenster); anderes Programm hält den Port offen |
| `radio=FAIL` im Bootbanner / Monitor | SX1262 nicht erreichbar — Board-Profil/Environment vertauscht? Richtiges Image geflasht? |
| Nachbarn sehen sich nicht | Frequenz/SF/BW/Sync-Word ungleich; Antenne fehlt |
| `Auth-Fehler` steigt | PSK ungleich zwischen den Nodes |
| MAVLink verbindet nicht | Peer-Node-ID falsch; FC-Baud ≠ `mav_baud`; RX/TX vertauscht; auf dem Gateway `rx_topics` manuell überschrieben |
| Telemetrie stockt / `tx_drop_duty` steigt | Duty-Cycle-Budget erschöpft — `SRx_*`-Raten senken, ggf. SF7/250 kHz |
| Gerät antwortet nicht auf Scan/Fernzugriff | Admin-PSK fehlt oder ungleich; Gerät außer Reichweite (Hop-Limit prüfen); Brücken-Node ohne Admin-PSK |
