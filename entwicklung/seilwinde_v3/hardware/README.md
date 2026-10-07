# Nano-R4-Schnittstellen und Werkbankabnahme

Stand: 2026-10-07. Die folgenden Pins sind der **kompilierte V3-Entwicklungsstand**, keine am Aufbau bestätigte Verdrahtung. Maßgeblich sind `firmware/Schleppseilwinde_V3_0/board_config.hpp` und [die Anforderungen](../../../docs/software_v3_0.md). Die reale V2-Kalibrierung bleibt [historische Referenz](../../../docs/calibration.md).

## Pins und elektrische Grenzen

| Funktion | Nano R4 / RA4M1 | V3-Verarbeitung / Anschluss |
| --- | --- | --- |
| EX Bus RX | D0 / P301 / SCI2 RXD | Eigener asynchroner FSP-UART, 125000 oder 250000 Baud, 8N1 |
| EX Bus TX | D1 / P302 / SCI2 TXD | Nur während angeforderter Antwort aktiv; anschließend GPIO-Eingang mit schwachem Pull-up |
| Hall | D2 / P105 / IRQ0 | FALLING-Interrupt, ISR-Zähler; zwei Magnete bleiben geometrische Arbeitshypothese |
| Endschalter | D7 / P107 | INPUT_PULLUP, LOW stoppt; D7 hat im fixierten Core **keinen externen Interrupt**, daher priorisiertes Polling |
| ESC-Signal | D4 / P103 | Eigener GPT-Adapter mit 20-ms-Rahmen; erste Pulse 1000 µs; aktiver HIGH-Puls bei Stopp sofort gekürzt |
| ACS712 OUT | A3 / P002 / ADC-Kanal 2 | 12-Bit-Ausgabeskala 0…4095, Referenz nominell 5000 mV; Sensorvariante/Sensitivität unbestätigt |
| Status-LED | LED_BUILTIN / Index 22 / P204 | D13/P111 ist beim Nano R4 **nicht** die eingebaute LED |
| USB-Diagnose | USB-C / USB CDC | Vom SCI2-UART getrennt, passiver Pi-Logger |
| ESC-Leistungsschalter | keine Pinzuweisung | `PowerAdapterQualified=false`; kein ungeprüfter Gate-/MOSFET-Ausgang |
| uniLIGHT | keine Pinzuweisung | `LightAdapterQualified=false`; kein angenommener Warn-PWM-Wert für ein unbekanntes Modul |

`MotorBenchQualified=false` sperrt reale Fahr-PWM im ausgelieferten Build. Stromkalibrierung ist ebenfalls deaktiviert, Sensitivität 0; eine vermutete ACS712-Variante darf diese Sperre nicht ersetzen. Freigaben sind bewusste lokale Integrationsschritte nach dokumentierter Messung; sie werden nicht aus dem Sender-Menü aktiviert.

