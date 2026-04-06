# Offene Punkte, Messungen und Tests

## Vor dem ersten Flugbetrieb

- Hall-Sensor-Typ und Magnetposition festlegen
- Magnetanzahl final festlegen: `2`, `3` oder `4`
- Anzahl Hall-Pulse pro Trommelumdrehung messen
- ESC-Typ und BEC-Stromreserve dokumentieren
- `Turnigy Plush 30A` im Aufbau dokumentieren
- reale ACS712-Nullpunkt- und Lastwerte messen
- langsame PWM fuer sicheren Endanlauf einmessen
- Stromgrenze fuer normalen Zug und fuer echten Stall vergleichen
- Lizenz fuer oeffentliche GitHub-Nutzung festlegen

## Hardware-Tests

- Endschalter elektrisch und mechanisch pruefen
- Hall-Signal mit langsamer und hoher Drehzahl pruefen
- Stoerfestigkeit der 5-V-Schiene unter Motorlast pruefen
- Temperaturverhalten von ACS712 und ESC pruefen
- Leitungsfuehrung auf Stoereinkopplung pruefen

## Firmware-Tests

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
- optional Service-Modus fuer Sensor- und Rohwertanzeige
- EEPROM-Struktur spaeter versionieren, falls weitere Parameter gespeichert werden
- pruefen, ob Endschalter auf fail-safe-NC umgestellt werden soll
- bei Brushless-Aufbau eventuell feinere Rampen oder Softstart ergaenzen
