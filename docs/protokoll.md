# Protokoll: Paketformat, Routing, Verschlüsselung

Referenzimplementierung: `firmware/src/mesh/packet.h`,
`firmware/src/crypto/crypto.cpp`, Python-Spiegel in
`tools/config-tool/loramesh_tool/protocol.py` (durch Tests gegeneinander
fixiert).

## 1. On-Air-Paketformat (Version 1)

Alle Mehr-Byte-Felder Little-Endian. Maximale LoRa-Framelänge: 255 Byte.

```
Header (11 Byte, Klartext — Router müssen ihn zum Weiterleiten lesen):
┌──────┬───────────┬──────────────────────────────────────────────┐
│ Byte │ Feld      │ Bedeutung                                    │
├──────┼───────────┼──────────────────────────────────────────────┤
│ 0    │ ver_flags │ Bit 7-4: Version (1)                         │
│      │           │ Bit 0: verschlüsselt · Bit 1: Fragment       │
│      │           │ Bit 2: want_ack (reserviert)                 │
│ 1    │ topic     │ Datentyp, siehe unten                        │
│ 2-3  │ src       │ Absender-Node-ID (uint16)                    │
│ 4-5  │ dst       │ Ziel-Node-ID, 0xFFFF = Broadcast             │
│ 6-9  │ pkt_id    │ uint32, monoton je Node (Nonce-Bestandteil)  │
│ 10   │ hop_limit │ Bit 3-0: verbleibende Hops                   │
└──────┴───────────┴──────────────────────────────────────────────┘
Payload:  Ciphertext (bzw. Klartext ohne PSK, nur für Bring-up)
Trailer:  4 Byte gekürzter AES-GCM-Tag (nur bei Verschlüsselung)
```

**Topics** (Topic-ID = Bitposition in den Sende-/Empfangs-Bitmasken):

| ID | Topic | Payload |
|---|---|---|
| 0x01 | `mavlink` | rohe MAVLink-v1/v2-Frames (ggf. fragmentiert) |
| 0x02 | `weather` | 10 B binär: int16 T×100 °C, uint16 rF×100 %, uint16 p×0,1 hPa, uint16 Batt mV, uint16 Flags (Bit 0 = simuliert) |
| 0x03 | `position` | 14 B binär: int32 lat/lon ×1e7, int16 Alt m, uint8 km/h, uint8 Kurs/2° |
| 0x04 | `status` | reserviert |
| 0x05 | `admin` | Fernverwaltung (JSON), verschlüsselt mit **separatem Admin-PSK** |
| 0x10–0x1F | `generic` | opak, anwendungsdefiniert (Typ-Byte-Prinzip = Topic-ID) |

## 2. Routing: Managed Flooding

- **Dedup:** LRU-Tabelle (64 Einträge) über `(src, pkt_id)` auf jedem Gerät.
  Duplikate werden verworfen.
- **Weiterleitung:** Router (und Nodes mit `relay=true`) senden jedes neue
  Paket mit `hop_limit − 1` erneut, sofern `hop_limit > 0` und das Paket
  nicht unicast an sie selbst adressiert war. Vor dem Rebroadcast wartet
  das Gerät einen SNR-gewichteten Jitter (schlechter Empfang → früher
  senden): entfernte Relays wiederholen zuerst, das maximiert den
  Reichweitengewinn pro Hop und entzerrt Kollisionen.
- **Zustellung:** Ein Paket wird lokal zugestellt, wenn `dst == eigene ID`
  oder `dst == 0xFFFF` und das Topic in der Empfangs-Bitmaske abonniert ist.
  Router stellen nie zu und besitzen keinen Schlüssel.
- **Direktverbindung:** Zwei Nodes in Funkreichweite empfangen einander
  unmittelbar — Router sind für den Pfad nicht erforderlich.
- **Warum Flooding statt Distance-Vector/AODV:** Bei Netzen dieser Größe
  (< 20 Knoten) und hochdynamischer Topologie (fliegende Drohnen) kostet
  Routing-Signalisierung mehr Airtime als sie spart; Flooding mit Hop-Limit
  und Dedup ist zustandslos, robust gegen Topologiewechsel und bewährt.

## 3. Verschlüsselung (Ende-zu-Ende)

**AES-128-GCM** mit netzweitem Pre-Shared Key (16 B):

```
Nonce (12 B) = src (2 B LE) ‖ pkt_id (4 B LE) ‖ 6 × 0x00
AAD          = Header-Bytes 0–9   (hop_limit ausgenommen — wird von
               Routern dekrementiert)
Tag          = GCM-Tag, auf 4 B gekürzt (Airtime-Overhead nur 4 B)
```

- **Nonce-Eindeutigkeit:** `pkt_id` ist streng monoton, wird alle 1024
  Pakete ins NVS persistiert und bei jedem Boot um 1024 erhöht — ein Reboot
  kann daher nie eine Nonce wiederverwenden. Jedes Fragment ist ein eigenes
  Paket mit eigener `pkt_id`/Nonce (Verschlüsselung **nach** der
  Fragmentierung).
- **Integrität/Authentizität:** GCM-AEAD; da der Header AAD ist, fällt auch
  jede Manipulation von `src`, `dst`, `topic` oder `pkt_id` auf.
- **Replay-Schutz:** je Quelle Sliding-Window (höchste gesehene `pkt_id` +
  32-Bit-Bitmap); wiederholte oder zu alte IDs werden verworfen. Zusätzlich
  filtert die Dedup-Tabelle Kurzzeit-Replays.
- **Router ohne Schlüssel:** Router leiten das Paket byte-identisch (bis auf
  `hop_limit`) weiter und können Payloads nicht lesen. Der PSK wird ihnen
  vom Tool nie übertragen.
