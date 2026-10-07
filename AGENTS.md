# AGENTS.md - RC-Schleppseilwinde V2.0

Diese Datei definiert die Arbeitsregeln fuer die Weiterentwicklung der Schleppseilwinde.

## Projektziel

Die Steuerung soll eine RC-Seilwinde robust, nachvollziehbar und hardwaretauglich betreiben. Vorrang haben Sicherheitsfunktionen und reproduzierbares Verhalten im Flug.

## Verbindliche Sicherheitsregeln

1. Der Endschalter bleibt harte Abschaltbedingung.
2. Ueberstrom alleine reicht nicht fuer einen Fehler aus.
3. Blockade wird nur aus hohem Strom plus fehlenden Hall-Impulsen erkannt.
4. Eine maximale Laufzeit bleibt immer als Failsafe erhalten.
5. `RESET/LEARN` darf kurze Freiziehbewegungen nicht als neue Seillaenge speichern.

## Architekturregeln

- Die Hauptlogik bleibt eine klare Zustandsmaschine.
- Einstellwerte stehen zentral und kommentiert im Sketch.
- Hall-Sensor bleibt auf einem Interrupt-Pin.
- Schnell- auf Langsamfahrt wird pulsbasiert abgeleitet, nicht primaer zeitbasiert.
- Aenderungen sollen fuer kleine Mikrocontroller uebersichtlich bleiben.
- Ein externer Logger bleibt rein passiv und darf nie Teil der Sicherheitskette werden.

## V3.0-Planung

Die nächste Generation heißt V3.0. Vor Änderungen `docs/software_v3_0.md` und `docs/codex_v3_0_auftrag.md` lesen. Implementierte Firmware, portable Logik, Tests, Simulator und Hardwareunterlagen liegen unter `entwicklung/seilwinde_v3/`; V2.0 als Referenz erhalten. Nano R4/Core 1.6.0 ist softwareseitig baubar, reale Hardwarequalifikation offen. JETIBOX-Textmenü ist implementiert; grafischer Device Explorer wird nicht zugesagt. Ergebnisse und Befehle stehen in V3 README/reports.

- V3-Einstellungen zentral in `core/settings.cpp`, Hardwareprofil in `firmware/Schleppseilwinde_V3_0/board_config.hpp` darunter. MotorBenchQualified bleibt standardmäßig false; unqualifizierte Adapter niemals durch EEPROM/Menu freigeben.
- Native Logik und Sketch teilen dieselben Quellen über `build.py`-Staging. Nur frische gültige Kanalframes erneuern RC-Alter; EX Bus besitzt kein Hold-Flag, Empfänger-Hold muss ausgeschaltet sein.
- Endschalter vor/innerhalb/nach begrenzter Busbearbeitung. Sicherheitsstopp übersteuert Rampen. LEARN ist manuelles Ausziehen bei STOP-PWM, Voll-Lernen nur ab Nullreferenz.
- Keine synchronen `Serial1.write/flush`, `analogRead`, `EEPROM.update` oder Servo-Defaultattach in den Steuerpfad übernehmen; SCI2/GPT/ADC/Flash-BGO-Adapter verwenden.
- Gültige 1000-us-STOP-Pulse erhalten. RX/TX-Turnaround und asynchrones Flashjournal gesondert testen; Menüübernahme und Antrieb während Commit sperren, danach neu armieren.
- Flashslot-Abstand mindestens 1024 Byte, Commitbyte zuletzt; V2-EEPROM nicht importieren. Lernen nur RAM, explizites Save im Stillstand.
- Firmwareänderungen erfordern passende native Prüfungen, beide R4-Baudbuilds und aktualisierte Hardware-Abnahme. Kein unbeaufsichtigtes Flashen/Motorstart.

## Festgelegter V2.0-Stand

Die folgenden Pins und Verhaltensangaben gelten für V2.0; eine V3-Pinbelegung separat qualifizieren und dokumentieren.

- Arbeitsdatei: `src/Schleppseilwinde_V2_0/Schleppseilwinde_V2_0.ino`
- Werkbank-Test: `src/Schleppseilwinde_Testbench/Schleppseilwinde_Testbench.ino`
- Hall-Sensor: `D2`
- Endschalter: `D7`
- RC Speed: `D3`
- RC Trigger: `D5`
- ESC Signal: `D4`
- ACS712: `A3`
- LED: `D13`

Der Testbench-Sketch ist nur fuer beaufsichtigte Werkbanktests ohne RC-Empfaenger gedacht und ersetzt die Endschalter-Absicherung nicht.
Im aktuellen Zwischenstand der Hauptfirmware wird der ACS712-Wert seriell beobachtet, aber die Stromschwelle wird erst nach Lasttests mit Seil als Fehlerkriterium aktiviert.
Das feste CSV-Telemetrieformat fuer den Pi-Logger ist in docs/pi_logger.md als Ziel dokumentiert. Im am 2026-10-07 geprueften main-Sketch ist es noch nicht implementiert; Serial liefert Text-Debug. Vor Aenderungen den jeweiligen Branch erneut pruefen.
Kurze RC-Aussetzer waehrend `RUN` werden erst nach `3` aufeinanderfolgenden ungueltigen RC-Lesungen als `RC_SIGNAL_LOSS` gelatcht, damit kurze EMV-Glitches keinen sofortigen Fehlstopp ausloesen.

## Dokumentationspflicht

Diese Unterlagen muessen aktiv gepflegt werden:

- `README.md`
- `AGENTS.md`
- `docs/hardware.md`
- `docs/decisions.md`
- `docs/pi_logger.md`
- `docs/todo.md`
- `docs/software_v3_0.md`
- `docs/codex_v3_0_auftrag.md`

Bei jeder relevanten Aenderung an Hardware, Pinbelegung, Sicherheitslogik, Lernlogik, Telemetrieformat oder Teststand muessen die passenden Dateien mit aktualisiert werden.

## Was nicht stillschweigend passieren darf

- Endschalterlogik abschwaechen
- Ueberstrom wieder als alleiniges Fehlerkriterium einfuehren
- Lernfunktion so aendern, dass kleine Reset-Bewegungen Werte speichern
- Pinbelegung ohne Doku-Anpassung aendern
- neue magische Zahlen quer im Code verteilen
- den Pi-Logger unbemerkt in die Steuer- oder Sicherheitslogik ziehen

## Testpflichtige Punkte nach Aenderungen

- Start bei `RUN`
- autonomes Weiterlaufen nach Rueckkehr auf Schaltermitte
- Stopp am Endschalter
- Stall-Fall: hoher Strom plus keine Hall-Pulse
- hohe Last mit noch drehender Trommel
- Laufzeit-Failsafe
- kurzer RC-Glitch waehrend `RUN` darf keinen Sofort-Fehlstopp mehr ausloesen
- Lernfahrt vollstaendig
- kurzer Reset ohne Ueberschreiben der Seillaenge
- nach Implementierung des Hauptfirmware-CSV: CSV-Header genau einmal nach Boot
- nach Implementierung des Hauptfirmware-CSV: `telemetry` im Intervall und `event` bei Zustandswechseln
