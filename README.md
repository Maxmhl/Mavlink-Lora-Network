# MAVLink-LoRa-Mesh-Netzwerk (EU868)

Komplettlösung für ein 868-MHz-LoRa-Mesh, das heterogene Daten über ein
gemeinsames Funknetz transportiert: **MAVLink-Drohnentelemetrie**,
**Wetterdaten** und **generische Payloads** — Ende-zu-Ende verschlüsselt,
mit reinen Weiterleitungs-Routern und einem Windows-Tool zum Flashen und
Konfigurieren.

```
Drohne (ArduPilot) ── UART ── Node (Heltec V3/T-Beam)
                                   ╲  LoRa 868 MHz (verschlüsselt)
                    Router (SenseCAP P1, Solar) ── leitet nur weiter
                                   ╱
PC (Mission Planner) ── USB-COM ── MAVLink-Gateway (T-Beam 1W)
```

## Hardware-Rollen

| Gerät | Board | Rolle |
|---|---|---|
| SenseCAP Solar Node P1 | XIAO ESP32-S3 + Wio-SX1262 | **Router** — Store-and-Forward-Flooding, kein PSK, liest nicht mit |
| Heltec V3 | ESP32-S3 + SX1262 | **Node** — sendet/empfängt konfigurierbare Datentypen |
| LilyGo T-Beam 1W | ESP32-S3 + SX1262 (1 W PA) + GNSS | **Node** oder **MAVLink-Gateway** (USB-COM-Port für die GCS) |
| LilyGo T-Beam v1.1/v1.2 | ESP32 + SX1262 + GPS | zusätzlich unterstützt |

Alle Boards laufen mit **einem gemeinsamen Firmware-Image** pro Board-Typ;
die Rolle wird per Konfiguration (NVS) über das Windows-Tool gesetzt.

## Kernkonzepte

- **Routing:** Managed Flooding mit Dedup über `(src, pkt_id)`, Hop-Limit
  (Default 3) und SNR-gewichtetem Rebroadcast-Jitter. Nodes in Reichweite
  empfangen sich direkt — Router sind nur Reichweitenverlängerung.
- **Verschlüsselung:** AES-128-GCM Ende-zu-Ende zwischen Nodes (Nonce aus
  `src + pkt_id`, Header als AAD, 4-Byte-Tag, Replay-Fenster). Router leiten
  nur Ciphertext weiter und erhalten den PSK nie. Details: [docs/protokoll.md](docs/protokoll.md)
- **MAVLink-Tunnel:** transparenter bidirektionaler Passthrough — FC-UART →
  Mesh (fragmentiert + verschlüsselt) → Gateway-USB → Mission Planner/QGC
  als normaler serieller Port. Kein Zusatzprotokoll.
- **EU868-Konformität:** Subband-Tabelle mit Duty-Cycle-Token-Bucket in der
  Firmware und Live-Validierung im Tool. Default 869,525 MHz (10 %-Subband,
  bis 27 dBm ERP). Details: [docs/funkparameter.md](docs/funkparameter.md)
- **Fernverwaltung:** Ein USB-Node dient als Funk-Brücke — Discovery per
  Broadcast-Ping, Status/Konfiguration/Neustart/Werksreset aller Geräte über
  das Mesh. Verwaltungsverkehr nutzt einen **separaten Admin-PSK**, den auch
  Router erhalten (Nutzdaten bleiben für Router unlesbar).
- **OLED-Bedienung am Gerät:** Heltec V3 und T-Beam zeigen Statusseiten
  (Übersicht/Funk/Netzwerk) auf dem Display; per Ein-Tasten-Menü lassen sich
  Nodes direkt neu starten oder ausschalten (T-Beam: PMU-Shutdown, Heltec:
  Deep-Sleep mit Tastenweckung).
- **Serial-Monitor im Tool:** Live-Log jedes per USB angeschlossenen Geräts
  (Bootbanner, Heartbeat, Events) zur Funktionskontrolle.

## Repository

```
firmware/            PlatformIO-Projekt (Arduino/ESP32, RadioLib)
  include/variants/  Pin-Maps je Board
  src/{radio,mesh,crypto,apps,console,config,board}/
tools/config-tool/   Windows-Tool (Python + PySide6 + esptool)
docs/                Inbetriebnahme, Protokoll, Funkparameter
```

## Download (ohne Python/PlatformIO)

Unter **[Releases](https://github.com/Maxmhl/Mavlink-Lora-Network/releases)**
liegen fertige Downloads (per GitHub Actions gebaut):

- `LoRaMeshKonfigurator.exe` — das Windows-Tool, einfach starten, keine
  Installation. (SmartScreen-Warnung beim ersten Start: „Weitere
  Informationen“ → „Trotzdem ausführen“ — die exe ist nicht code-signiert.)
- `firmware-merged-<board>.bin` — die vier Firmware-Images, direkt im Tab
  „Flashen“ verwendbar.

## Schnellstart

```bash
# 1. Firmware bauen (alle Boards)
cd firmware && pio run

# 2. Windows-Tool starten
cd tools/config-tool && pip install -r requirements.txt && python -m loramesh_tool

# 3. Pro Gerät: Tab "Flashen" -> Tab "Konfiguration" (Rolle, PSK, Topics)
```

Ausführliche Anleitung inkl. Mission-Planner-Einrichtung:
**[docs/inbetriebnahme.md](docs/inbetriebnahme.md)**
