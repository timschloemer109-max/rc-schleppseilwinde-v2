# Seilwinden-Software V3.0: Anforderungen und Übergabe

Stand: 2026-10-07. Diese Datei ist der Entwicklungsauftrag aus den bisherigen Wünschen und Erkenntnissen; sie beschreibt **keine bereits implementierte V3.0**. Der geprüfte Firmwarestand bleibt V2.0. Der Repositoryname bleibt zunächst unverändert.

Der ausführbare Codex-Auftrag steht in [codex_v3_0_auftrag.md](codex_v3_0_auftrag.md). Vor Entwicklungsbeginn immer den aktuellen Branch, Code und [AGENTS.md](../AGENTS.md) prüfen.

## Verbindlich entschieden

- Die nächste Softwaregeneration heißt **V3.0**.
- Endschalter, maximale Laufzeit, Neutralfreigabe und Schutz vor unbeabsichtigtem Wiederanlauf bleiben erhalten.
- Hoher Strom allein ist kein Blockadefehler; eine Blockade benötigt hohen Strom **und** fehlende Hall-Impulse, mit geprüfter Anlaufmaske.
- Kurze Freizieh-/Resetbewegungen dürfen den gelernten Seilweg nicht überschreiben.
- Der Logger bleibt passiv; sein Ausfall darf die Steuerung nicht beeinflussen.
- Der ESC-BEC versorgt aktuell nichts. Steuerung und Sensorik werden aus der RC-Anlage versorgt.
- Der Motorleistungspfad läuft aus einem separaten **2S-LiPo über den ESC** (7,4 V nominal, 8,4 V voll).
- Im Schlepper muss der RC-Versorgung ein passender Wandler auf **stabile 5,0 V für die Windensteuerung** nachgeschaltet werden. Der Wandler ist gefordert, sein Einbau und konkreter Typ sind noch nicht bestätigt.

## Geprüfter V2.0-Ausgangspunkt

Referenz bei Erstellung: main, Commit `cebe1604e8cf8fc3bb3d0b493ee5742a7ae6b3e8`, Sketch `src/Schleppseilwinde_V2_0/Schleppseilwinde_V2_0.ino`.

| Bereich | Geprüfter Stand / Bedeutung für V3.0 |
| --- | --- |
| Controller | klassischer Nano / ATmega328P; 16 MHz, 32 KB Flash, 2 KB SRAM, 5-V-Logik |
| Hall | D2, Interrupt; zwei Magnete / zwei Pulse je Umdrehung |
| Endschalter | D7, active-low; bleibt vorrangige Abschaltbedingung |
| RC-Eingänge | D3 Speed, D5 Trigger; zwei blockierende `pulseIn()` mit je bis zu 25 ms Timeout |
| ESC | D4; Stopp 1000 µs, langsame Endphase derzeit 1280 µs |
| FAST/SLOW | FAST über RC-Speed; Umschaltung derzeit bei etwa 87 % des gelernten Wegs |
| Einschalten | ESC-Arming mit Stopp-PWM; stabile Neutralstellung und neue RUN-Flanke erforderlich |
| Laufzeit | derzeit 60000 ms; für neuen Aufbau prüfen, weiterhin begrenzen |
| Strom | ACS712 an A3, nur Beobachtung; Variante und Schwellen noch kalibrieren |
| Lernen | EEPROM, Glättung; Mindestdauer/-pulse und Plausibilität gegen bisherigen Wert |
| RC-Störung | RUN: drei aufeinanderfolgende ungültige PWM-Lesungen lösen Fehler aus |
| Serial | 115200 Baud, Text-Debug; das in der Logger-Doku beschriebene Hauptfirmware-CSV ist im geprüften Sketch noch nicht umgesetzt |
| Neue Funktionen | kein EX Bus, kein JETIBOX-Konfigurationsmenü, kein uniLIGHT-Ausgang und kein ESC-Leistungsschalter in diesem Sketch |

Zwei PWM-Timeouts können zusammen etwa 50 ms vor der nächsten Endschalterprüfung blockieren. Das muss V3.0 durch begrenzte, nicht blockierende Verarbeitung lösen. Ein schnellerer Controller allein beseitigt diese Ursache nicht. Flash-/RAM-Auslastung und maximale Reaktionszeit wurden für V3.0 noch nicht gemessen.

## Geplante Funktionen

