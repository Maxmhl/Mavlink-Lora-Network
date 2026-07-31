# Funkparameter & EU868-Konformität

Rechtsgrundlage in Deutschland: Allgemeinzuteilung der BNetzA für SRD
(863–870 MHz, basierend auf ETSI EN 300 220). Die Firmware erzwingt die
Grenzwerte selbst (Tabelle in `firmware/src/radio/duty_cycle.cpp`), das
Windows-Tool validiert live bei der Eingabe.

## Subbänder (vereinfachte, konservative Tabelle)

| Subband | Duty-Cycle | max. ERP | Bemerkung |
|---|---|---|---|
| 863,0 – 868,0 MHz | 1 % | 14 dBm (25 mW) | |
| 868,0 – 868,6 MHz | 1 % | 14 dBm (25 mW) | klassisches LoRaWAN-Band |
| 868,7 – 869,2 MHz | 0,1 % | 14 dBm (25 mW) | sehr knappes Budget |
| **869,4 – 869,65 MHz** | **10 %** | **27 dBm (500 mW)** | **Default: 869,525 MHz** |
| 869,7 – 870,0 MHz | 1 % | 14 dBm (25 mW) | |

Frequenzen außerhalb dieser Bereiche (inkl. der Lücken, z. B. 868,6–868,7)
lehnt Firmware wie Tool ab.

## Duty-Cycle-Durchsetzung

Token-Bucket je Gerät: Kapazität = Duty-Cycle × 1 h Airtime, Nachfüllrate =
Duty-Cycle-Anteil kontinuierlich. Vor jedem TX wird die Paket-Airtime
(RadioLib `getTimeOnAir`) gegen das Budget geprüft; ohne Budget wird das
Paket verworfen und gezählt (`tx_drop_duty` im Status/Monitor-Tab).

## TX-Leistung und der 1-W-PA des T-Beam 1W

- Die Firmware begrenzt die konfigurierbare Leistung auf das ERP-Limit des
  gewählten Subbands (27 dBm nur im 10-%-Band, sonst 14 dBm).
- **Achtung T-Beam 1W:** Das Board hat einen externen PA (bis 32 dBm am
  Modulausgang). Antennengewinn und PA-Verstärkung gehen in die ERP ein —
  der Betreiber ist dafür verantwortlich, dass die abgestrahlte Leistung
  die 500 mW ERP im 10-%-Band nicht überschreitet. Im Zweifel TX-Leistung
  niedriger konfigurieren. Betrieb mit >500 mW ERP ist in DE ohne
  Amateurfunk-Zulassung (und dann nur in Amateurbändern) nicht zulässig.
- Niemals ohne Antenne senden (PA-Schaden).

## Empfohlene Profile

| Profil | SF/BW | Brutto-Rate | Nutzung |
|---|---|---|---|
| **Telemetrie (Default)** | SF7 / 125 kHz | ≈ 5,5 kbit/s | MAVLink-Streams (reduzierte Raten, siehe Inbetriebnahme) |
| Schneller Link | SF7 / 250 kHz | ≈ 11 kbit/s | kurze Distanz, mehr MAVLink-Durchsatz |
| Reichweite | SF9–SF10 / 125 kHz | 0,9–1,8 kbit/s | Sensorik, keine Echtzeittelemetrie |

Alle Geräte eines Netzes müssen in Frequenz, SF, BW, CR **und Sync-Word**
(= Netzwerk-ID, Default 0x2B) übereinstimmen.
