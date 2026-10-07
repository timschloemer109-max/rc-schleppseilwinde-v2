# Hardware, Verdrahtung und Schutzbeschaltung

Diese Datei ist der aktuelle Hardware-Stand fuer `Schleppseilwinde_V2_0`.

V3.0 ist separat für Nano R4 implementiert und baubar. Software-Pinplan und offene reale Messungen: [V3-Hardware](../entwicklung/seilwinde_v3/hardware/README.md). Keine V2-Messwerte als R4-Freigabe übernehmen. Garantierte JETI-Pegel (HIGH ab 3,0 V/LOW bis 1,5 V) passen nicht zu den R4-5-V-Grenzen (VIH ab 4,0 V/VIL bis 1,0 V); geeigneten Schnittstellentreiber festlegen und messen.

## Ziel-Hardware

- Arduino Nano
- ESC mit Servo-PWM-Eingang
- Antriebsmotor fuer die Winde
- ACS712 Stromsensor
- Hall-Sensor `KY-003` mit `A3144`
- `2` Magnete an der Trommel oder einem drehenden Teil
- Endschalter fuer Seil-Endlage
- RC-Empfaenger mit zwei Kanaelen

## Aktueller Referenzstand der Teile

- Hall-Sensor: `KY-003 A3144`
- Magnetanzahl: `2`
- ESC-Referenz: `Turnigy Plush 30A`
- Endschalter: Typ noch offen, von Eigenbau bis Mikroschalter moeglich
- ACS712: genaue Modulbezeichnung noch nachtragen

## Feste Pinbelegung

| Funktion | Nano-Pin | Verdrahtung |
| --- | --- | --- |
| Hall-Sensor Signal | `D2` | Sensor-Ausgang |
| Endschalter | `D7` | Schalter gegen GND |
| RC Geschwindigkeit | `D3` | Signal vom Empfaenger |
| RC Trigger | `D5` | Signal vom Empfaenger |
| ESC Signal | `D4` | Signal zum ESC |
| ACS712 Ausgang | `A3` | Analogausgang vom Sensor |
| Status-LED | `D13` | intern oder extern |

## Stromversorgung

Korrigierter Referenzstand, 2026-10-07:

- Der Windenmotor wird **über den ESC aus einem separaten 2S-LiPo** versorgt (7,4 V nominal, 8,4 V voll).
- **Der ESC-BEC versorgt aktuell nichts.** Seine Plusleitung im Servostecker isolieren; Signal und gemeinsame Masse bleiben angeschlossen.
- Arduino, Hall-Sensor und ACS712 beziehen ihre Versorgung aus der **RC-Anlage**, unabhängig vom Motorakku.
- Die RC-Anlage im Swiss Trainer steht derzeit auf **6,0 V**; eine spätere HV-Umstellung ist noch offen.
- **Ein passender vorgeschalteter Wandler auf stabile 5,0 V für Arduino und Sensorik ist erforderlich.** Konkreter Wandler und Einbau sind noch nicht bestätigt. 6 V/HV nicht direkt an den 5-V-Pin anschließen.
- Eingangsspannungsbereich für aktuelle 6 V einschließlich Einbrüchen und für das später gewählte HV-Maximum prüfen. Ausgangsstrom aus Controller/Sensoren plus Reserve bestimmen; Ripple, Transienten, EMV und Temperatur unter realer Last messen.
- Motor, Servos und Lichtleistung nicht über den Steuerungswandler versorgen. Lokale Entkopplung passend zu Board und Sensoren prüfen.
- RC-Anlage, ESC, Arduino und Sensoren brauchen gemeinsame Masse; Motorstrom nicht über dünne Signalmasse führen.
- Bei USB-/Pi-Anschluss die Rückspeisung zwischen Versorgungen anhand der konkreten Boardbeschaltung prüfen. Kein ungeprüftes Parallelschalten von 5-V-Quellen.

