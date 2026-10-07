# Codex-Auftrag: Seilwinde V3.0 entwickeln und testen

Stand: 2026-10-07. Den folgenden Auftrag vollständig in Codex einfügen. Die Dokumentation beschreibt den Zielstand; fehlende Hardwareprüfungen bleiben ausdrücklich offen.

Ausgeführt im Entwicklungsbranch `codex/seilwinde-v3`: [Implementierung/Befehle](../entwicklung/seilwinde_v3/README.md), [tatsächliche Testergebnisse](../entwicklung/seilwinde_v3/reports/test_report.md), [offene Hardwareabnahme](../entwicklung/seilwinde_v3/hardware/README.md). Der folgende Originalauftrag bleibt die nachvollziehbare Abnahmegrundlage.

---

Entwickle die Software **V3.0** der RC-Schleppseilwinde im Repository `timschloemer109-max/rc-schleppseilwinde-v2`. Setze die Entwicklung um und führe alle in deiner Umgebung möglichen sinnvollen Tests aus. Liefere funktionsfähigen, reproduzierbar baubaren Code, Testwerkzeuge und einen ehrlichen Testbericht. Arbeite schrittweise weiter; ein Plan allein erfüllt den Auftrag nicht.

## 1. Arbeitsgrundlage prüfen

Lies zuerst `AGENTS.md`, `README.md`, `docs/software_v3_0.md`, `docs/hardware.md`, `docs/decisions.md`, `docs/calibration.md`, `docs/pi_logger.md` und relevante EMV-Unterlagen. Prüfe den tatsächlichen aktuellen Code, Branch und offene PRs. Übernimm keine Behauptung über implementierte Funktionen ohne Codeabgleich.

V2.0 bleibt als Referenz erhalten. Ändere die Firmware nicht nur dem Namen nach in 3.0 und überschreibe keine fremden Arbeitsstände. Verwende einen eigenen Entwicklungsbranch. Prüfe externe Bibliotheken anhand ihrer aktuellen Primärquellen und ihrer Lizenz. Änderungen am Schlepper-Repo sind nicht Teil dieses Firmwareauftrags; dessen Versorgungsvorgaben sind verbindliche Schnittstellenbedingungen.

## 2. Arbeitsordner ordentlich aufbauen

Arbeite im in Codex hinterlegten Arbeitsordner. Ermittle den tatsächlichen Repository-Root; verwende einen vorhandenen Checkout, statt eine zweite Kopie hineinzuklonen. Wenn noch keiner vorhanden ist, lege den Checkout unter `Seilwindenentwicklung/rc-schleppseilwinde-v2/` an.

Lege **im Repository** `entwicklung/seilwinde_v3/` als zusammenhängenden Entwicklungsbereich an:

| Ordner | Inhalt |
| --- | --- |
| `firmware/Schleppseilwinde_V3_0/` | Arduino-Sketch und plattformspezifische Adapter |
| `core/` | auf dem Host testbare Steuerung, Parameter und Protokolllogik |
| `tests/` | Unit-, Integrations-, Fehler- und Regressionstests |
| `simulator/` | EX-Bus-Testmaster, Szenarien und unabhängige Referenztelegramme |
| `hardware/` | V3-Pinplan, Schnittstellen und konkrete Werkbank-Testanleitung |
| `reports/` | kompakter versionierter Testbericht und Ressourcen-/Timingnachweise |
| `.build/` und `artifacts/` | erzeugte Builds, große Logs und temporäre Ergebnisse, per gitignore ausgeschlossen |

Ergänze dort eine README mit Aufbau und ausführbaren Build-/Testbefehlen. Keine losen neuen Dateien im Workspace verteilen. Halte zentrale Anforderungen in `docs/software_v3_0.md` und vermeide widersprüchliche Kopien. Nutze Buildsystem-/Arduino-Konventionen korrekt; nötige Quellen zuverlässig einbinden und aus sauberem Checkout bauen.

## 3. Firmware umsetzen

