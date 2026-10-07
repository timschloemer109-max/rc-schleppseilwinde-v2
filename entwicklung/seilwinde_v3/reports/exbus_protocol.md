# EX-Bus-/EX-Protokollnachweis

Stand 2026-10-07, Ausgangscommit `dd2a0c857bbce81bc6cc4814a9337ccd09324bdf` plus
reviewbare V3-Quellen auf dem Entwicklungsbranch. Der abschließende Gesamtbericht
nennt den Ergebniscommit. Hier stehen tatsächlich ausgeführte Protokollprüfungen.

| Prüfung | Ergebnis | Nachweis / Grenze |
| --- | --- | --- |
| Native Protokollsuite mit strengen Warnungen | bestanden | 109847 Checks, 0 Fehler; Clang 23.1.3 / llvm-mingw 20261006 UCRT, C++11 |
| Address-/Undefined-Behavior-Sanitizer | bestanden | derselbe ausführbare Test, kein Befund |
| Unabhängige Referenztelegramme | bestanden | Python 3.13.7; 11 Vektoren, CRC16/CRC8, Header-/JSON-Parität |
| Fuzz und Fehlerbehandlung | bestanden | 100000 Bytes, Xorshift32 Seed `0x5345494c`; alle 128 Einzelbitänderungen eines 16-Byte-Kanalframes abgelehnt |
| Fragmentierung / Zeitüberlauf | bestanden | 40/80 µs Bytes, Überlauf bei `0xfffffff0`, unvollständige Frames und Gesamtdeadline |
| Antwortfenster-Berechnung | bestanden | JETIBOX 40 B benötigt 3200 µs bei 125k / 1600 µs bei 250k; 200 µs Guard, Überalterung abgelehnt |
| Echte Application-Telemetrie unabhängig decodiert | bestanden | 15 numerische IDs, 16 Deskriptoren, 320 Anfragen einschließlich ID-/Carousel-Überlauf, Vorzeichen/Skalierung/Sättigung/Gültigkeitsbits |
| JETIBOX-Menütransport/-Darstellung | bestanden | genau 32 Zeichen / 2×16 gepaddet; alte Anfragen und Tasten >2000 µs ignoriert |
| Unabhängiger Testmaster → echte Application/Core | bestanden | 14 ASan-/UBSan-Replayfälle, beide Baudraten, Normal-/Verlust-/Rausch-/Überlasttraffic, Hall/Strom/Endschalter/volle Loggerqueue |
| Reale 125/250-kBaud-/TX-Freigabe-/Antwortlatenz | nicht ausführbar | kein angeschlossener R4/Receiver/Logikanalysator; Werkbankabnahme erforderlich |
| Erkennung konkreter Receiver-Hold-Konfiguration | nicht ausführbar | keine Hold-Markierung im öffentlichen Kanalformat; Receiverkonfiguration am Aufbau prüfen |

## Reproduzieren

Aus Repository-Root, Compiler-Bin auf `PATH` für die Sanitizer-Runtime:

```powershell
$taskCompilerBin = (Resolve-Path 'entwicklung/seilwinde_v3/.build/tools/llvm-mingw-20261006-ucrt-x86_64/bin').Path
$env:PATH = $taskCompilerBin + ';' + $env:PATH
& "$taskCompilerBin\clang++.exe" -std=c++11 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror '-fsanitize=address,undefined' -g -O1 -Ientwicklung/seilwinde_v3/core entwicklung/seilwinde_v3/core/exbus.cpp entwicklung/seilwinde_v3/tests/test_exbus.cpp -o entwicklung/seilwinde_v3/.build/exbus/test_exbus.exe
& entwicklung/seilwinde_v3/.build/exbus/test_exbus.exe
python entwicklung/seilwinde_v3/simulator/exbus_master.py --self-test
python entwicklung/seilwinde_v3/tests/test_simulator.py --sanitize --compiler "$taskCompilerBin\clang++.exe"
```

Die Gesamtbuildbefehle sind in [V3 README](../README.md) verbindlich.

Der unabhängig erzeugte 8-s-Testmaster erreicht mit **unveränderten sicheren
Standardzeiten des Steuerkerns** die RUN-Flanke bei 4026–4062 ms. Der Verlustfall
stoppt bei 4698 ms (125k) beziehungsweise 4704 ms (250k); weiterlaufende
Telemetrie und anschließende RF-Kanalrückkehr mit weiterhin RUN bewirken keinen
Wiederanlauf. In den sechs zusätzlichen Endschalterfällen wird bei 4350 ms die
STOP-Ausgabe im selben injizierten Tick beobachtet (0 ms Modelllatenz).
Die 1-ms-Tickfrequenz und diese virtuelle Latenz sind kein gemessener
Zeitnachweis für RA4M1, UART, ESC oder Motorleistung. Ergebnisse pro Fall liegen
reproduzierbar unter `.build/simulator-sanitized/results.json`.

