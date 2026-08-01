# LoRa-Mesh Konfigurator (Windows-Tool)

Desktop-Tool (Python + PySide6) zum Flashen und Konfigurieren der Mesh-Geräte.

## Fertige .exe herunterladen

Die einfachste Variante: `LoRaMeshKonfigurator.exe` von der
[Releases-Seite](https://github.com/Maxmhl/Mavlink-Lora-Network/releases)
herunterladen und starten — keine Python-Installation nötig. Die exe wird
vom GitHub-Actions-Workflow (`.github/workflows/build.yml`) mit PyInstaller
auf einem Windows-Runner gebaut. Beim ersten Start meldet sich ggf.
SmartScreen („Weitere Informationen“ → „Trotzdem ausführen“), da die Datei
nicht code-signiert ist.

## Installation aus dem Quellcode

```bash
cd tools/config-tool
python -m venv venv
venv\Scripts\activate          # Windows
pip install -r requirements.txt
python -m loramesh_tool
```

## Als .exe paketieren (optional)

```bash
pip install pyinstaller
pyinstaller --noconsole --name LoRaMeshKonfigurator -p . loramesh_tool/__main__.py
```

## Tabs

1. **Flashen** — `firmware-merged.bin` (aus `firmware/.pio/build/<env>/`)
   per esptool auf das Gerät schreiben (Offset 0x0, Chip laut Board-Profil).
2. **Konfiguration** — Rolle (Node / Router / MAVLink-Gateway), Node-ID,
   LoRa-Parameter mit EU868-Live-Validierung, Topics, MAVLink-Peer und
   Netzwerk-PSK (Generieren + auf Gerät schreiben; Router erhalten keinen PSK).
3. **Monitor** — zyklisches Status-Polling: Nachbartabelle (RSSI/SNR),
   Airtime-Budget, Paketzähler.
4. **MAVLink-Gateway** — identifiziert alle angeschlossenen CLMESH-Geräte und
   zeigt den COM-Port des Gateways für Mission Planner / QGroundControl an.
3. **Serial-Monitor** — Live-Ausgabe des Geräts (Bootbanner, Logs,
   JSON-Events, 30-s-Heartbeat) mit Zeitstempeln, Einfärbung, Log-Export und
   Eingabezeile für Kommandos — zum Prüfen, ob die Firmware korrekt läuft.
   (Die weiteren Tabs verschieben sich entsprechend auf 4–6.)
6. **Fernverwaltung** — ein USB-Node als Funk-Brücke: Netzwerk-Discovery
   (Broadcast-Ping), Status, Konfiguration lesen/ändern (mit EU868-Prüfung),
   Neustart und Werksreset entfernter Geräte — auch der Router. Benötigt den
   Admin-PSK auf Brücke und Zielgerät.

## Tests

```bash
pip install -r requirements-dev.txt
pytest tests/
```
