# ESP32-Logger fuer PWM-Stoersuche

Dieses Paket dient nur der Diagnose von PWM-Stoerungen. Es ist kein Teil der Flug- oder Sicherheitskette der Winde.

## Ziel

- Arduino-Ausgang `D4 -> ESC` passiv mit einem ESP32 mitschneiden
- Pulsbreitenfehler, fehlende Pulse und Timeouts bei laufendem Verbrennungsmotor aufzeichnen
- CSV-Daten ueber USB-Serial direkt am PC speichern

Der Startpunkt ist bewusst ein einzelner Messkanal. Die Firmware ist intern schon so aufgebaut, dass spaeter weitere Kanaele ergaenzt werden koennen.

## Paketinhalt

- `esp32_rc_pwm_logger/esp32_rc_pwm_logger.ino`: ESP32-Arduino-Sketch
- `log_serial_to_csv.py`: Mitschnitt der seriellen CSV-Ausgabe in Datei
- `README.md`: Aufbau, Verdrahtung und Testablauf

## Hardware-Aufbau

### Aktueller Messpunkt

- zu messendes Signal: `Arduino D4 -> ESC`
- ESP32 misst das Signal nur passiv parallel mit

### Spannungsteiler

Fuer das 5- bis 5.5-V-Signal wird ein einfacher Spannungsteiler verwendet:

- oberer Widerstand: `10k`
- unterer Widerstand: `10k`

Damit gilt ungefaehr:

- `5.0 V -> 2.5 V`
- `5.5 V -> 2.75 V`

Das liegt sicher unter `3.3 V` und reicht ueblicherweise fuer die High-Erkennung am ESP32.

### Verdrahtung

```text
Arduino D4 / ESC-Signal ---- 10k ----+---- ESP32 GPIO 18
                                     |
                                    10k
                                     |
ESP32 GND ---------------------------+---- gemeinsame Masse
```

Wichtig:

- GND vom Windenaufbau und GND vom ESP32 muessen verbunden sein
- Signal moeglichst kurz fuehren
- Messleitung nicht parallel zu Zuendkabeln verlegen

## Firmware-Verhalten

Der ESP32 misst pro Puls:

- `pulse_us`: High-Pulsbreite in Mikrosekunden
- `period_us`: Zeit seit dem letzten Rising Edge
- `time_us`: Zeit seit Start des ESP32

Der Logger markiert Pulse nur als auffaellig, verwirft sie aber nicht.

CSV-Header:

```csv
time_us,channel,pulse_us,period_us,state
```

`state`:

- `OK`
- `OUT_OF_RANGE`
- `TIMEOUT`

## Standard-Konfiguration

- Baudrate: `115200`
- aktiver Messpin: `GPIO 18`
- Kanalname in CSV: `D4`
- Timeout ohne neuen Rising Edge: `50000 us`
- gueltiger Pulsbereich fuer Markierung: `800 .. 2200 us`

Wenn ein anderer ESP32-Pin verwendet werden soll, wird nur die Kanal-Konstante im Sketch angepasst.

## Build

Beispiel mit `arduino-cli` und generischem ESP32-Target:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32 tools/esp32-logger/esp32_rc_pwm_logger
```

## Mitschnitt am PC

Abhaengigkeit:

```powershell
pip install pyserial
```

Beispiel ohne Host-Zeitstempel:

```powershell
python tools/esp32-logger/log_serial_to_csv.py --port COM7 --baud 115200 --out logs\esp32_pwm_motor_aus.csv
```

Beispiel mit lokalem Host-Zeitstempel:

```powershell
python tools/esp32-logger/log_serial_to_csv.py --port COM7 --baud 115200 --out logs\esp32_pwm_motor_an.csv --host-time
```

## Empfohlener Testablauf

1. ESP32 per USB an den PC anschliessen.
2. Spannungsteiler an das Signal `D4 -> ESC` anschliessen.
3. Gemeinsame Masse herstellen.
4. Firmware flashen.
5. Python-Logger starten.
6. Je eine Messung aufnehmen:
   - Motor aus
   - Motor an Leerlauf
   - Motor an mit Gas
7. CSVs spaeter vergleichen:
   - Ausreisser in `pulse_us`
   - unplausible `period_us`
   - `TIMEOUT`

## Spaetere Erweiterungen

- zweiter Kanal fuer `RC Trigger`
- dritter Kanal fuer `RC Speed`
- einfache Statistik ueber Jitter und Timeouts
- zusaetzliche Spannungsmessung

