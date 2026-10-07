# RC-Schleppseilwinde V3.0

Implementierter Entwicklungsstand für Nano R4 / RA4M1. Maßgebliche Anforderungen:
[software_v3_0.md](../../docs/software_v3_0.md). V2-Firmware bleibt unverändert.
Der Standardbuild liefert ausschließlich STOP-PWM; `MotorBenchQualified=false`.
Hardware-Abnahme steht aus, keine Flugfreigabe. Keine automatischen Uploads.

## Aufbau

| Bereich | Inhalt |
| --- | --- |
| `firmware/Schleppseilwinde_V3_0/` | Sketch, begrenzte SCI2-UART-, GPT-PWM-, ADC- und Flash-BGO-Adapter |
| `core/` | gemeinsame echte Steuerungslogik, Einstellungen, Journal, EX Bus, Menü, CSV |
| `tests/` | native Unit-/Integrations-/Fehlerprüfungen und Logger-Kompatibilität |
| `simulator/` | deterministischer EX-Bus-Testmaster und unabhängig belegte Referenztelegramme |
| `hardware/` | Pinplan, offene Pegel-/Leistungsentscheidungen, Werkbank-Abnahme |
| `reports/` | tatsächliche Ergebnisse, Primärquellen, Ressourcen und Grenzen |
| `.build/`, `artifacts/` | ignorierte Werkzeuge, Sketch-Staging, Binärdateien und Logs |

## Reproduzierbar bauen und testen

Vom Repository-Root, Python 3.13, C++11 mit Clang/GCC:

```sh
python entwicklung/seilwinde_v3/build.py native --compiler clang++ --sanitize
python entwicklung/seilwinde_v3/build.py arduino --install
python entwicklung/seilwinde_v3/build.py arduino --bus-baud 250000
```

Arduino CLI **1.4.1**, Boardcore **arduino:renesas_uno 1.6.0**, FQBN
**arduino:renesas_uno:nanor4**, FSP aus diesem Core. Keine externe EX-Bus-/Servo-
Bibliothek wird benötigt oder in der Firmware verlinkt. Versionsprüfung erfolgt
in `build.py` und `toolchain.json`. `--install` installiert nur den fixierten Core.
`--config-file` erlaubt ein lokales CLI-Datenverzeichnis. Ohne `--install` wird
ein vorhandener Core geprüft und benutzt. Der Build lädt keinen Sketch auf ein Board.

`build.py` kopiert die Firmware und kanonischen `core/`-Quellen in ein frisches
Arduino-Sketch-Verzeichnis `.build/sketch/Schleppseilwinde_V3_0/src/winch/`.
Dadurch werden alle C++-Quellen tatsächlich kompiliert; keine relativen Includes
auf einen vom Arduino-Preprocessor verschobenen Ursprungsordner. Den `.ino`
deshalb nicht allein in der IDE kompilieren. GitHub Actions baut aus sauberem Checkout.

Dieser Windows-Arbeitsordner ist für den alten ARM-GCC des Cores zu lang. Die
verifizierte Lösung ist ein kurzes Laufwerksalias auf denselben Checkout, kein
zweiter Clone. Einen freien Buchstaben verwenden; hier wurde `V:` verwendet:

```powershell
subst V: "C:\Users\Tim\Nextcloud\CAD sciebo\Swiss Trainer\Seilwindenentwicklung\rc-schleppseilwinde-v2"
$env:PATH = 'V:\entwicklung\seilwinde_v3\.build\tools\llvm-mingw-20261006-ucrt-x86_64\bin;' + $env:PATH
python V:/entwicklung/seilwinde_v3/build.py native --compiler V:/entwicklung/seilwinde_v3/.build/tools/llvm-mingw-20261006-ucrt-x86_64/bin/clang++.exe --sanitize
python V:/entwicklung/seilwinde_v3/build.py arduino --arduino-cli "C:\Program Files\Arduino CLI\arduino-cli.exe" --config-file V:/entwicklung/seilwinde_v3/.build/arduino/arduino-cli-short.yaml
```

Das lokale YAML enthält ausschließlich `directories.data/downloads/user` unter
`V:/entwicklung/seilwinde_v3/.build/arduino/`; auf einem anderen Rechner entsprechende
Pfade in einer lokalen Konfiguration wählen. Standardpfade funktionieren in kurzen
Checkouts; Linux-CI braucht kein Alias. Den Alias nach Arbeitsende mit `subst V: /D`
entfernen. Native Windowsprüfung nutzt Address-/UB-Sanitizer; LeakSanitizer ist auf
dieser Windows-Toolchain nicht verfügbar, auf Linux-CI aktiviert.

## Bedienung und sichere Speicherung

JETIBOX zeigt zwei Zeilen mit je 16 Zeichen: **L/R** blättert, **U/D** verändert
einen Entwurf. **Apply RAM** übernimmt validierte Werte nur im Stillstand.
**Save flash** speichert die zuvor angewendeten Einstellungen explizit.
**Discard edits** verwirft Änderungen und holt die aktuellen RAM-Werte.
Kanalnummern im Menü sind 1-basiert, im internen Datensatz 0-basiert; provisorisch
Speed CH1, Drei-Stufen-Trigger CH2, Licht CH3 und Leistung CH4.
Die tatsächliche Zuordnung und frische Kanal-Ausgabe bei RF-Verlust am JETI-Aufbau prüfen.

