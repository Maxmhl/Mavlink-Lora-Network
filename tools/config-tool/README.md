# LoRa-Mesh Konfigurator (Windows-Tool)

Desktop-Tool (Python + PySide6) zum Flashen und Konfigurieren der Mesh-Geräte.

## Installation

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

## Tests

```bash
pip install -r requirements-dev.txt
pytest tests/
```
