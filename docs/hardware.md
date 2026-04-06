# Hardware, Verdrahtung und Schutzbeschaltung

Diese Datei ist der aktuelle Hardware-Stand fuer `Schleppseilwinde_V2_0`.

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

- Der ESC versorgt den Motor aus dem Antriebsakku.
- Nano, Empfaenger, Hall-Sensor und ACS712 brauchen eine stabile 5-V-Versorgung.
- Wenn der ESC ein BEC hat, kann dieses 5 V fuer Nano und Empfaenger liefern.
- Alle Massen muessen verbunden sein: ESC, Nano, Empfaenger, ACS712 und Hall-Sensor.
- Mit dem `Turnigy Plush 30A` ist ein klassischer erster Aufbau gut machbar.
- Die 5-V-Leitung ist im aktuellen Konzept kurz; zusaetzliche Pufferung auf der 5-V-Schiene ist deshalb vorerst nicht vorgesehen.

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
- Wichtig ist vor allem gleichmaessiger Abstand und eine reproduzierbare Flanke am Sensor.
- Damit entstehen in der Grundannahme `2` Hall-Pulse pro Trommelumdrehung.

## Schutzbeschaltung

### ESC-Signal

- Direktverbindung Nano `D4` auf ESC-Signal ist fuer den ersten kurzen Aufbau ok.
- Ein kleiner Serienwiderstand ist optional, aber nicht zwingend vorgesehen.

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

```text
Antriebsakku
  +--------------------> ESC Leistungseingang
  |
  +--------------------> ACS712 Primarseite / Leistungspfad nach Aufbau

ESC Motorausgang ------> Windenmotor

ESC/BEC 5V ------------+-----------------> Nano 5V
                       +-----------------> Empfaenger +5V
                       +-----------------> Hall VCC
                       +-----------------> ACS712 VCC

Gemeinsame Masse ------+-----------------> Nano GND
                       +-----------------> Empfaenger GND
                       +-----------------> ESC GND
                       +-----------------> Hall GND
                       +-----------------> ACS712 GND

Empfaenger CH Speed ---> Nano D3
Empfaenger CH Trigger -> Nano D5
Nano D4 --------------> ESC Signal
Hall OUT -------------> Nano D2
ACS712 OUT -----------> Nano A3
Endschalter ----------> Nano D7 gegen GND
Nano D13 -------------> Status-LED
```

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

1. 5 V am Nano und Empfaenger messen.
2. GND-Verbindungen durchklingeln.
3. Endschalter-Eingang im Monitor pruefen.
4. Hall-Pulse von Hand an der Trommel pruefen.
5. ACS712-Rohwert im Stillstand beobachten.
6. ESC erst ohne Seil und mit kleiner PWM testen.
