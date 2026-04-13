# Entscheidungen und verworfene Ansaetze

## Aktive Entscheidungen

### 1. Neue Basis ist der V2-Entwurf, nicht `seilwinde_1.2.ino`

Begruendung:

- der Altcode ist stark blockierend
- Stall-Erkennung nur ueber Strom ist fachlich unzureichend
- die zeitbasierte Schnell-/Langsamumschaltung ist nicht reproduzierbar

### 2. Hall-Sensor auf `D2`

Begruendung:

- der Hall-Sensor braucht als erste Prioritaet einen Interrupt-Pin
- damit bleibt die Drehzahlerfassung sauber und einfach

Ergaenzung:

- Referenzsensor ist `KY-003 A3144`
- Startkonfiguration sind `2` Magnete mit `2` Pulsen pro Trommelumdrehung

### 3. Endschalter auf `D7`

Begruendung:

- der Endschalter braucht keinen Interrupt
- so bleiben die knappen Spezialpins fuer zeitkritische Signale frei

### 4. Erste V2.0 liest RC noch mit `pulseIn()`

Begruendung:

- fuer den ersten Hardwarestand ist das schnell umsetzbar und gut nachvollziehbar
- die eigentliche Sicherheitsverbesserung kommt durch Hall-Sensor, Zustandsmaschine und bessere Fehlerlogik

Nachteile:

- weiterhin blockierend
- langfristig nicht die Endausbaustufe

### 5. Stall-Erkennung = Strom plus fehlende Pulse plus Anlaufmaske

Begruendung:

- hoher Strom allein darf im Flug nicht zum Fehlstopp fuehren
- eine kurze Startmaske verhindert falsche Fehler beim Anlaufen

Zwischenstand:

- bis belastbare Lasttests mit Seil vorliegen, wird der ACS712 in der Hauptfirmware nur beobachtet und weiter seriell ausgegeben
- die eigentliche Ausloeseschwelle fuer Stall wird erst danach final festgelegt

### 6. ACS712 wird beim Einschalten auf Nullpunkt eingemessen

Begruendung:

- Offset und Versorgung driften
- eine einfache Startkalibrierung verbessert die Praxistauglichkeit deutlich

### 7. Schutzbeschaltung bleibt im ersten Stand bewusst einfach

Begruendung:

- Leitungen sind kurz
- vorhandene Module bringen bereits Grundbeschaltung mit
- unnoetige Zusatzteile sollen vermieden werden

Nachbesserungen werden nur gezielt am realen Problem vorgenommen.

### 8. Lernwert wird geglaettet in EEPROM gespeichert

Begruendung:

- Seillaenge und Wickelbild schwanken
- ein gleitender Mittelwert ist robuster als hartes Ueberschreiben

### 9. Werkbank-Test als separater Sketch

Begruendung:

- der Testaufbau hat aktuell noch keinen Endschalter und keinen RC-Empfaenger
- damit bleibt die Hauptfirmware unvermischt und die Testabsicht eindeutig
- Hall, ESC und ACS712 koennen so isoliert und reproduzierbar vermessen werden

Ergaenzung:

- Testdatei ist `src/Schleppseilwinde_Testbench/Schleppseilwinde_Testbench.ino`
- Bedienung erfolgt seriell mit CSV-Ausgabe fuer Codex oder den Serial Monitor
- der Testbench-Sketch ist nur fuer beaufsichtigte Werkbanktests gedacht

## Bewusst verworfene oder vertagte Loesungen

### Rein zeitbasierte Schnell-/Langsamumschaltung

Verworfen, weil zu stark abhaengig von Akku, Reibung, Last und Wickelbild.

### Abschaltung nur bei Ueberstrom

Verworfen, weil das im schnellen Flug zu Fehlabschaltungen fuehrt.

### Komplexe Zusatzmechanik statt besserer Sensorik

Vorerst verworfen. Die vorhandene Mechanik gilt als brauchbar, die Verbesserung soll primaer elektronisch erfolgen.

### Sofort kompletter Umstieg auf interruptbasierte RC-Messung

Vertagt auf spaeteren Stand. Das ist sinnvoll, aber fuer den ersten V2.0-Hardwareaufbau nicht zwingend der erste Engpass.

### Endschalterlosen Werkbanktest in die Hauptfirmware mischen

Bewusst vermieden. Der temporaere Werkbanktest bleibt ein eigener Sketch, damit die Flug- und Sicherheitslogik der Hauptfirmware nicht aufgeweicht wird.

### Echter Schaltplan in KiCad ohne feste Teileliste

Vertagt, bis die konkreten Sensor- und Steckverbinder-Typen feststehen.

## Bekannte Probleme und Grenzen

- `pulseIn()` ist weiterhin nicht ideal und spaeter ein Kandidat fuer Ersatz.
- Die ACS712-Grenzwerte muessen am realen Aufbau eingemessen werden.
- Die pulsgesteuerte Endphase ist jetzt mit einem ersten echten Lernwert von `321` Pulsen verifiziert, muss aber draussen noch gegen einen kompletten Abwickeltest abgesichert werden.
- Die Default-Verdrahtung nimmt einen active-low Endschalter an; eine fail-safe-NC-Variante sollte spaeter geprueft werden.
- Die Hall-Pulszahl pro Umdrehung ist fuer den aktuellen Aufbau mit `2` Pulsen pro Trommelumdrehung bestaetigt.
- Fuer oeffentliche Nachnutzung fehlt noch eine explizite Lizenzentscheidung.
