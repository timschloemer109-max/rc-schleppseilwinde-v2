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

## Festgelegter V2.0-Stand

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

## Dokumentationspflicht

Diese Unterlagen muessen aktiv gepflegt werden:

- `README.md`
- `AGENTS.md`
- `docs/hardware.md`
- `docs/decisions.md`
- `docs/todo.md`

Bei jeder relevanten Aenderung an Hardware, Pinbelegung, Sicherheitslogik, Lernlogik oder Teststand muessen die passenden Dateien mit aktualisiert werden.

## Was nicht stillschweigend passieren darf

- Endschalterlogik abschwaechen
- Ueberstrom wieder als alleiniges Fehlerkriterium einfuehren
- Lernfunktion so aendern, dass kleine Reset-Bewegungen Werte speichern
- Pinbelegung ohne Doku-Anpassung aendern
- neue magische Zahlen quer im Code verteilen

## Testpflichtige Punkte nach Aenderungen

- Start bei `RUN`
- autonomes Weiterlaufen nach Rueckkehr auf Schaltermitte
- Stopp am Endschalter
- Stall-Fall: hoher Strom plus keine Hall-Pulse
- hohe Last mit noch drehender Trommel
- Laufzeit-Failsafe
- Lernfahrt vollstaendig
- kurzer Reset ohne Ueberschreiben der Seillaenge