| Wunsch | Umsetzung / Abgrenzung |
| --- | --- |
| JETI EX Bus | Kanaldaten empfangen und Telemetrie über eine Verbindung zurücksenden; getrennte Speed-/Trigger-PWM-Eingänge nach erfolgreicher Migration ersetzen |
| Senderbedienung | vorhandene Drei-Stufen-Bedienung und autonomes Einziehen nach Rückkehr zur Mitte erhalten; Kanalzuordnung zentral konfigurierbar |
| Telemetrie | geschätzte Seilposition / Restlänge, Restprozent, FAST/SLOW, Strom, Hall-Pulse/Pulsrate, Trommeldrehzahl, ESC-PWM, Stopp-/Fehlergrund und Gültigkeitsstatus |
| Einstellen an DS-24 | Nutzseillänge, FAST-Anteil, Geschwindigkeiten, Warnschwelle; später Rampen sowie kalibrierte Strom-/Stallzeiten |
| Seilende-Lichtwarnung | Arduino als Signalverarbeitung zwischen RC-Anlage und uniLIGHT untersuchen; Lichtsignal durchreichen und definierte Warnung auslösen |
| JETI-Sprachwarnung | Schwellen und Ansagen vorzugsweise am Sender konfigurieren; keine zwingende Lua-App oder sicherheitskritische Senderlogik |
| Antrieb | feinere PWM-Rampen / Softstart, unter Beibehaltung sofortiger Stoppbedingungen |
| RC-Leistungsschalter | separaten High-Side-Schalter für ESC/Motorzweig vorbereiten, mit Fail-Safe AUS und eigener Einschalt-/Arming-Sequenz |
| Diagnose | versioniertes CSV über USB für vorhandenen passiven Pi-Logger, begrenzte Ausgaberate und Puffer |
| Einstellungen | versionierter, validierter persistenter Datensatz; Grenzen und Defaults; Schreiben nur bei zulässiger expliziter Übernahme im Stillstand |

Ein zusätzliches aktiv angetriebenes Seilführungsservo ist derzeit **nicht** gefordert. Die zuletzt besprochene genutete, vom Seil passiv bewegte Führung und der federnde Riemenspanner sind mechanische Forschungsaufgaben und benötigen zunächst keine neue MCU-Leistung.

## Controllerbewertung

| Kandidat | Ressourcen und 5-V-Eignung | Bewertung |
| --- | --- | --- |
| vorhandener Nano / ATmega328P | 16 MHz, 32 KB Flash, 2 KB SRAM; USB und Hardware-UART teilen denselben Kanal | für bisherige Regelung ausreichend; EX Bus plus Menü, Logger und Erweiterungen benötigen genaue Speicher-/Timingprüfung |
| Nano Every / ATmega4809 | bis 20 MHz, 48 KB Flash, 6 KB SRAM, 5-V-Logik | Zwischenlösung; ebenfalls Portierungsarbeit, begrenzter Zugewinn |
| **Nano R4 / RA4M1** | Cortex-M4 mit FPU, 48 MHz, 256 KB Flash, 32 KB SRAM, 8 KB Datenflash; 5-V-GPIO; Qwiic separat 3,3 V | bevorzugter Entwicklungskandidat mit deutlicher Reserve; EX-Bus-Portierung und Hardwarequalifikation noch offen |

Nano R4 ist ein **Kandidat, kein bereits eingebauter oder qualifizierter Controller**. Bei R4 lassen sich USB-`Serial` und Hardware-`Serial1` auf D0/D1 getrennt nutzen. GPIO-Stromgrenzen, Timer/Servo, Interrupts, ADC-Auflösung und Persistenz müssen anhand des verwendeten Arduino-Cores geprüft werden. Die ACS712-Umrechnung muss zur tatsächlichen ADC-Auflösung und Referenz passen. Ausgänge für Leistungsstufen brauchen passende Treiber.