## Quellen / Implementierungsentscheidung

Primärquellen am 2026-10-07 gelesen:

- [JETI EX Bus Communication Protocol 1.21](https://www.jetimodel.com/support/telemetry-protocol/ex-bus-communication-protocol.html), [PDF](https://www.jetimodel.cz/uloz/c6d00c58-78ab-4c4d-9f15-34e84523479e.pdf/?1626095164=): Rahmen-/Kanal-/JETIBOX-Format, Antwortfenster, CRC und Beispieltelegramme S. 6/7.
- [JETI Telemetry Communication Protocol 1.07](https://www.jetimodel.com/support/telemetry-protocol/jeti-telemetry-communication-protocol.html), [PDF](https://swshop.jetimodel.com/uloz/2132a27b-f704-4ba1-a62e-29194ac9a22f.pdf/?1625830586=): EX-Typen, Textdeskriptoren, Herstellerbereich, CRC8 und Beispiele S. 8.
- [nichtgedacht/JetiExBus](https://github.com/nichtgedacht/JetiExBus), [Lizenz](https://github.com/nichtgedacht/JetiExBus/blob/master/LICENSE), [UART-Backend](https://github.com/nichtgedacht/JetiExBus/blob/master/src/JetiExBusArduinoSerial.cpp): Version 1.0.7, MIT; RA4M1-Backend fehlt. V3 hat eigene Protokollimplementierung und keine Abhängigkeit oder Übernahme aus dieser Library. Arduino/Core/Servo-Lizenzen separat im Zielplattformnachweis prüfen.

EX Bus erhält einen eigenen begrenzten Parser: maximal 64 B, je Byte ein Aufruf,
maximal ein begrenzter Resynchronisationsscan bei Fehler, Interbyte-Timeout 1000 µs
und Gesamtdeadline 6000 µs. Beide Zeitvergleiche funktionieren über 32-Bit-Überlauf.
Die gewählte 6-ms-Grenze reicht für 64 B bei 125k (5120 µs); sie ist eine
V3-Defensivgrenze und kein zusätzliches Versprechen der JETI-Spezifikation.
Unbekannte Blöcke werden kontrolliert übersprungen und ergeben keine RC-Frische.
Kanalwerte werden von Achtelmikrosekunden auf volle µs abgerundet; die Firmware
validiert ihre tatsächlich zugeordneten Steuerkanäle gegen zulässige PWM-Grenzen.
Paket-IDs sind Antwortkennungen; ihre Wiederholung beweist keine neuen RF-Daten.

EX-Nutzdaten sind auf 28 B begrenzt (29 B abzüglich des in EX Bus ausgelassenen
Separators). Datenwerte nutzen zulässige Typen 0/1/4/8 mit direktem Betrag,
Vorzeichen und Dezimalposition. Textdeskriptoren und 32-Zeichen-JETIBOX-Antworten
werden unabhängig von UART und Menüpolitik aufgebaut. Die offiziellen Beispiele
enthüllen ein nichtnull reserviertes EX-Byte in einem Slave-Beispiel; eigene
Ausgaben setzen es auf null, `validExCrc` kontrolliert die CRC und Länge ohne
dieses Beispiel fälschlich abzulehnen.

Der Parser kann aus CRC-gültigen Kanalframes keinen undokumentierten Hold-Status
ableiten. Nach der öffentlichen Spezifikation muss der Master bei fehlenden
RF-Kanaldaten Kanalpakete aussetzen. Eine abweichende Hold-/Failsafewert-Ausgabe
muss die konkrete Receiverkonfiguration ausschließen. Das Alter frischer Kanäle,
Neutralfreigabe und Wiederanlaufschutz bleiben Aufgaben des echten Steuerkerns.

Vorherige Fehler bei der strengen Kompilierung (Integerpromotion-Warnungen) und
die zu strenge Prüfung des reservierten EX-Bytes wurden behoben; die oben genannten
Prüfungen wurden danach erfolgreich wiederholt. Es wurden keine Hardwaremesswerte
erzeugt und keine Flugfreigabe aus den Hosttests abgeleitet.