Die Schlepperintegration steht in [SYSTEMS.md](https://github.com/timschloemer109-max/king140-schlepper/blob/main/docs/SYSTEMS.md#5-v-versorgung-der-seilwinde). V3.0/Nano R4 ist softwareseitig [implementiert](../entwicklung/seilwinde_v3/README.md); tatsächliche Verdrahtungs-/Versorgungsabnahme bleibt offen.

### Geplanter RC-Leistungsschalter

Ein High-Side-Schalter soll nur den Pluspfad des ESC/Motors schalten. Steuerung und RC bleiben versorgt. Die Leistungsschaltung für 20–30 A plus Reserve, Treiber, unabhängige RC-Auswertung, Precharge und Einschalt-/Signalrückspeisung sind noch auszulegen. Fail-Safe ist AUS; nach Zuschalten braucht der ESC seine Arming-Sequenz und eine sichere Neutralfreigabe. Keine Low-Side-Trennung der gemeinsamen Signalmasse.

## Empfohlene Verdrahtung

| Von | Nach | Hinweis |
| --- | --- | --- |
| Empfaenger CH Speed Signal | Nano `D3` | nur Signal, GND gemeinsam |
| Empfaenger CH Trigger Signal | Nano `D5` | 3-Stufen-Schalter |
| Nano `D4` | ESC Signal | optional 220 Ohm in Serie |
| ACS712 `OUT` | Nano `A3` | kurz fuehren, Masse sauber |
| ACS712 `VCC` | `+5V` | lokal puffern |
| ACS712 `GND` | `GND` | Sternpunkt bevorzugen |
| Hall `OUT` | Nano `D2` | bei Open Collector mit Pull-up |
| Hall `VCC` | `+5V` | 100 nF direkt am Sensor |
| Hall `GND` | `GND` | verdrillt oder kurz fuehren |
| Endschalter | Nano `D7` zu `GND` | aktuell active-low in Firmware |

## Hall-Sensor und Magnetanordnung

- Verwendet wird ein `KY-003`-Modul mit `A3144`.
- Gestartet wird mit `2` Magneten.
- Die beiden Magnete sollten gleichmaessig um `180 Grad` versetzt sitzen.
- Fuer den aktuellen Aufbau sind die beiden Magnete gleich gepolt montiert.
- Wichtig ist vor allem gleichmaessiger Abstand und eine reproduzierbare Flanke am Sensor.
- Damit entstehen in der Grundannahme `2` Hall-Pulse pro Trommelumdrehung.

## Werkbank-Teststand ohne Endschalter

- Fuer den aktuellen Werkbanktest ist der Endschalter noch nicht montiert.
- Verwendet wird dafuer der separate Sketch `src/Schleppseilwinde_Testbench/Schleppseilwinde_Testbench.ino`.
- Der Test ist nur ohne Seil und unter Aufsicht vorgesehen.
- Der Motor bleibt bis zum seriellen Kommando `ARM` und einer anschliessenden Fahranweisung bei `1000 us`.
- Die serielle Ausgabe erfolgt als CSV mit Strom, Hall-Pulsen und PWM-Wert.
- Eine harte Laufzeitbegrenzung bleibt aktiv.
- Stall wird auch in der Testbench nur aus hohem Strom plus fehlenden Hall-Impulsen erkannt.

## Schutzbeschaltung

### ESC-Signal

- Direktverbindung Nano `D4` auf ESC-Signal ist fuer den ersten kurzen Aufbau ok.
- Ein kleiner Serienwiderstand ist optional, aber nicht zwingend vorgesehen.
- Die Hauptfirmware sendet auch im Stillstand aktiv `1000 us` und trennt das ESC-Signal nicht ab.
- Beim Einschalten haelt die Hauptfirmware den ESC zuerst einige Sekunden auf `1000 us` und gibt `RUN` erst nach stabiler Trigger-Mittelstellung frei.
- Kurze RC-PWM-Aussetzer werden in der Hauptfirmware waehrend `RUN` erst nach `3` aufeinanderfolgenden ungueltigen Reads als `RC_SIGNAL_LOSS` gewertet; saubere Leitungsfuehrung bleibt trotzdem Pflicht.

### Hall-Sensor

- Das `KY-003`-Modul bringt die noetige Grundbeschaltung bereits mit.
- Fuer den ersten Aufbau sind keine zusaetzlichen externen Widerstaende vorgesehen.
- Leitung kurz halten und nicht direkt neben Motor- oder Akkuleitungen fuehren.

### Endschalter

- Direkt auf `INPUT_PULLUP` ist fuer den ersten Stand ok.
- Zunaechst keine zusaetzliche RC-Beschaltung.
- Wichtiger ist eine mechanisch saubere und reproduzierbare Loesung.

### ACS712

- Leistungspfad getrennt von sensiblen Signalleitungen fuehren.
- Analogsignal zu `A3` moeglichst kurz und nicht parallel zur Motorleitung.
- Falls sich der ACS712 im Aufbau als stoeranfaellig zeigt, wird gezielt nur dort nachgebessert.

## Bauteilbegruendungen

- **Hall-Sensor statt Opto**: unempfindlicher gegen Schmutz und Licht.
- **ACS712**: bereits vorhanden und fuer den ersten Stand ausreichend, obwohl spaeter ein praeziserer Sensor moeglich ist.
- **Arduino Nano**: vorhanden, genug I/O, schnell fuer diese Aufgabe.
- **ESC per Servo-PWM**: kompatibel zum bisherigen Aufbau und zum Brushless-Umbau.
- **Turnigy Plush 30A**: bekannter vorhandener Brushless-Regler und deshalb gute Referenz fuer die erste V2-Hardware.

## Schematischer Anschlussplan

| Zweig | Anschlussfolge |
| --- | --- |
| Motorleistung | 2S-LiPo → ACS712-Leistungspfad → ESC → Motor |
| Steuerungsversorgung | RC-Anlage → passender 5-V-Wandler (gefordert, noch auszuwählen) → Arduino + Hall + ACS712-VCC |
| ESC-BEC | unbenutzt; Plus isoliert |
| Masse | RC, Wandler, Arduino, Sensoren und ESC gemeinsam; Leistungs- und Signalrückströme sauber führen |
| Signale | V2.0-Pins gemäß Tabelle oben; V3.0-Pinplan noch zu qualifizieren |

Der High-Side-Leistungsschalter ist eine geplante Ergänzung des Motorzweigs, keine bereits eingebaute Funktion.

## Wichtige Anmerkung zum Schaltplan

Das ist derzeit ein sauberer Verdrahtungs- und Anschlussplan, aber noch kein fertiger E-CAD-Schaltplan mit Symbolen und Footprints. Fuer einen echten KiCad- oder EasyEDA-Schaltplan muessen noch die exakten Teile feststehen:

- genaue ACS712-Modulbezeichnung
- Endschalter-Typ
- Steckverbinder

Mit diesen Angaben kann als naechster Schritt ein echter Schaltplan in KiCad aufgebaut werden.

## Schaltplan-Datei

Ein erster sauberer Schaltplan fuer den Aufbau liegt als SVG hier:

- `docs/schaltplan_v2.svg`

## Erste Hardware-Checks vor Motorlauf

1. RC-Spannung und davon getrennt stabile 5 V hinter dem Wandler am Arduino und den Sensoren messen; ESC-BEC-Plus muss isoliert sein.
2. GND-Verbindungen durchklingeln.
3. Trigger zum Einschalten in Mittelstellung lassen und die normalen ESC-Beeptoene abwarten.
4. Endschalter-Eingang im Monitor pruefen.
5. Hall-Pulse von Hand an der Trommel pruefen.
6. ACS712-Rohwert im Stillstand beobachten.
7. ESC erst ohne Seil und mit kleiner PWM testen.

## Testbench-Kommandos

- `ARM` schaltet den Test frei, bewegt den Motor aber noch nicht.
- `STOP` setzt den ESC sofort auf `1000 us`.
- `SET <us>` haelt einen festen Testwert innerhalb des freigegebenen Testfensters.
- `SWEEP <start> <end> <step> <hold_ms>` faehrt eine langsame PWM-Rampe.
- `STATUS` gibt sofort eine CSV-Statuszeile aus.
- `ZERO` kalibriert den ACS712 im Stillstand neu.
