# Seilwinde V3.0: ausgeführte Tests und offene Abnahme

Datum: **2026-10-07, Europe/Berlin**. Geprüfter Implementierungscommit:
**`a79fe70e7641ac7b78674e0c9d6521c5e028e396`**, Branch `codex/seilwinde-v3`,
Basis `dd2a0c857bbce81bc6cc4814a9337ccd09324bdf` von main. Die nachfolgenden
Berichtscommits ändern keine Firmwarequellen. Vorhandenen Repository-Checkout
unter Piper J3 wiederverwendet, eigener Worktree im hinterlegten Swiss-Trainer-
Arbeitsordner; kein zweiter Clone. Fremde uncommittierte V2-Arbeit und PR #1
wurden geprüft und erhalten; keine V2-Firmwareänderung, kein automatischer Merge.

## Implementiert und tatsächlich geprüft

| Prüfgruppe | Ergebnis | Tatsächlicher Nachweis |
| --- | --- | --- |
| Steuerung / Sicherheit | **bestanden** | 74.320 Checks: Boot RUN/LEARN, Neutral/neue Flanke, Arming, autonome Fahrt, Endschalter FAST/SLOW/Rampe/Lernen, Wiederanlaufschutz, Kurzstörung/echter Verlust/Signalrückkehr, Laufzeit, Strom+Hall-Stall/Anlaufmaske, hohe Last mit Pulsen, Sensorverlust, Zähler-/Zeitgrenzen, Licht-/Leistungsprioritäten; zusätzlich 12.000 feste-Seed Sicherheitssequenzen |
| Lernen / Position | **bestanden** | Bekannte Nullreferenz erforderlich; vollständiger manueller Auszug, kurze/unplausible Fahrt verworfen, Restweg erhalten, Reboot/ungeklärte Bewegung unbekannt und SLOW, Meterwerte separat als Schätzung/Gültigkeitsbits; kein motorisches Lernen |
| Persistenz | **bestanden** | 261.891 Checks: explizite Bytes, CRC32/Version, leere/alte/korrupten Datensätze, jede Unterbrechungsstelle, Verlust des ganzen inaktiven 1024-Byte-Löschblocks, Sequenz ffffffff→0→1 mit unabhängigem Python/zlib-Vektor, keine Schreiblast bei gleichen Werten, unzulässige Übernahme abgelehnt |
| EX Bus / Parser | **bestanden** | 109.847 Checks, 11 unabhängige Referenztelegramme, Fragmentierung 40/80 µs, CRC/Länge/Rauschen/Timeout, unknown blocks, 100.000 Xorshift32-Fuzzbytes Seed `0x5345494c`, 128 Einzelbitänderungen |
| Telemetrie / JETIBOX | **bestanden** | Unabhängige CRC-/Nutzdaten-Decodierung aller 15 IDs und 16 Deskriptoren über 320 Anfragen; Werte, Vorzeichen, Dezimalen, Sättigung, ID-Echo, Gültigkeitsbits; exakt 2×16 Zeichen, alte Menüanfragen abgelehnt |
| Gemeinsame Application / Menü / CSV | **bestanden** | Gleiche Quellen wie Sketch: nur Kanalframes erneuern RC-Alter, Telemetrie allein löst Failsafe aus, Überlast/volle Loggerqueue und Endschalter, micros/millis-Epochen, Header einmal/34 Spalten/250-ms-Intervall, validierte Menüentwürfe und Apply/Save-Sperre während Flash-Maintenance |
| Echter Testmaster → Application/Core | **bestanden** | 14 deterministische 8-s-Replays bei beiden Baudraten: normal/loss/noise/overload, simulierte Hall-/Endschalter-/Stromeingaben, volle Loggerqueue. RUN mit Default-Arming bei 4026–4062 ms; Kanalverlust stoppt bei 4698/4704 ms; RUN bei Signalrückkehr startet nicht neu |
| Tatsächliche UART-/PWM-Adapterquellen mit API-Spies | **bestanden** | 48 Checks: initiale/100 wiederholt erhaltene 1000-µs-STOP-Pulse, aktiver 1800-µs-Puls unmittelbar LOW, Disable, PRIMASK, RX-Zeitstempel/Ring/Overflow, RX pending verhindert TX, eigenes Echo ignoriert, TX_COMPLETE statt TX_EMPTY, verlorene TEI/Überlauf-Watchdog. Spies sind keine MCU-Emulation |
| Passiver Pi-Loggerparser | **bestanden** | Vier Python-Tests: V2/V3 Normalisierung, Version/Recordtyp, Header/Debug/Teilzeilen. Kein Serialgerät geöffnet |
| Strenge native Warnungen / ASan / UBSan | **bestanden** | Alle fünf nativen Suiten und beide kompilierten Python-Treiber ohne Befund. `-Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wshadow`, C++11; Windows LeakSanitizer nicht verfügbar, Linux-CI aktiviert ihn |
| Vollständiger Nano-R4-Build 125000 | **bestanden** | CLI 1.4.1, Core 1.6.0, echter FSP-/GPT-/ADC-/Flash-ISR-Link; 69.704 Byte Flash / 8.888 Byte globale RAM-Belegung |
| Vollständiger Nano-R4-Build 250000 | **bestanden** | Dasselbe vollständige Firmwareprofil mit fixierter zweiter Baudrate; gleiche Ressourcenbelegung |
| Reale JETI/R4-Pegel als Direktanschluss | **fehlgeschlagen** als Garantienachweis | JETI HIGH≥3,0/LOW≤1,5 V gegenüber R4@5 V HIGH≥4,0/LOW≤1,0 V. Qualifizierter Pegel-/Half-Duplex-Treiber nötig; keine Messwerte erfunden |
| JETI-Studio-GUI/Senderemulator | **nicht ausführbar** | Keine Installation/Prozess gefunden, kein Sender/Empfänger; offizielle Sender-/Lua-Funktionen recherchiert, keine RA4M1-/EX-Bus-Hardwareemulation nachgewiesen |
| Reale R4-/ESC-/Flash-/Versorgungs-/Motorprüfung | **nicht ausführbar** | CLI-Portinventar leer; kein Upload, kein Motorlauf. [Werkbank-Abnahme HW-01…15](../hardware/README.md) vollständig offen |
| GitHub CI | **bestanden** | [Run 37615791297](https://github.com/timschloemer109-max/rc-schleppseilwinde-v2/actions/runs/37615791297) für `c1f2cbdeaeca6ff02eff716418089a4c9618da23`: alle drei Jobs erfolgreich, native ASan/UBSan einschließlich LeakSanitizer unter Ubuntu 24.04 und vollständige Nano-R4-Builds 125000/250000 aus sauberem Checkout |

Die Prüfzahlen sind Assertions/Checks, keine gleichwertigen unabhängigen
Hardwareversuche. 0 ms Endschalterlatenz in den sechs Replayfällen bedeutet
STOP **im selben injizierten 1-ms-Tick**, keine gemessene Echtzeitlatenz.

Reviewbarer Entwicklungsstand: [Draft-PR #2](https://github.com/timschloemer109-max/rc-schleppseilwinde-v2/pull/2).
Der erfolgreiche CI-Commit enthält dieselben Firmware-/Testquellen wie der oben
genannte Implementierungscommit; spätere reine Berichtsänderungen ändern diese
Testergebnisse nicht.

## Reproduzieren / Werkzeuge

Python **3.13.7**, LLVM-MinGW **20261006**, Clang **23.1.3**, Arduino CLI **1.4.1**
(e39419312), Arduino Renesas Core **1.6.0**, darin FSP **4.0.0** und
ARM GCC **7.2.1 20170904**. Keine externe JetiExBus-/Servo-Library verlinkt.
Alle Versionen/Installations- und kurzen Windows-Pfadbefehle:
[README](../README.md), [Plattformnachweis](platform_qualification.md).

Von kurzem Checkout bzw. Alias `V:` wurden tatsächlich ausgeführt:

```powershell
$env:PATH = 'V:\entwicklung\seilwinde_v3\.build\tools\llvm-mingw-20261006-ucrt-x86_64\bin;' + $env:PATH
python V:/entwicklung/seilwinde_v3/build.py native --compiler V:/entwicklung/seilwinde_v3/.build/tools/llvm-mingw-20261006-ucrt-x86_64/bin/clang++.exe --sanitize
python V:/entwicklung/seilwinde_v3/build.py arduino --arduino-cli "C:\Program Files\Arduino CLI\arduino-cli.exe" --config-file V:/entwicklung/seilwinde_v3/.build/arduino/arduino-cli-short.yaml --bus-baud 125000
python V:/entwicklung/seilwinde_v3/build.py arduino --arduino-cli "C:\Program Files\Arduino CLI\arduino-cli.exe" --config-file V:/entwicklung/seilwinde_v3/.build/arduino/arduino-cli-short.yaml --bus-baud 250000
git diff --check
```

Erzeugte lokale Rohlogs: `artifacts/native-tests.log`,
`artifacts/nano-r4-125000.log`, `artifacts/nano-r4-250000.log`;
Replayresultate `.build/simulator-sanitized/results.json`. Generiert und ignoriert,
durch Befehle reproduzierbar. Genau ein kanonischer Core wird ins frische
Arduino-Sketch-Staging kopiert; alle Quellen tatsächlich kompiliert.

Der abschließende Build hat **keine eigenen Firmwarewarnungen**. Die sichtbaren
Core-1.6.0-Warnungen betreffen Const-Discard in Backtrace, Sign-Vergleiche und
ungenutzte I²C/CANFD-Werte sowie TinyUSB-TU_ASSERT-Rückgabe in void-Funktion.
Ursachen geprüft, keine pauschale Warnungsunterdrückung. Ein früherer eigener
Warnbefund zur deaktivierten Stromsensitivität/Division durch Null wurde durch
explizit geprüfte Konversionsfunktion behoben; ADC jetzt ausdrücklich im realen
12-Bit-Modus, statt versehentlich 14-Bit-Rohwerte durch 4095 zu teilen.

## Ressourcen / Timinggrenzen

Beide vollständigen Profile: **69.704 / 262.144 Byte Flash (26,59 %)**,
**8.888 / 32.768 Byte globale RAM-Belegung (27,12 %)**, **23.880 Byte** nominell
für lokale Daten/Stack/Heap verbleibend. Das ist ein Linkerbericht, kein gemessener
Worst-Case-Stack-/Heapnachweis. Lokale `.bin`-SHA256:

| Profil | SHA256 |
| --- | --- |
| 125000 | `353ff983dd14677f0c98d0933b0c153be325e4d6311bef60a4eb7ab3bf4585c2` |
| 250000 | `aca05ea3bc5121f135a68b35eed1bff2b1c38ccba31a1eccc76b7c68b2d7c41d` |

Parser maximal 64 Byte, SCI-RX 127 nutzbare Einträge mit Zeitstempel, TX 64 Byte,
CSV 2048 Byte. Loop bearbeitet höchstens 64 RX-Bytes und beendet Busverarbeitung
nach 350 µs Budget; USB höchstens 32 Byte. ADC einmalige Statusprüfung pro Tick,
Flash-BGO ein Stufenübergang pro Tick und 500-ms-Stufendeadline; Motor während
Save gesperrt. TX nur innerhalb 4-ms-Antwortfenster inklusive 200-µs-Guard und
ohne neuere RX-Daten; verlorene TX-Fertig-IRQ hat endlichen Watchdog.

Endschalterprüfung vor, im und nach Bus-/Nebensystemdienst. Programmziel
**loop <1 ms**, echte D7→Signal-LOW-/gültiger-STOP-Puls-/Motorlatenz noch ungemessen.
1000-µs-STOP-Pulse bleiben auch bei wiederholten Stoppaufrufen gültig. Ein laufender
längerer Puls wird sofort LOW; Reaktion des konkreten ESC auf diese Pulsverkürzung
und die folgende 20-ms-Pulsfolge muss geprüft werden. Interruptjitter, USB-Core-
Treiber-Wartepfade, Flash/Brownout sowie reale Strom-/PWM-Latenz bleiben offen.

## Offene Funktionen und nächste Werkbankschritte

Motorstandardprofil ist **gesperrt**. UniLIGHT und ESC-High-Side-Schalter besitzen
getestete portable Logik, aber benannte deaktivierte Adapter ohne Pin-/Befehlserfindung.
ACS712-Kalibrierung/Sensitivität fehlt; keine angenommene Stallstromschwelle aktiv.
Öffentliche Kanalframes markieren Hold nicht; Empfänger muss bei RF-Verlust
Kanalausgabe stoppen. Grafischer Device Explorer ist nicht Teil der Implementierung.

1. Nano R4, passenden 5-V-Wandler, konkreten Empfängerport/Kanalplan und
   qualifizierten Pegel-/Half-Duplex-Treiber bestimmen. Motorakku ab, Versorgung,
   Quellenrückspeisung, Pegel und beide Baudprofile messen.
2. D4-Boot-/STOP-/Arming-PWM, Endschalterreaktion unter maximaler Bus-/USB-Last,
   Hall-Pulse, ADC-Referenz/ACS712-Variante und Flash-BGO mit Unterbrechungen messen.
3. uniLIGHT-Befehl und externe High-Side-Leistungsstufe auslegen, deaktivierte
   Adapter ersetzen; AUS-Default, Precharge, Einschaltreihenfolge und Arming prüfen.
4. Erst danach beaufsichtigte Seil-/Last-/Stall-/EMV-/Brownout-Tests durchführen,
   Sender-/JETIBOX-/Telemetrie-/Pi-Anzeigen am echten Aufbau bestätigen.

Keine Flugfreigabe aus diesen Softwaretests. Hardwaremessfelder sind absichtlich
leer. Die [Plattformprüfung](platform_qualification.md) und der
[Protokollnachweis](exbus_protocol.md) enthalten Primärquellen und Details.

## Externe Quellen / Lizenzen

JetiExBus 1.0.7 (MIT) wurde geprüft, nicht kopiert/verlinkt. Arduino Renesas Core
1.6.0 hat im Paket eine MIT-Lizenz, enthält aber Komponenten mit eigenen Hinweisen:
TinyUSB MIT, Servo-/EEPROM-Referenz LGPL-2.1-or-later (nicht verlinkt). Die tatsächlich
installierten FSP-4.0.0-Header enthalten Renesas-Nutzungsbedingungen für Renesas-
Produkte; der Zielcontroller ist RA4M1. Keine FSP-Implementierung wurde ins Repo
kopiert. Der vorhandene Repository-Lizenzstatus wird durch diesen Auftrag nicht
stillschweigend geändert. Paket-Lizenzhinweise beim Weitergeben von Buildpaketen erhalten.
