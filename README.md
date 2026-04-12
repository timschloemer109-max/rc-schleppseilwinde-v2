# RC-Schleppseilwinde V2.0

Dieses Projekt entwickelt die Steuerung einer RC-Schleppseilwinde fuer Modellflugzeuge weiter. Der belastbare neue Arbeitsstand ist die erste Hardware-Version `Schleppseilwinde_V2_0` im Ordner `src/`.

## Zielbild

- harte Abschaltung ueber Endschalter
- Blockadeerkennung nur aus Strom plus fehlender Drehbewegung
- umdrehungsbasierte Umschaltung von schnell auf langsam
- Lernfunktion fuer Seillaenge mit EEPROM-Speicherung
- Bedienung weiter ueber 3-Stufen-Schalter am Sender

## Projektstand

- `seilwinde_1.2.ino` bleibt Referenz fuer den Altstand
- `winde_controller_v2.ino` bleibt Entwurfsstand
- `src/Schleppseilwinde_V2_0/Schleppseilwinde_V2_0.ino` ist die neue Arbeitsbasis
- `src/Schleppseilwinde_Testbench/Schleppseilwinde_Testbench.ino` ist der separate Werkbank-Sketch fuer ESC-, Hall- und ACS712-Tests ohne RC-Empfaenger

## Aktueller Referenz-Hardwarestand

- Hall-Sensor: `KY-003` mit `A3144`
- Magnete: `2` Stueck an Trommel oder drehendem Teil
- ESC-Referenz: `Turnigy Plush 30A`
- Stromsensor: `ACS712`, genaue Modulvariante noch nachtragen
- Endschalter: noch offen, bisher auch einfache Eigenbau-Loesung moeglich

## Empfohlene Ordnerstruktur

```text
seilwinde_2.0/
|- README.md
|- AGENTS.md
|- docs/
|  |- hardware.md
|  |- decisions.md
|  `- todo.md
|- src/
|  `- Schleppseilwinde_V2_0/
|     `- Schleppseilwinde_V2_0.ino
|  `- Schleppseilwinde_Testbench/
|     `- Schleppseilwinde_Testbench.ino
|- seilwinde_1.2.ino
`- winde_controller_v2.ino
```

## Festgelegte Pinbelegung fuer V2.0

| Funktion | Arduino Nano | Hinweis |
| --- | --- | --- |
| Hall-Sensor | `D2` | Interrupt-Eingang |
| Endschalter | `D7` | `INPUT_PULLUP`, aktuell active-low ausgelegt |
| RC Geschwindigkeit | `D3` | PWM-Eingang vom Empfaenger |
| RC Trigger 3-Stufen | `D5` | PWM-Eingang vom Empfaenger |
| ESC Signal | `D4` | Servo-PWM zum ESC |
| ACS712 | `A3` | Strommessung |
| Status-LED | `D13` | Fehler-/Statusanzeige |

Die Verdrahtung, Schutzbeschaltung und der schematische Anschlussplan stehen in `docs/hardware.md`. Gemessene Werkbankwerte und Kalibrierdaten stehen in `docs/calibration.md`.

## Firmware-Stand V2.0

Die neue Firmware basiert bewusst auf dem V2-Entwurf, nicht auf dem Altcode. Gegenueber dem Vorschlag wurden fuer den ersten Hardwarestand zusaetzlich festgezogen:

- RC-Signalverlust fuehrt nicht versehentlich in `RESET/LEARN`
- Stall-Erkennung bleibt vorbereitet, Strom wird bis zu belastbaren Seiltests aber nur beobachtet
- ACS712 wird beim Einschalten auf Nullpunkt eingemessen
- Hall-Impulse werden per Interrupt gezaehlt
- Lernen wird nur bei plausibler Mindestdauer und Mindestpulszahl uebernommen
- langsame Endphase ist aktuell fest auf `1180 us` gesetzt

Fuer den aktuellen Zwischenstand gilt:

- `RUN_FAST` nutzt weiter den RC-Speed-Eingang
- `RUN_SLOW` nutzt einen festen Wert von `1180 us`
- Stromwerte werden im Debug-Monitor ausgegeben, loesen aber noch keinen Fehler aus
- die maximale Laufzeit der Hauptfirmware ist fuer den aktuellen Seilaufbau voruebergehend auf `60000 ms` gesetzt

## Werkbank-Testbench

Fuer den aktuellen Aufbau ohne RC-Empfaenger und noch ohne Endschalter gibt es einen separaten Test-Sketch:

- `src/Schleppseilwinde_Testbench/Schleppseilwinde_Testbench.ino`

Der Sketch ist bewusst nur fuer beaufsichtigte Werkbanktests gedacht:

- ESC-Ausgang auf `D4`, Hall-Sensor auf `D2`, ACS712 auf `A3`
- serielle Steuerung mit `115200` Baud
- CSV-Telemetrie fuer `PWM`, Strom, Pulse, Pulsrate und grobe Drehzahl
- keine RC-Logik, keine Lernlogik und kein Ersatz fuer den fehlenden Endschalter
- harte Laufzeitbegrenzung und Stall-Stopp nur aus hohem Strom plus fehlenden Hall-Impulsen
- gemessene Anlauf- und Drehzahldaten werden in `docs/calibration.md` gesammelt

Unterstuetzte Befehle:

- `ARM`
- `STOP`
- `SET <us>`
- `SWEEP <start> <end> <step> <hold_ms>`
- `STATUS`
- `ZERO`
- `HELP`

## Erste Inbetriebnahme

1. Verdrahtung nach `docs/hardware.md` aufbauen.
2. Endschalterfunktion ohne Motor pruefen.
3. Hall-Sensor und Impulszaehlung im Seriellen Monitor pruefen.
4. ACS712-Nullpunkt im Stillstand kontrollieren.
5. ESC zuerst ohne Seil und mit kleiner PWM testen.
6. Erst danach Lernfahrt fuer die Seillaenge machen.

## Erste Inbetriebnahme der Testbench

1. RC-Empfaenger abgesteckt lassen und den Aufbau nur fuer Hall, ACS712 und ESC verdrahten.
2. Testbench-Sketch auf den Nano laden.
3. Seriellen Monitor oder Codex mit `115200` Baud verbinden.
4. Banner, CSV-Kopfzeile und plausiblen ACS712-Nullpunkt pruefen.
5. Hall-Pulse zunaechst von Hand mit den zwei Magneten an der Trommel pruefen.
6. Erst danach den Motorstrom anschliessen und mit kleinen `SET`-Schritten anfangen.

## Dokumentationsregel

`README.md`, `AGENTS.md` und die Dateien in `docs/` sind gemeinsam der aktuelle Projektstand. Wenn Pinbelegung, Sicherheitslogik, Verdrahtung oder Lernlogik geaendert werden, muessen diese Unterlagen mit gepflegt werden.