Flash-Speicherung verwendet zwei getrennte 1024-Byte-Löschblöcke, versionierte
128-Byte-Datensätze, CRC32, Sequenznummer und Commitbyte zuletzt. Der R4-Adapter
schreibt asynchron mit Background Operation, verifiziert den Datensatz und sperrt
währenddessen Motorfreigabe und Menüänderungen. Danach neue Arming-/Neutralfolge.
Identische Werte erzeugen keine Flashschreiblast. Kein automatischer Import alter
V2-EEPROM-Kalibrierwerte. Flashfehler verriegeln die Hardwarefreigabe bis zum Neustart.

LEARN bedeutet **manuelles Ausziehen bei STOP-PWM**. Ein neuer Vollweg wird nur ab
bekannter Endlage, nach Mindestdauer/-pulszahl und innerhalb der Plausibilitätsgrenzen
in RAM übernommen. Anschließend explizit Save. Kurzes Freiziehen verändert den
Lernwert nicht. Ein einzelner Hall-Sensor erkennt keine Richtung: unbeaufsichtigte
Bewegung im Stillstand/Boot ohne Endlage macht die Position unbekannt; RUN startet
dann konservativ SLOW. Meter sind Schätzungen und benötigen eingegebene Nutzseillänge.

## Telemetrie, Diagnose und Adaptergrenzen

EX Bus liefert 16 Deskriptoren und 15 Messwerte in begrenzten Paketen. Numerische
IDs: 1 Restmeter (0,1 m), 2 Restprozent (0,1 %), 3 Zustand, 4 Strom (0,01 A),
5 Hall-Gesamtpulse, 6 Pulsrate, 7 Drehzahl (zwei Pulse/Umdrehung), 8 angeforderte ESC-PWM,
9 Stoppgrund, 10 Gültigkeitsbits, 11 frische RC-Daten, 12 Leistung, 13 Lichtwarnung,
14 Busfehler, 15 verworfene Loggerzeilen. Gültigkeitsbits: 1 Position bekannt,
2 Weg gelernt, 4 Strom kalibriert/gültig, 8 Metermaßstab vorhanden. Meter nur bei
Bits 1+2+8 verwenden. EX-Zähler werden auf 2.097.151 begrenzt; CSV enthält volle
32-Bit-Zähler. Hersteller-/Gerätekennung A400/0300 für diesen Prototyp; bei mehreren
Eigenbaugeräten eindeutige Gerätekennungen vergeben. Kein grafischer Device Explorer.

Öffentliche Kanalframes besitzen kein Hold-Flag. Ein Master mit fortgesetzten
unveränderten Hold-Kanalframes lässt sich damit nicht erkennen; der Empfänger muss
bei RF-Verlust die Kanal-Ausgabe stoppen. CRC-gültige Telemetrie-/Menüframes halten
RC-Daten nicht frisch. Timeout standardmäßig 100 ms, Moduswechsel nach drei gültigen
Kanalframes, Neutralfreigabe 250 ms, Arming 3000 ms.

CSV 3.0 erweitert das V2-23-Spaltenformat um elf Felder. Der passive Pi-Logger
akzeptiert beide Formate; fehlende V2-Felder bleiben leer. `esc_output_us` ist der
Steuerungs-Sollwert, `actual_esc_output_us` der Hardware-Sollwert nach Sperren;
beides keine elektrische Messung. `hardware_inhibited` macht die Standardsperre
sichtbar. Fehlende Stromkalibrierung gibt ein leeres Stromfeld aus. Der 2048-Byte-
Ring verwirft ganze neue Zeilen bei Überlast; USB bedient höchstens 32 Byte je loop,
kein Serial-Warten. Header einmal pro Boot. Ereignisse werden bestmöglich ohne
Warten ausgegeben, Paket-/Logverlust ist an Zählern sichtbar.

uniLIGHT und High-Side-Leistungsschalter haben geprüfte portable Prioritätslogik,
aber ausdrücklich **deaktivierte Hardwareadapter** ohne erfundene Pins/Befehle.
ACS712-Variante, Empfängerport, Schnittstellentreiber, 5-V-Wandler und Leistungsteil
müssen am Aufbau festgelegt werden. Siehe [Hardware](hardware/README.md),
[Testbericht](reports/test_report.md) und [Plattformprüfung](reports/platform_qualification.md).

## Virtuelle Senderverbindung

JETI Studio 1.2.3/DC/DS-24II 6.04 wurde tatsächlich installiert und getestet.
Die [Emulator-Lua-App](simulator/jeti/README.md) verbindet Senderbedienelemente
über Loopback mit der portablen Application und dem realen EX-Bus-Parser.
JETIBOX-Text/decodierte Telemetrie, Endschalter, Failsafe und Wiederanlaufschutz
sind in der GUI geprüft. [Ergebnisse und LCD-Nachweis](reports/jeti_studio_test.md)
grenzen diesen Test von nativer Funk-/EX-Sensor- und R4-Hardwareprüfung ab.

```powershell
python entwicklung/seilwinde_v3/tests/test_jeti_bridge.py --sanitize
python entwicklung/seilwinde_v3/simulator/jeti_bridge.py --log entwicklung/seilwinde_v3/artifacts/jeti-gui-session.jsonl
```

Der Server baut den Hosthelfer, benötigt einen C++-Compiler und bleibt aktiv.
Ctrl+C beendet ihn; kein Hardwarezugriff. Vor ca. 71 Minuten eine neue Sitzung
starten; Host-Zeitsprünge über 1 Sekunde beenden diese Live-Testbrücke.
