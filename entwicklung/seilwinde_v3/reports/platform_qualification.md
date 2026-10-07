# Plattformqualifikation Nano R4 und JETI Studio

Datum: 2026-10-07, Europe/Berlin. Referenz-HEAD bei Prüfung: `dd2a0c857bbce81bc6cc4814a9337ccd09324bdf`, ergänzt um die V3-Arbeitsdateien. Finale Firmware-Ressourcen und Testcommit stehen im [Gesamttestbericht](test_report.md). Dieser Bericht unterscheidet Quellcodeprüfung, Compile-/Linknachweis und noch fehlende reale Messung.

## Werkzeuge und tatsächliche Umgebung

| Prüfung | Ergebnis |
| --- | --- |
| Arduino CLI | **bestanden**, 1.4.1, Commit e39419312; vorhandene Installation `C:\Program Files\Arduino CLI\arduino-cli.exe` |
| R4-Core | **bestanden**, lokal unter V3 `.build/arduino`, `arduino:renesas_uno@1.6.0`; FQBN `arduino:renesas_uno:nanor4`; darin Renesas FSP 4.0.0 |
| ARM-Compiler | **bestanden**, offizieller Core-Toolchain `arm-none-eabi-gcc@7-2017q4`, GCC 7.2.1 20170904 |
| Servo-Referenz | **bestanden** als Compile-/Quellcodeprüfung, Arduino Servo 1.3.0; V3 verwendet wegen Startverhalten den eigenen GPT-Adapter |
| Nativer Compiler | **bestanden**, portables LLVM-MinGW 20261006 / Clang 23.1.3, x86_64-w64-windows-gnu; lokal `.build/tools/llvm-mingw-20261006-ucrt-x86_64/bin` |
| Python | Vorhanden: 3.13, `C:\Users\Tim\AppData\Local\Programs\Python\Python313\python.exe` |
| CMake/Ninja auf PATH | Bei Erstprüfung nicht vorhanden; Hostbefehle/CI werden durch das Projektbuildwerkzeug bereitgestellt |
| Hardwareinventar | `arduino-cli board list --json` mit lokaler Konfiguration: `{"detected_ports":[]}`; `[System.IO.Ports.SerialPort]::GetPortNames()` leer. WMI-Abfrage im Sandboxlauf verweigert; daher kein WMI-Hardwarenachweis |
| Reales Flashen / Motorlauf | **nicht ausführbar**, kein verfügbarer R4/JETI-Aufbau; kein Upload oder Motorlauf ausgeführt |
| JETI Studio installiert | **bestanden**, anschließend offizielle Windows-64-Bit-Version 1.2.3 unter V3 `.build/jeti-studio/app` installiert; Studio und Senderemulator tatsächlich gestartet |
| JETI-Studio-GUI-/Lua-/Telemetrietest | **bestanden** für eigene Lua-/Loopback-Brücke: DC/DS-24II 6.04, Lua 5.3/32-Bit-Integer, LuaSocket 3.0-rc1; Sendersteuerung zum echten portablen Core, JETIBOX-Text und EX-Werte angezeigt. [Tatsächlicher GUI-Test](jeti_studio_test.md); native EX-Sensor-/Funk-/R4-Prüfung offen |

