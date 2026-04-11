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
- Lizenz fuer oeffentliche GitHub-Nutzung festlegen

## Hardware-Tests

- Endschalter elektrisch und mechanisch pruefen
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
- Start bei `RUN`
- Weiterlaufen nach Rueckkehr des Schalters in die Mitte
- sauberer Stopp am Endschalter
- Stall-Test mit absichtlicher Blockade
- hohe Last bei drehender Trommel ohne Fehlabschaltung
- Signalverlust eines RC-Kanals
- Laufzeit-Failsafe
- Lernen mit voller Abrollstrecke
- kurzer Reset ohne Speichern

## Naechste Software-Schritte

- optional RC-Eingaenge spaeter auf Interrupt oder Pin-Change umstellen
- Testbench-Messdaten spaeter fuer Anlauf-PWM und Stromgrenzen auswerten
- `docs/calibration.md` nach Lasttest und Endschaltertest weiterpflegen
- EEPROM-Struktur spaeter versionieren, falls weitere Parameter gespeichert werden
- pruefen, ob Endschalter auf fail-safe-NC umgestellt werden soll
- bei Brushless-Aufbau eventuell feinere Rampen oder Softstart ergaenzen