Die geprüfte Bibliothek [nichtgedacht/JetiExBus](https://github.com/nichtgedacht/JetiExBus), Version 1.0.7, führt Teensy 3.x, ATmega32U4, ESP32, SAMD21 und ATmega328PB als Plattformen. Ein Renesas-RA4M1-Backend ist dort nicht vorhanden; ATmega328PB ist nicht der vorhandene ATmega328P. `architectures=*` ist kein Kompatibilitätsnachweis. R4-UART, Interrupt-/Timingcode und Half-Duplex-Umschaltung müssen portiert und kompiliert werden.

## Was das öffentliche JETI-Protokoll erlaubt

- EX Bus: USART 8N1, 125000 oder 250000 Baud, Half-Duplex.
- Kanaldaten, EX-Telemetrie und ein **JETIBOX-Textmenü mit 32 Zeichen** sind öffentlich dokumentiert. Das Textmenü ist die belastbare Planungsbasis für Einstellungen vom Sender.
- Die grafische **Device-Explorer-Konfiguration ist im öffentlichen Protokoll nicht dokumentiert**. Sie wird nicht als verfügbare V3.0-Funktion zugesagt. Eine spätere Erweiterung benötigt separat belegte Schnittstellen.
- Die empfohlene RX-/TX-Verbindung mit 2,4-kΩ-Widerstand, Freigabe der TX-Leitung und Antwortzeitfenster müssen aus der offiziellen Spezifikation umgesetzt werden.
- Logikpegel gemäß EX-Bus-Spezifikation: LOW höchstens 1,5 V, HIGH mindestens 3,0 V. Ob das garantierte HIGH zum R4-Eingang passt, ist anhand des MCU-Datenblatts und am Aufbau zu prüfen; gegebenenfalls Schnittstellentreiber verwenden.
- Versorgungsspannung und Signalpegel sind getrennte Fragen. 6-V-RC-Versorgung bedeutet nicht 6-V-Signal.
- EX Bus ist eine Master-/Slave-Verbindung. Die vorhandenen RSat-Leitungen an CB320 R1/R2 nicht per einfachem Y-Abzweig mit einem weiteren Teilnehmer belegen.
- Die konkrete geeignete EX-Bus-Ausgabe und Kanalbelegung im Swiss Trainer sind noch festzulegen. E1–E4 sind laut Schlepper-Dokumentation aktuell Telemetrieeingänge.

## Versorgung und Leistungsschalter

Maßgeblicher Anschlussplan: [hardware.md](hardware.md). Integration und Wandleranforderungen im [Schlepper-Repo](https://github.com/timschloemer109-max/king140-schlepper/blob/main/docs/SYSTEMS.md#5-v-versorgung-der-seilwinde).

RC-Anlage im Swiss Trainer derzeit 6,0 V, spätere HV-Umstellung erst nach Komponentenprüfung. Der 5-V-Wandler muss den tatsächlichen Eingang einschließlich Einbrüchen, Toleranzen und später gewähltem HV-Maximum abdecken. Ausgangsstrom aus Controller/Sensoren plus Reserve bestimmen; Ripple, Transienten, Temperatur und EMV am Aufbau prüfen. Keine 6 V/HV direkt auf den 5-V-Pin geben. Motor, Servos und Lichtleistung gehören nicht auf diesen Steuerungszweig.

Gemeinsame Masse für RC, Controller, Sensoren und ESC; Leistungsrückströme aus Signalmasse fernhalten. BEC-Plus des ESC isolieren, keine parallelen 5-V-Quellen. USB-/Logger-Rückspeisung anhand der konkreten Boardbeschaltung prüfen. Die Steuerung bleibt beim Abschalten des ESC-Leistungspfads an.

Für den elektronischen Schalter sind 20–30 A reale Last mit thermischer und Einschaltstromreserve zu betrachten. Schaltung, MOSFETs/Treiber und unabhängiger RC-Decoder gegenüber MCU-Ansteuerung sind noch offen. High-Side schaltet Plus; gemeinsame Signalmasse bleibt verbunden. Precharge, Einschaltstrom und Rückspeisung über das ESC-Signal prüfen. Kein Leistungs-MOSFET direkt aus GPIO als ungeprüfte Lösung.

## Grenzen der Seilpositionsmessung

Ein einzelner Hall-Sensor zählt Pulse, erkennt aber keine Drehrichtung. Mehrlagige Wicklung verändert den wirksamen Trommelradius. Meterwerte daher als **Schätzung** mit gültig/ungelernt/unbekannt kennzeichnen; Richtung aus eindeutigem Betriebszustand ableiten oder zusätzliche Erfassung vorsehen. Manuelles Ausziehen, Bewegung bei ausgeschalteter MCU und Neustart ohne bekannte Position brauchen ein definiertes Verhalten. Der Endschalter bleibt die absolute Endlagenreferenz.

## Architektur und Entwicklungsreihenfolge

1. Aktuellen Code und offene Branches prüfen; V2.0 als Referenz erhalten.
2. Entwicklungsordner `entwicklung/seilwinde_v3/` anlegen; Firmware, portable Zustandslogik, Tests, Simulator und Hardwareunterlagen dort sauber trennen. Maßgebliche Anforderungen bleiben in dieser Datei.
3. Steuerungskern mit injizierbarer Zeit und Ein-/Ausgängen auf dem Host testbar machen; Hall, Endschalter, Strom und Eingangs-Gültigkeit zentral verarbeiten.
4. Nicht blockierende Eingänge und begrenzte Laufzeiten umsetzen; EX-Bus-Parser/Encoder vom R4-UART-Treiber trennen.
5. R4-Backend zuerst minimal mit Kanaldaten und Telemetrie qualifizieren; danach JETIBOX-Menü, Persistenz, Warnlicht und Leistungsschalter-Schnittstelle.
6. ADC-/PWM-/Pinbelegung und Einstellungen dokumentieren; keine V2-Kalibrierdaten ungeprüft als R4-Werte übernehmen.
7. Hosttests, R4-Build und automatische Prüfungen einrichten; Hardwaretests als gesonderte offene Abnahme führen.

RC-Failsafe muss auf Alter **frischer, gültiger Kanaldaten** beruhen. Telemetrie-/Menüpakete dürfen fehlende Kanaldaten nicht frisch halten. Ungültige CRC, unvollständige Frames und Hold-Daten nicht unkritisch als sicheren RUN-Befehl interpretieren. Timeout/Glitchtoleranz messbar festlegen; Wiederkehr des Signals oder Neustart darf nicht selbsttätig anlaufen.

## Testwerkzeuge und Abnahme

[JETI Studio](https://www.jetimodel.com/support/jeti-studio/jeti-studio.html) enthält offiziell einen DC/DS-24-Senderemulator. Er ist für Senderoberfläche/Lua-Tests zu untersuchen. Eine vollständige EX-Bus-Empfänger- und RA4M1-Hardwareemulation ist damit nicht nachgewiesen; die Recherche hat keinen für diesen Gesamtaufbau bestätigten fertigen Emulator ergeben.

Zusätzlich einen EX-Bus-Testtreiber auf Basis der öffentlichen Spezifikation bauen oder einen nachweislich geeigneten vorhandenen verwenden: deterministische Kanaldaten, Telemetrie-/JETIBOX-Anfragen, Byte-Fragmentierung, CRC-/Längenfehler, Paketverlust, Timeout, Rückkehr und Überlast. Gemeinsame Encoder-/Decoderfehler durch unabhängig berechnete Referenztelegramme vermeiden. Ein Hostmodell prüft Logik, kein echtes UART-Timing.

Abnahme mindestens:

- Boot mit RUN/LEARN, Neutralfreigabe, neue RUN-Flanke, ESC-Neueinschalten und kein Wiederanlauf nach Fehler.
- Endschalter in jeder aktiven Phase, auch während Busverkehr, Menü, Logging und Rampen; maximale Reaktionszeit begründen und am Aufbau messen.
- FAST/SLOW einschließlich Neustart/Restweg; Voll-Lernen und Verwerfen kurzer oder unplausibler Lernbewegungen.
- Kalibrierte Blockadeerkennung, hohe Last bei drehender Trommel und Laufzeit-Failsafe.
- RC-Kurzstörung, echter Verlust, defekte/alte Kanalframes und Wiederkehr; Uhrzeitüberlauf, Pulszählergrenzen und atomare ISR-Zugriffe.
- Ungültige Parameter, leeres/korruptes/altes EEPROM, unterbrochenes Speichern, minimale Schreiblast.
- Warnlicht-Priorität und Durchleitung; Fehler im Licht-/Loggerzweig darf keine Motorfreigabe erzeugen.
- Simultane EX-Bus-Telemetrie, Menü und USB-Logger; volle Puffer/abgezogener USB, kein Warten auf Serial.
- Reproduzierbarer Nano-R4-Build mit fixierten Core-/Library-Versionen und Flash-/RAM-Bericht.
- Am echten R4/JETI-Aufbau: Pegel, 125/250 kBaud, TX-Freigabe, Antwortfenster, ESC-Arming, Brownout, 5-V-Ripple/EMV und Lasttests.

Ergebnisse als **bestanden, fehlgeschlagen oder nicht ausführbar** protokollieren, mit Commit, Werkzeugversionen und Befehlen. Ohne Hardwareprüfung keine Flugfreigabe aus einem grünen Hosttest ableiten.

## Primärquellen

Abruf/Codeprüfung: 2026-10-07.

- [Arduino Nano R4](https://docs.arduino.cc/hardware/nano-r4/) und [Datenblatt ABX00142](https://docs.arduino.cc/resources/datasheets/ABX00142-datasheet.pdf)
- [klassischer Nano, Datenblatt A000005](https://docs.arduino.cc/resources/datasheets/A000005-datasheet.pdf)
- [Nano Every, Datenblatt ABX00028](https://docs.arduino.cc/resources/datasheets/ABX00028-datasheet.pdf)
- [offizielles JETI EX-Bus-Protokoll](https://www.jetimodel.com/support/telemetry-protocol/ex-bus-communication-protocol.html)
- [JetiExBus-Bibliothek](https://github.com/nichtgedacht/JetiExBus) und [UART-Backend](https://github.com/nichtgedacht/JetiExBus/blob/master/src/JetiExBusArduinoSerial.cpp)
- [JETI Studio](https://www.jetimodel.com/support/jeti-studio/jeti-studio.html) und [offizielle Lua-Apps / Emulatorhinweis](https://github.com/JETImodel/Lua-Apps)
