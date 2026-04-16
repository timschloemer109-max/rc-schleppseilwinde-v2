# Offene Punkte, Messungen und Tests

## Vor dem ersten Flugbetrieb

- Hall-Sensor-Typ und Magnetposition festlegen
- Magnetanzahl final festgelegt: `2`
- Magnetorientierung dokumentiert: `2` Magnete, `180 Grad` versetzt, gleiche Polrichtung
- Anzahl Hall-Pulse pro Trommelumdrehung gemessen: `2`
- ESC-Typ und BEC-Stromreserve dokumentieren
- `Turnigy Plush 30A` im Aufbau dokumentieren
- reale ACS712-Nullpunkt- und Lastwerte weiter messen und in `docs/calibration.md` nachfuehren
- langsame PWM fuer sicheren Endanlauf einmessen
- Stromgrenze fuer normalen Zug und fuer echten Stall erst nach Seiltests final festlegen
- erster echter Lernwert aufgezeichnet: `321` Hall-Pulse
- Rasen-Lernfahrt dokumentiert: `337` Hall-Pulse, geglaetteter Lernwert danach `347.1`
- Lizenz fuer oeffentliche GitHub-Nutzung festlegen

## Hardware-Tests

- Endschalter elektrisch und mechanisch geprueft, erster echter Seilweg-Endtest erfolgreich
- Werkbank-Testbench ohne Seil mit angeschlossenem Motor pruefen
- Hall-Signal mit langsamer und hoher Drehzahl pruefen
- Stoerfestigkeit der 5-V-Schiene unter Motorlast pruefen
- Temperaturverhalten von ACS712 und ESC pruefen
- Leitungsfuehrung auf Stoereinkopplung pruefen

## Firmware-Tests

- Testbench: `ARM`, `STOP`, `SET`, `SWEEP`, `STATUS` und `ZERO` pruefen
- Testbench: CSV-Ausgabe auf Vollstaendigkeit und Plausibilitaet pruefen
- erster Lasttest mit aufgezeichneter CSV gegen `docs/calibration.md` dokumentiert
- erster Stalltest mit aufgezeichneter CSV gegen `docs/calibration.md` dokumentiert
- Hauptfirmware: ACS712 vorerst nur beobachten, spaeter wieder als Fehlerkriterium zuschalten
- Endschalter-Latch und Ruecksetzung ueber `RESET/LEARN` dokumentiert
- Hauptfirmware: Laufzeit-Failsafe voruebergehend auf `60000 ms` fuer langsames Aufwickeln gesetzt
- erstes langsames Aufwickeln mit echtem Seil und aktivem Endschalter dokumentiert
- Lernen mit voller Abrollstrecke erfolgreich dokumentiert
- pulsgesteuerte Umschaltung FAST -> SLOW mit Lernwert erfolgreich dokumentiert
- Weiterlaufen nach Rueckkehr des Schalters in die Mitte erfolgreich dokumentiert
- Signalverlust eines RC-Kanals waehrend des Laufs erfolgreich dokumentiert
- Einschalten mit Trigger in Mitte und normalem ESC-Schaerfen pruefen
- Einschalten mit Trigger auf `RUN`: keine Bewegung bis Mitte und neuer `RUN`
- Einschalten mit Trigger auf `RESET/LEARN`: keine Lernfahrt bis Mitte
- Arduino vor ESC, ESC vor Arduino und nahezu gleichzeitig pruefen
- kein langsames Phantom-Einziehen nach manuellem Seil-Ausziehen und Loslassen
- sauberer Stopp am Endschalter
- Stall-Test mit absichtlicher Blockade
- hohe Last bei drehender Trommel ohne Fehlabschaltung
- Laufzeit-Failsafe
- kurzer Reset ohne Speichern
- Neustart mitten im Einfahrweg mit neuer Restweg-Logik praktisch erfolgreich geprueft
- teilweise Lernfahrt unterhalb der neuen `70 %`-Schwelle erfolgreich verworfen
- gespeicherte Seillaenge nach Arduino-Neustart gegen EEPROM-Inhalt und Laufverhalten erfolgreich geprueft
- Rasentest auf Rasen mit regulaerem Endschalter-Stopp und Peakstrom von etwa `5.03 A` dokumentiert

## Naechste Software-Schritte

- optional RC-Eingaenge spaeter auf Interrupt oder Pin-Change umstellen
- Testbench-Messdaten spaeter fuer Anlauf-PWM und Stromgrenzen auswerten
- `docs/calibration.md` nach weiteren Seil- und Lasttests weiterpflegen
- EEPROM-Struktur spaeter versionieren, falls weitere Parameter gespeichert werden
- pruefen, ob Endschalter auf fail-safe-NC umgestellt werden soll
- bei Brushless-Aufbau eventuell feinere Rampen oder Softstart ergaenzen
- weitere Abwickeltests im Freifeld mit Laengenvergleich gegen den Lernwert dokumentieren
- Beobachtungsregel fuer Fahrtests bei Bedarf spaeter ins README uebernehmen
- Bootsignal an `D4` bei Bedarf mit Oszi oder Logikanalysator messen
- optionalen Serienwiderstand oder Pulldown am ESC-Signal nur bei echtem Restproblem pruefen
- 5-V-Verhalten beim manuellen Rueckdrehen oder Ausziehen weiter beobachten