- **Schlüsselverwaltung:** Der PSK wird im Windows-Tool generiert
  (`secrets.token_hex`), per USB-Konfigprotokoll übertragen und im NVS
  gespeichert — nie im Quellcode. Härtungsoption für Produktion:
  ESP32-Flash-Encryption + NVS-Encryption per eFuse aktivieren (einmalig,
  irreversibel — siehe Espressif-Doku); ohne diese Option ist der PSK bei
  physischem Zugriff auf ein Node-Gerät auslesbar.

**Referenzvektor** (fixiert in `tests/test_crypto_vectors.py`):

```
PSK        000102030405060708090a0b0c0d0e0f
Header     topic=0x02 src=0x0011 dst=0xFFFF pkt_id=42 hops=3
Nonce      11002a000000000000000000
AAD        11021100ffff2a000000        (ver_flags=0x11: v1 + encrypted)
Plaintext  "hello mesh"
CT‖Tag     69ee6643de79a4edc339b4d0f1a2
```

## 4. Fragmentierung (MAVLink)

MAVLink-v2-Frames können bis 280 B lang sein, in einen LoRa-Frame passen
240 B Payload. Nachrichten > 240 B werden fragmentiert; jedes Fragment trägt
im (verschlüsselten) Payload einen 2-Byte-Header:

```
[0] msg_seq   — identisch für alle Fragmente einer Nachricht
[1] frag_idx << 4 | frag_total   (max. 15 Fragmente ⇒ 3570 B je Nachricht)
```

Reassembly beim Empfänger: 3 parallele Slots, Schlüssel `(src, msg_seq)`,
Timeout 5 s. Der MAVLink-Framer schneidet den UART/USB-Bytestrom vorher an
Frame-Grenzen (Magic 0xFE/0xFD + Längenfeld), sodass fast alle Frames
unfragmentiert bleiben; Nicht-MAVLink-Bytes werden nach 100 ms Idle
transparent durchgereicht.

## 5. Konfigprotokoll (USB-Serial, 115200 Baud, JSON-Lines)

```
+++CLMESH-CFG+++                       Handshake (Gateway: nur im 2-s-Bootfenster)
{"cmd":"get"}                          Konfiguration lesen
{"cmd":"set","role":"router",...}      Teil-Update, wird in NVS gespeichert
{"cmd":"set","psk":"<32 hex>"}         PSK setzen ("" löscht)
{"cmd":"status"}                       Live-Status inkl. Nachbartabelle
{"cmd":"send","topic":16,"dst":65535,"hex":"..."}   generisches Paket senden
{"cmd":"reboot"} {"cmd":"factory"} {"cmd":"exit"}
```

Antworten sind einzeilige JSON-Objekte mit `"ok":true/false`; asynchrone
Ereignisse tragen einen `"evt"`-Schlüssel (`weather`, `position`,
`generic`, `admin`), Log-Zeilen beginnen mit `#`.

Zusätzlich für Fernverwaltung über einen USB-Brücken-Node:

```
{"cmd":"remote","dst":34,"data":{"acmd":"status"}}
```

## 6. Fernverwaltung (Admin-Topic 0x05)

Jedes Gerät — **auch Router** — führt eine AdminApp aus, die Verwaltungs-
kommandos über Funk entgegennimmt. Damit Router verwaltbar sind, ohne
Nutzdaten lesen zu können, wird Admin-Verkehr mit einem **zweiten,
unabhängigen 16-B-Schlüssel** (Admin-PSK) verschlüsselt — gleiches
AES-128-GCM-Schema wie in Abschnitt 3, nur anderer Schlüssel:

| Schlüssel | Nodes/Gateways | Router |
|---|---|---|
| Daten-PSK (Topics ≠ 0x05) | ✔ | ✘ nie |
| Admin-PSK (Topic 0x05) | ✔ | ✔ |

**Ablauf:** Das Windows-Tool sendet `{"cmd":"remote","dst":N,"data":{...}}`
an einen per USB angeschlossenen Node (Funk-Brücke). Dieser verschlüsselt
das innere JSON mit dem Admin-PSK und flutet es als `TOPIC_ADMIN`-Paket ins
Mesh. Das Zielgerät antwortet an die Absender-ID; die Brücke reicht die
Antwort als `{"evt":"admin","src":N,"data":{...}}` an das Tool durch.

**Kommandosatz** (`acmd` im inneren JSON):

| acmd | Adressierung | Wirkung |
|---|---|---|
| `ping` | Unicast + **Broadcast** | Discovery: Rolle, FW, Uptime, Akku, RSSI/SNR aus Gerätesicht |
| `status` | Unicast + Broadcast | wie USB-`status` (Zähler, Airtime, Nachbarn) |
| `get` | Unicast + Broadcast | Konfiguration lesen (nie Schlüsselmaterial) |
| `set` | **nur Unicast** | Teil-Update wie USB-`set`, aber `psk`/`admin_psk` werden abgelehnt |
| `reboot` | **nur Unicast** | Antwort wird gesendet, Neustart ~2,5 s später |
| `factory` | **nur Unicast** | NVS-Löschung inkl. Schlüssel + Neustart |

**Schutzmechanismen:** GCM-Authentifizierung mit Admin-PSK (kein gültiges
Kommando ohne Schlüssel fälschbar), Replay-Sliding-Window und Dedup wie bei
Nutzdaten, Broadcast-Antworten zufällig über 100–1500 ms verteilt
(Kollisionsvermeidung bei Discovery). Schreibkommandos verlangen Unicast.
**Bewusste Grenzen:** Schlüssel sind nur per USB setzbar (kein Remote-
Lockout/Key-Rollover über Funk), kein OTA-Firmware-Update.