Nutze **Arduino Nano R4 / RA4M1 als bevorzugtes Ziel**, qualifiziere aber zuerst Core, UART, ADC, Servo/Timer, Interrupts und Persistenz. Der vorhandene ATmega328P ist V2-Referenz. Die geprüfte JetiExBus-Bibliothek hat noch kein R4-Backend: Portierung oder begründete Alternative erforderlich, kein Kompatibilitätsversprechen durch `architectures=*`.

Trenne Zustandsmaschine, Zeitquelle, Hardwarezugriff, Eingänge, Persistenz und Diagnose. Halte die Verarbeitung begrenzt und nicht blockierend. Kein `pulseIn()`, lange `delay()`, endloses Warten auf USB/Serial oder unbeschränkte Bus-/Log-Schleifen im Steuerpfad. Beachte Zeitüberläufe und atomaren Zugriff auf ISR-Daten.

Implementiere die in `docs/software_v3_0.md` spezifizierten Funktionen:

- vorhandene Drei-Stufen-Bedienung, Neutralfreigabe und autonomes Einziehen erhalten;
- Endschalter mit Vorrang, maximale Laufzeit und kein automatischer Wiederanlauf;
- pulsbasiertes FAST/SLOW, geschützte Lernfunktion und validierte versionierte Speicherung;
- kalibrierbare Strommessung; Blockade nur aus hohem Strom plus fehlenden Pulsen und Anlaufmaske;
- JETI EX Bus für Kanaldaten, Telemetrie und **öffentlich dokumentiertes JETIBOX-Textmenü**;
- Sender-Einstellungen für Seillänge, FAST-Anteil, Geschwindigkeiten, Warnschwelle und zulässige Schutz-/Rampenwerte;
- Softstart/Rampen, sofort von Stoppbedingungen übersteuerbar;
- uniLIGHT-Signal-Durchleitung und Seilende-Warnung mit dokumentierter Priorität;
- Leistungsschalter-Schnittstelle: AUS beim Reset/Fehler, sichere Zuschalt-/Arming-Sequenz und Neutralfreigabe;
- begrenzte CSV-Diagnose für den passiven USB/Pi-Logger.

Grafischer Device Explorer ist nicht öffentlich spezifiziert und keine zugesicherte Funktion. EX-Bus-Failsafe auf frische gültige Kanaldaten stützen; Telemetrieanfragen dürfen deren Alter nicht zurücksetzen. Keine ungeprüfte parallele Anbindung an RSat/CB320 R1/R2.

Die Elektrik ist gesetzt: RC-Anlage derzeit 6,0 V, künftig eventuell HV; **passender vorgeschalteter Wandler liefert stabile 5,0 V an Steuerung/Sensoren**. ESC-BEC unbenutzt und Plus isoliert. Motor über ESC aus separatem 2S-LiPo. Steuerung bleibt bei ausgeschaltetem Motorzweig an. Ein MCU-Ausgang ersetzt keinen Gate-/Leistungstreiber.

Wo Hardwaredetails fehlen, verwende klar benannte deaktivierte Adapter und sichere Defaults statt erfundener Belegungen, Stromgrenzen oder uniLIGHT-Werte. Entwickle die unabhängigen Teile weiter und dokumentiere die konkrete noch erforderliche Entscheidung. Meterwerte als Schätzung mit Gültigkeit ausgeben; ein einzelner Hall-Sensor misst keine Richtung und keinen konstanten Meterweg bei Mehrlagenwicklung.

## 4. Testumgebung und JETI-Simulation

