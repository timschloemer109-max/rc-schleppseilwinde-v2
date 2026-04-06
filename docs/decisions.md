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
- vorgesehen sind `2 bis 4` Magnete

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

## Bewusst verworfene oder vertagte Loesungen

### Rein zeitbasierte Schnell-/Langsamumschaltung

Verworfen, weil zu stark abhaengig von Akku, Reibung, Last und Wickelbild.

### Abschaltung nur bei Ueberstrom

Verworfen, weil das im schnellen Flug zu Fehlabschaltungen fuehrt.

### Komplexe Zusatzmechanik statt besserer Sensorik

Vorerst verworfen. Die vorhandene Mechanik gilt als brauchbar, die Verbesserung soll primaer elektronisch erfolgen.

### Sofort kompletter Umstieg auf interruptbasierte RC-Messung

Vertagt auf spaeteren Stand. Das ist sinnvoll, aber fuer den ersten V2.0-Hardwareaufbau nicht zwingend der erste Engpass.

### Echter Schaltplan in KiCad ohne feste Teileliste

Vertagt, bis die konkreten Sensor- und Steckverbinder-Typen feststehen.

## Bekannte Probleme und Grenzen

- `pulseIn()` ist weiterhin nicht ideal und spaeter ein Kandidat fuer Ersatz.
- Die ACS712-Grenzwerte muessen am realen Aufbau eingemessen werden.
- Ohne gelernte Seillaenge gibt es noch keine vernuenftige pulsgesteuerte Endphase.
- Die Default-Verdrahtung nimmt einen active-low Endschalter an; eine fail-safe-NC-Variante sollte spaeter geprueft werden.
- Hall-Pulszahl pro Umdrehung ist noch nicht fest gemessen und muss dokumentiert werden.
- Fuer oeffentliche Nachnutzung fehlt noch eine explizite Lizenzentscheidung.