LLVM-MinGW wurde vom [offiziellen Projektrelease 20261006](https://github.com/mstorsjo/llvm-mingw/releases/tag/20261006) heruntergeladen. Lokale SHA-256 von `llvm-mingw-20261006-ucrt-x86_64.zip`: `317492c456aa27ee607a5919f1d2d38dcdc1112516a24d0bf4b00d078f52d17a`.

## Compile-/Linknachweis

Minimaler API-Qualifikationssketch mit tatsächlichen V3-UART-/GPT-/Flashadaptern, Hall-Interrupt, D7-Polling, ADC-Auflösungs-API und EEPROM-/Servo-Referenzheadern: **bestanden**, 47.976 Bytes Flash / 5.836 Bytes globale RAM-Belegung, gegen Board-Limits 262.144 / 32.768 Bytes. FSP-Flash-Ready-ISR wurde tatsächlich verlinkt; finaler Referenzlauf nach UART-Watchdog-/Stop-Puls-Korrektur. Diese Zahlen gehören ausschließlich zum kleinen Qualifikationsbuild; sie sind weder finale Firmwarewerte noch Stack-/Heapmessung.

Die **tatsächlichen UART-/PWM-C++-Adapter** wurden zusätzlich mit deterministischen API-/Register-Spies auf dem Host ausgeführt: **48 Prüfungen bestanden**, sowohl ohne Sanitizer als auch mit Address-/Undefined-Behavior-Sanitizer und strengen Compilerwarnungen als Fehler. Abgedeckt: 1000-µs-Initialwert, hundert wiederholte Stop-Aufrufe ohne Kürzen eines gültigen Stop-Pulses, sofortiges Kürzen einer 1800-µs-Fahrt, ungültige PWM-Anforderungen, Abschaltung/alter Callback, ISR-Zeitstempel, PRIMASK-Erhalt, Ringüberlauf, ausstehende RX-Daten/IRQ, Echo-Unterdrückung, TX_DATA_EMPTY gegen TX_COMPLETE und UART-Watchdog einschließlich `micros()`-Überlauf. Diese Spies prüfen die Adapter-Kontrollpfade; sie emulieren weder RA4M1-Ausführungszeit noch Signalform.

```powershell
python entwicklung/seilwinde_v3/tests/test_r4_adapters.py
python entwicklung/seilwinde_v3/tests/test_r4_adapters.py --sanitize
```

Der Referenzsketch wird aus denselben aktuellen Treiberdateien generiert und kompiliert; keine Uploadfunktion:

```powershell
python V:/entwicklung/seilwinde_v3/hardware/qualify_r4.py --config-file V:/entwicklung/seilwinde_v3/.build/arduino/arduino-cli-short.yaml
```

Die erste Kompilierung am vollständigen Nextcloud-Pfad scheiterte mit `stdlib.h: No such file or directory`. Die Datei war vorhanden; GCC 7 ignorierte überlange interne Include-Suchpfade vor `..`-Normalisierung. Derselbe vorhandene Checkout über temporäre `subst V:`-Zuordnung gebaut: erfolgreich. Keine zweite Repositorykopie und keine globale Coreinstallation.

Reproduzierbare lokale Vorbereitung im Repository-Root, V: muss frei sein:

```powershell
subst V: (Get-Location).Path
$base = 'V:/entwicklung/seilwinde_v3/.build/arduino'
New-Item -ItemType Directory -Force "$base/data", "$base/downloads", "$base/user" | Out-Null
@"
directories:
  data: $base/data
  downloads: $base/downloads
  user: $base/user
"@ | Set-Content "$base/arduino-cli-short.yaml"
arduino-cli --config-file "$base/arduino-cli-short.yaml" core update-index
arduino-cli --config-file "$base/arduino-cli-short.yaml" core install arduino:renesas_uno@1.6.0
# Nur für den optionalen Servo-Referenzqualifikationssketch:
arduino-cli --config-file "$base/arduino-cli-short.yaml" lib install Servo@1.3.0
```

Die V3-README und das Buildwerkzeug stellen den Firmwarebuild aus den kanonischen Corequellen bereit. Nach allen lokalen Builds kann die Aliaszuordnung mit `subst V: /D` entfernt werden; die Dateien bleiben im vorhandenen Checkout. Unter Linux/kurzem Windowspfad ist kein Alias erforderlich.

Warnungen mit `--warnings all` wurden geprüft und nicht pauschal unterdrückt: eigene UART-/PWM-/Flashadapter ohne Warnung. Core 1.6.0 meldet Const-Discard in `cm_backtrace.c`, Sign-Vergleiche/ungenutzte I²C-/CANFD-Werte in `IRQManager.cpp`, Sign-Vergleich in `Interrupts.cpp` und TinyUSB-`TU_ASSERT` mit Rückgabewert in einer void-Funktion. Servo 1.3.0 meldet beim Referenzbuild Sign-Vergleiche über `constrain(int,uint32_t,uint32_t)`. Das sind fixierte Fremdquellen; Warnungen bleiben sichtbar. Tatsächliche Laufzeit-/USB-Wartepfade bleiben Teil der Hardwareabnahme.

## Geprüfte R4-Schnittstellen

| Bereich | Code-/Datenblattbefund und Konsequenz |
| --- | --- |
| CPU/Speicher | RA4M1 Cortex-M4, 48 MHz, 256 kB Codeflash, 32 kB SRAM und 8 kB Datenflash; [Arduino Nano R4](https://docs.arduino.cc/hardware/nano-r4/) |
| Pinmux | Core 1.6.0 `variants/NANOR4/variant.cpp`/`pinmux.inc`: D0/P301 und D1/P302 SCI2, D2/P105 IRQ0, D7/P107 ohne IRQ, A3/P002 ADC2, LED_BUILTIN Index22/P204 |
| UART-API | [Core 1.6.0 `Serial.cpp`](https://github.com/arduino/ArduinoCore-renesas/blob/1.6.0/cores/arduino/Serial.cpp): `write()` wartet auf `tx_empty`, `flush()` auf `tx_complete`, `write_raw()` auf TEND; unpassend für begrenzten Steuerpfad. V3 nutzt eigenen FSP-UART ohne diese Warteschleifen |
| TX-Freigabe | [FSP 4.0.0 SCI-UART](https://github.com/renesas/fsp/blob/v4.0.0/ra/fsp/src/r_sci_uart/r_sci_uart.c): `TX_DATA_EMPTY` bedeutet letzte Byteübertragung läuft noch; TEI löst `TX_COMPLETE` aus. V3 gibt TX erst dann über TE/PFS frei und unterdrückt eigenes Echo durch RX-Abschaltung während TX |
| ISR-Daten | RX-Ring mit 127 nutzbaren Einträgen und `micros()`-Zeitstempel, Fehler-/Overflowepoch, PRIMASK-erhaltende atomare Mainzugriffe; reale IRQ-/Timestamp-/Freigabelatenz ungemessen |
| ADC | [Core `analog.cpp`](https://github.com/arduino/ArduinoCore-renesas/blob/1.6.0/cores/arduino/analog.cpp) verwendet im Arduino-Standardpfad 14-Bit-Konversion plus Ausgabeskalierung; `analogRead()` hat Scan-Warteschleife. V3 wählt im eigenen FSP-Adapter ausdrücklich **reale 12-Bit-Konversion** vor `R_ADC_Open`, passend zu 0…4095, und verteilt Scanstart/status/read über Schleifendurchläufe. [FSP 4.0.0 `r_adc.c`](https://github.com/renesas/fsp/blob/v4.0.0/ra/fsp/src/r_adc/r_adc.c) erlaubt für ADC14 sowohl 12 als auch 14 Bit; ohne diese Wahl wäre die Stromskalierung um Faktor vier falsch. Stromkalibrierung bleibt gesperrt |
| Servo/GPT | [Servo 1.3.0 Renesas](https://github.com/arduino-libraries/Servo/blob/1.3.0/src/renesas/Servo.cpp): `attach()` setzt erst DEFAULT_PULSE_WIDTH=1500 µs und startet Timer, vorangestelltes `writeMicroseconds()` ohne Attach ist wirkungslos. V3 nutzt zwei begrenzte GPT-Intervalle mit sicherem 1000-µs-Initialwert, sofortigem LOW-Stopp und separater Wartungsabschaltung |
| Datenflash | [Core EEPROM](https://github.com/arduino/ArduinoCore-renesas/blob/1.6.0/libraries/EEPROM/src/EEPROM.h) und `virtualEEPROM` können bei einzelnen Byteänderungen ganze 1024-Byte-Blöcke synchron löschen/schreiben. V3 verwendet eigene RAM-Staging-/BGO-Persistenz; keine Arduino-EEPROM-Schreibaufrufe im Steuerpfad |
| Asynchroner Flash | [FSP 4.0.0 Flash LP](https://github.com/renesas/fsp/blob/v4.0.0/ra/fsp/src/r_flash_lp/r_flash_lp.c): BGO mit `FCU_FRDYI`, Datenflashblock 1024 Bytes, Programmierung 1 Byte; Generic IRQ im Core verfügbar. Commit-Byte separat zuletzt, Rücklesen erst nach P/E-Ende, 500-ms-Stufenwatchdog, niedrigere IRQ-Priorität14 |
| Elektrische EX-Bus-Eignung | **nicht bestanden als Direktanschluss-Garantienachweis**: RA4M1 normaler RX-Eingang benötigt HIGH ≥4,0 V / LOW ≤1,0 V bei VCC=5 V, JETI garantiert ≥3,0 V / ≤1,5 V. Ein 2,4-kΩ-Widerstand löst das nicht. Qualifizierten Pegel-/Half-Duplex-Treiber vor realem Anschluss festlegen |

Die direkten FSP-/Registeradapter sind bewusst auf Nano R4 und Core 1.6.0 begrenzt. Ein Update des Cores erfordert neue Quellcodeprüfung, Build und reale Timingabnahme. Kein vorhandenes JetiExBus-Renesas-Backend wurde behauptet oder als Bibliothek eingebunden; der portable V3-Protokollcode ist eine getrennte eigene Implementierung.

Lizenzprüfung anhand der installierten/verlinkten Primärquellen: Arduino-Core/UART/EEPROM-Referenz LGPL-2.1-or-later, Arduino Servo allgemeine Bibliothek LGPL-2.1-or-later und Renesas-Servo-Backend MIT; das fixierte FSP enthält Renesas-Nutzungsbedingungen für Renesas-Produkte. Keine Fremdtreiberquellen wurden ins Repository kopiert. Die lokalen Werkzeugpakete/Qualifikationsreferenzen bleiben ignorierte Buildartefakte; die Firmware benötigt keine externe Servo-/JetiExBus-Bibliothek.

## JETI Studio: belegte Funktionen und Testgrenzen

Die am 2026-10-07 gelesene [offizielle JETI-Studio-Seite](https://www.jetimodel.com/support/jeti-studio/jeti-studio.html) bietet Version 1.2.3 für Windows 64 Bit an und beschreibt DC/DS-24 sowie DC/DS-24II Senderemulator, Lua-Verwaltung und `.log`-/`.jml`-/Echtzeit-Telemetrieoberflächen. Das [offizielle Lua-Apps-Projekt](https://github.com/JETImodel/Lua-Apps) bestätigt die Sender-/Lua-Testbasis. Nach der Erstprüfung wurde Studio auf ausdrücklichen Benutzerwunsch heruntergeladen, installiert und die virtuelle Verbindung tatsächlich geprüft.

Der [GUI-Testbericht](jeti_studio_test.md) belegt Lua-Senderbedienung, 32-Zeichen-JETIBOX-Menü und unabhängig decodierte EX-Werte über unsere Hostbrücke, einschließlich Endschalter/Failsafe/Wiederanlaufschutz. Diese Werte erscheinen in der App-Formansicht; keine native Sensorinjektion in `system.getSensors()`. Reale Sender-Warnschwellen, Sprachwarnungen und vom Zielboard empfangene Telemetrie sind weiterhin am belegten Empfänger-/Interfaceweg zu prüfen. LuaSocket ist ein dokumentierter Emulatorweg, kein Nachweis einer elektrischen Verbindung zur Firmware.

Weder RA4M1-Ausführung noch vollständige EX-Bus-Empfängeremulation sind in diesen Primärquellen als Funktion nachgewiesen. Studio-„EX Bus devices configuration“ ist kein Nachweis für eine frei implementierbare grafische Device-Explorer-Schnittstelle. Der V3-Testmaster prüft daher das öffentliche Kanal-/Telemetrie-/JETIBOX-Protokoll und die portable Steuerlogik unabhängig; physisches UART-Timing, Pegel, Servo-PWM, Flash-BGO, Brownout und EMV bleiben auf der [Werkbankliste](../hardware/README.md).