Prüfe [JETI Studio](https://www.jetimodel.com/support/jeti-studio/jeti-studio.html): Es enthält einen DC/DS-24-Senderemulator. Ermittle konkret, welche Sender-/Lua-/Telemetrieoberflächen damit in dieser Umgebung testbar sind. Setze diesen nicht ohne Nachweis mit einem EX-Bus-Empfänger oder RA4M1-Emulator gleich. Falls die GUI nicht ausführbar ist, dokumentiere das und führe die unabhängigen Tests trotzdem aus.

Baue zusätzlich einen deterministischen EX-Bus-Testmaster nach der offiziellen Spezifikation, sofern kein geeigneter nachweislich nutzbarer vorhanden ist. Teste Parser/Encoder und echte portable Firmwarelogik auf dem Host mit injizierbarer Zeit, simulierten Hall-Pulsen, Endschalter, Strom und RC-Daten. Nutze unabhängige Referenztelegramme mit belegter Herkunft/CRC; ein Test darf nicht ausschließlich denselben Encoder wie der Prüfling als Wahrheitsquelle verwenden.

Decke mindestens ab:

1. Boot mit RUN/LEARN, Neutralfreigabe, echte Startflanke, ESC-Arming, Fehlerquittierung und Signalwiederkehr ohne Wiederanlauf.
2. Endschalter während FAST, SLOW, Softstart, Lernen, Menü, Logging und Buslast; definierte maximale Abschaltlatenz.
3. FAST/SLOW-Restweg, Neustart, unkalibrierte Position, vollständiges Lernen und Ablehnung kurzer/unplausibler Lernbewegungen.
4. Hohe Last mit Hall-Pulsen, echter Stall, Anlaufmaske und Laufzeit-Failsafe.
5. Gültige Kanal-/Telemetrie-/JETIBOX-Pakete, CRC-/Längenfehler, Fragmentierung, Rauschen, Paketverlust, Timeout und ausschließlich Telemetrieverkehr.
6. Zähler-/Zeitüberlauf, Grenzwerte, ungültige Einstellungen, leere/korrupt/altes persistentes Format, unterbrochenes Speichern und Schreibverschleiß.
7. Logger abgezogen/volle Puffer, hohe Buslast und Menübedienung gleichzeitig; Sicherheit darf nicht blockieren.
8. Licht-/Leistungsschalter-Prioritäten, AUS-Defaults und keine Motorfreigabe durch Fehler eines Nebensystems.

Nutze für native Tests strenge Compilerwarnungen und verfügbare Address-/Undefined-Behavior-Sanitizer. Ergänze reproduzierbare Zufalls-/Fuzzfälle für den Protokollparser mit festen Seeds. Richte automatisierte Hosttests und Nano-R4-Kompilierung in CI ein; fixiere Core-/Library-Versionen und dokumentiere Flash/RAM sowie offene Stack-/Timingmessungen. Prüfe bei Warnungen deren Ursache; keine pauschale Unterdrückung.

## 5. Hardwaretests sauber abgrenzen

Lege eine konkrete Werkbank-Abnahme an: 5-V-Wandler/Ripple, Signalpegel, 125/250 kBaud, Half-Duplex/TX-Freigabe, Antwortfenster, Servo-PWM, Endschalterlatenz, Hall/ADC-Kalibrierung, Einschaltreihenfolge, Brownout/EMV, uniLIGHT und ESC-Leistungsschalter unter Last.

Führe nur tatsächlich angeschlossene und sinnvoll ausführbare Hardwaretests aus. Dieser Auftrag erlaubt kein unbeaufsichtigtes Flashen oder Anlaufenlassen eines Motors. Ohne vorhandene Hardware: **nicht ausgeführt** mit Ursache und nächstem Prüfschritt. Erfinde keine Messwerte, Emulatorergebnisse oder Flugfreigabe.

## 6. Fertigstellung und Übergabe

Führe alle erreichbaren Tests und den Zielboard-Build aus, behebe Fehler und wiederhole die betroffenen Prüfungen. Dokumentiere Grenzen der Simulation.

Pflege README, AGENTS, Hardware-/Entscheidungs-/Logger-Doku und TODO passend zum tatsächlich implementierten Stand. Liefere nachvollziehbare Commits auf dem Entwicklungsbranch und einen reviewbaren PR, sofern GitHub-Zugriff verfügbar ist; merge nicht automatisch.

Der Abschlussbericht muss enthalten: implementierte und noch offene Funktionen, genaue Build-/Testbefehle und Werkzeugversionen, Ergebnisse pro Testgruppe, Ressourcenverbrauch, noch ungemessene Timing-/Hardwarepunkte und die nächsten Werkbankschritte. Kennzeichne **bestanden / fehlgeschlagen / nicht ausführbar**. Simuliert getestete Software und am echten JETI-Aufbau bestätigtes Verhalten getrennt ausweisen.