Nano R4 betreibt GPIOs mit 5 V; Qwiic hat 3,3 V. [Arduino ABX00142, Abschnitt 3.4](https://docs.arduino.cc/resources/datasheets/ABX00142-datasheet.pdf) begrenzt GPIO-Strom auf 8 mA. Motor-, Lichtleistung und Gate-Ansteuerung gehören an passende externe Treiber.

## EX-Bus-Pegel und Half-Duplex

Die [JETI-Spezifikation](https://www.jetimodel.com/support/telemetry-protocol/ex-bus-communication-protocol.html) zeigt RX/TX-Verbindung mit 2,4-kΩ-TX-Serienwiderstand. Dieser Widerstand ist **kein Pegelwandler**. Der [RA4M1-Datensatz, Tabelle 2.4](https://www.renesas.com/en/document/dst/ra4m1-group-datasheet) garantiert normale/periphere Eingangserkennung bei HIGH ≥ 0,8 × VCC und LOW ≤ 0,2 × VCC: bei 5,0 V also HIGH ≥ 4,0 V und LOW ≤ 1,0 V. JETI garantiert nur HIGH ≥ 3,0 V und LOW ≤ 1,5 V. Die garantierten Bereiche decken sich nicht vollständig. Eine direkte Busanbindung ist damit elektrisch **nicht qualifiziert**.

Vor Anschluss muss ein geeigneter Empfänger-/Sender-Schnittstellentreiber festgelegt werden, der diese Eingangsbereiche, zulässigen Ausgangspegel, ausgeschaltete Teilnehmer und hochohmige TX-Freigabe abdeckt. Bei einem zunächst direkt untersuchten Empfänger reichen typische gemessene Pegel allein nicht als Worst-Case-Nachweis. Der öffentliche 2,4-kΩ-Schaltvorschlag und der rohe R4-UART-Adapter dürfen nur mit qualifizierter Pegelschnittstelle verwendet werden.

`R4ExBusUart` besitzt SCI2 exklusiv; `Serial1.begin()/write()/flush()` dürfen daneben nicht verwendet werden. Er speichert bis zu 127 empfangene Bytes mit ISR-Zeitstempel und übernimmt Antworten in einen eigenen 64-Byte-Puffer. Während der Antwort sind RX und RX-Interrupt absichtlich deaktiviert, damit eigenes Echo keine Kanäle erzeugt. Erst `UART_EVENT_TX_COMPLETE` nach dem letzten Stopbit schaltet TE aus und D1 zurück auf Eingang; `TX_DATA_EMPTY` darf das nicht tun. Der Master muss die angeforderte Antwortlücke lassen. Buskollisionen, Registerumschaltung und Freigabelatenz bleiben reale Messpunkte.

Nicht per Y-Kabel an RSat-/CB320-R1/R2 anbinden. Geeigneten EX-Bus-Ausgang, Empfänger-Failsafe-/Hold-Verhalten und Kanalzuordnung im Swiss Trainer zuerst festlegen. Telemetrieeingänge E1–E4 sind nicht automatisch ein geeigneter Master-Ausgang.

## Versorgung

Verbindlich: RC-Anlage derzeit 6,0 V → ausgewählter Wandler → stabile 5,0 V für Steuerung und Sensoren. Motorzweig: separater 2S-LiPo → geeigneter High-Side-Schalter, sobald qualifiziert → ACS712-Leistungspfad/ESC → Motor; Reihenfolge und Strommessposition mit dem konkreten Leistungsschaltplan festlegen. ESC-BEC-Plus isolieren. Gemeinsame Masse bleibt verbunden, Motor-Rückstrom von Signalmasse fernhalten. Steuerung muss bei abgeschaltetem ESC weiterlaufen.

Keine 6 V/HV direkt an 5V. Nano-R4-VIN besitzt zwar einen eigenen Wandler ([Arduino, Abschnitt 3.2](https://docs.arduino.cc/resources/datasheets/ABX00142-datasheet.pdf)); das ersetzt die verbindliche projektspezifische RC→5-V-Schnittstelle nicht. Einspeisepunkt für die externen 5 V sowie USB-/Pi-Rückspeiseschutz nach Board-Schaltplan festlegen und messen. Motor/Servos/Licht nicht vom Steuerungszweig speisen.

## Speicher und PWM

`R4FlashStorage` verwendet die ersten zwei 1024-Byte-Datenflashblöcke bei 0x40100000 und 0x40100400 exklusiv. Zwei 128-Byte-Journaldatensätze liegen in getrennten Löschblöcken. Übernahme: RAM-Staging → asynchrones Löschen des inaktiven Blocks → 127 Datenbytes → Commit-Byte zuletzt → vollständige Rückleseprüfung. Während BGO wird nur die RAM-Kopie gelesen. Kein Mischen mit Arduino-EEPROM oder RM_VEE. Bei Speicherfehler/500-ms-Stufen-Timeout bleibt die Freigabe bis zum Neustart gesperrt.

Der eigene GPT-PWM-Adapter vermeidet den ungeprüften 1500-µs-Startpuls von Renesas `Servo.attach()`. Die Signaländerung auf LOW bei einer aktiven Fahrt verkürzt einen laufenden Puls; dass der konkrete ESC dies sicher behandelt, ist zu messen. Der nachfolgende Stopp-Puls erscheint im nächsten 20-ms-Rahmen. PWM-Stopp ist keine elektrische Abschaltung des Motorleistungspfads.

## Werkbank-Abnahmeblatt

Alle unten stehenden Schritte sind am 2026-10-07 **nicht ausgeführt**: Arduino-CLI-Portinventar leer, kein Nano R4/JETI-Aufbau verfügbar. Messfelder bewusst leer. Build-/Hosttests ersetzen keine Freigabe für Motorlast oder Flug. Firmwareprofile wählen die Baudrate explizit über `build.py --bus-baud 125000/250000`, keine automatische Baudratenerkennung. ADC-Rohwerte stehen als begrenzte `#adc_raw`-Debugzeile einmal je Sekunde auf USB zur Verfügung.

Pro Lauf festhalten: Datum, Git-Commit, `.elf/.bin`-Hash, Core, Firmwareparameter, Board-/Empfänger-/ESC-/uniLIGHT-Typ und Firmwareversion, Versorgungs-/Kabelaufbau, Messgerät, Rohmitschnitt und Ergebnis **bestanden / fehlgeschlagen / nicht ausführbar**. Bei einem fehlgeschlagenen Schutztest zuerst Ursache beheben und die betroffenen Tests wiederholen.

| ID | Aufbau und konkreter Versuch | Nachweis / Abnahmekriterium |
| --- | --- | --- |
| HW-01 | Motorakku ab, RC→5-V-Wandler allein; minimale/maximale RC-Spannung, Lastwechsel und späteres HV-Maximum | 5,0-V-Zweig innerhalb festgelegter Board-/Sensorgrenzen; min/max/Ripple/Transienten/Temperatur/Stromaufnahme messen. Wandlerdatenblatt und Einbau dokumentieren |
| HW-02 | GND durchklingeln, BEC-Plus prüfen; USB/Pi jeweils ab/an mit RC ab/an | Keine unzulässige Rückspeisung; gemeinsame Signalmasse ohne Leistungsrückstrom; Verhalten bei ausgefallener Quelle dokumentieren |
| HW-03 | Qualifizierten EX-Bus-Treiber ohne ESC anschließen; beide Teilnehmer separat abschalten | HIGH/LOW und Flanken an Master, RX und TX messen; RA4M1- und JETI-Grenzen mit Reserve auch bei ausgeschalteter Gegenstelle einhalten |
| HW-04 | 125 und 250 kBaud, Kanäle/Telemetrie/JETIBOX kombiniert, worst-case 40-Byte-Antwort | Mit Logikanalysator CRC, ID-Echo, Start-/Endzeit messen; Antwort einschließlich 200-µs-Softwareguard im 4-ms-Fenster; TX-Freigabe nach letztem Stopbit ohne aktiven Idle-Treiber; Baudwechsel/Erkennung prüfen |
| HW-05 | Endschalter elektrisch aktivieren in FAST/SLOW, Rampe, Lernen, maximalem Busverkehr, Menü, Flashsave und angeschlossenem/abgezogenem USB | D7-Flanke, Schleifen-/Stoppmarke und D4 gleichzeitig erfassen; worst-case D7→LOW und D7→1000-µs-Folgepuls messen. Softwareziel Schleifendauer < 1 ms; nicht aus Hostmessung ableiten. Leistungspfad-AUS separat messen |
| HW-06 | Boot mit RUN/LEARN, gehaltenes RUN, frisch neutral, echte neue Flanke, Signalunterbrechung/-wiederkehr | Kein Fahrpuls ohne Neutralfreigabe und neue Flanke; STOP während ESC-Arming, keine Wiederaufnahme nach Fehler/Busverlust/Brownout |
| HW-07 | D4 zunächst am Logikanalysator, ESC ohne Motorakku; Einschalten/RESET/Save und 1000/1280/2000-µs-Anforderung | Kein 1500-µs-Bootglitch; Pulsdauer/Jitter/20-ms-Rahmen/Timerreserve messen; gekürzten Stopp-Puls und signalarmen Wartungszustand am konkreten ESC bewerten |
| HW-08 | Hall per Hand/Funktionsgenerator; Rohpulse und ISR-Zähler parallel | Zwei Pulse/Umdrehung neu bestätigen; maximale Drehzahl, 1000-µs-Flankenfilter, EMV-Fehlpulse und atomare Zählerzugriffe verifizieren |
| HW-09 | ACS712-Typ bestimmen, Motorzweig aus; Referenzspannung/ADC-Rohwerte bei Null und bekannten Strömen | Offset, Sensitivität, Vorzeichen, Filter-/Samplelatenz und Sättigung messen. V2-Werte nicht übernehmen; Stall erst nach R4-Kalibrierung aktivieren |
| HW-10 | Unter Aufsicht mechanisch gesicherter Aufbau; hohe Last mit fortgesetzten Pulsen, dann begrenzte kontrollierte Blockade | Hohe Last allein stoppt nicht; hoher kalibrierter Strom plus fehlende Pulse stoppt nach geprüfter Anlaufmaske. Keine Hand in laufende Trommel; Strom-/Hall-/PWM-Kurven speichern |
| HW-11 | Vollständige Lernfahrt bis Endschalter, anschließend kurze Resetfahrt und unplausible Pulswerte | Voll-Lernen plausibel übernehmen; kurze/unplausible Bewegungen ändern den gültigen Journaldatensatz nicht; manuelles Ausziehen/Neustart als Positionsunsicherheit prüfen |
| HW-12 | Parameter explizit im Stillstand speichern; Versorgung in Erase-, Body- und Commitphase kontrolliert unterbrechen | Alter oder vollständig neuer CRC-gültiger Datensatz, niemals Teilformat; Saveanzeige erst nach Rücklesen. Wiederholte unveränderte Übernahme ohne Flashschreibvorgang |
| HW-13 | Modell der Licht-/Leistungsstufe exakt bestimmen, zunächst Prüflast | Pass-through/Warnpriorität, AUS bei Reset/Fehler, Einschaltreihenfolge/Precharge/Arming, Rückspeisung über ESC-Signal und thermische Reserve bei 20–30 A prüfen; Treiber extern |
| HW-14 | Verbrenner-/Zündungs-/Killswitch-EMV-Vergleich nach `docs/emv_rc_stoerungen.md`, Motorpfad zuerst deaktiviert | Versorgungs-, EX-Bus-, Hall- und D4-Rohsignale vergleichen; keine Geisterpulse/-drehungen; Störungen nicht allein durch längeren RC-Timeout verdecken |
| HW-15 | Maximaler Telemetrie-/Menü-/USB-Verkehr, Flashsave und längerer Betrieb | Stack-/Heapreserve, UART-Ringüberlauf, verworfene Logs/Requests, Schleifenmaximum und Timerjitter messen; Nebensysteme erzeugen niemals Motorfreigabe |

Erst nach bestandener Signal-/Schutzabnahme sind beaufsichtigte Motorversuche sinnvoll. Der vorliegende Entwicklungsbuild hat keine Flugfreigabe.
