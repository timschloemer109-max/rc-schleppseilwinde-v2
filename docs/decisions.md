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

Ergaenzung:

- die Lernfahrt wird aktuell nur uebernommen, wenn sie mindestens `70 %` des bisherigen Lernwerts erreicht
- damit werden halbe oder deutlich unvollstaendige Ausziehfahrten besser von echten Voll-Lernfahrten getrennt

### 10. Endphase nutzt beim Neustart den geschaetzten Restweg

Begruendung:

- ein Neustart mitten im Einfahrweg soll nicht wieder fast die komplette Fast-Phase fahren
- die langsame Endphase muss sich auch nach einem manuellen Abbruch noch am verbleibenden Weg orientieren

Ergaenzung:

- die Firmware fuehrt dafuer eine geschaetzte Seilposition relativ zum Endschalter mit
- die Umschaltung auf `RUN_SLOW` erfolgt aktuell bei etwa `87 %` des gelernten Vollwegs beziehungsweise beim Erreichen des entsprechenden Restwegs

### 9. Werkbank-Test als separater Sketch

Begruendung:

- der Testaufbau hat aktuell noch keinen Endschalter und keinen RC-Empfaenger
- damit bleibt die Hauptfirmware unvermischt und die Testabsicht eindeutig
- Hall, ESC und ACS712 koennen so isoliert und reproduzierbar vermessen werden

Ergaenzung:

- Testdatei ist `src/Schleppseilwinde_Testbench/Schleppseilwinde_Testbench.ino`
- Bedienung erfolgt seriell mit CSV-Ausgabe fuer Codex oder den Serial Monitor
- der Testbench-Sketch ist nur fuer beaufsichtigte Werkbanktests gedacht

### 11. ESC-Start nur mit Minimalsignal und Neutral-Freigabe

Begruendung:

- die Arduino-Servo-Library startet beim `attach()` standardmaessig mit `1500 us`
- dieser Initialpuls kann bei Brushless-Reglern zu uneinheitlichem Schaerfen oder sogar zum Programmiermodus fuehren
- beim Einschalten darf ein bereits aktiver `RUN`- oder `RESET/LEARN`-Schalter keinen Lauf ausloesen

Ergaenzung:

- die Firmware schreibt dem ESC-Objekt vor `attach()` bereits `1000 us`
- nach dem Einschalten bleibt die Winde fuer eine feste Arming-Zeit auf `1000 us`
- erst nach stabil erkannter Mittelstellung des Triggers wird `RUN` freigegeben
- gestartet wird danach nur auf einer echten Flanke `STANDBY -> RUN`
- `RESET/LEARN` wird ebenfalls erst nach dieser Neutral-Freigabe ausgewertet

### 12. RC-Signalverlust waehrend `RUN` wird ueber mehrere Samples entprellt

Begruendung:

- die aktuelle RC-Messung per `pulseIn()` kann unter EMV-Belastung kurze Einzel-Aussetzer liefern
- ein einmaliger ungueltiger Read darf nicht sofort einen gelatchten Fehlstopp ausloesen
- die Failsafe-Funktion bleibt erhalten, weil mehrere aufeinanderfolgende ungueltige Reads weiterhin sicher auf `RC_SIGNAL_LOSS` fuehren

Ergaenzung:

- `RC_SIGNAL_LOSS` wird im aktuellen Stand erst nach `3` aufeinanderfolgenden ungueltigen RC-Lesungen waehrend `RUN` gesetzt
- der Debug-Output zeigt dazu den aktuellen Fehlerburst als `rcLossCnt` und erfolgreich abgefangene Kurzstoerungen als `rcRecov`

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
- Die neue Restweg-Logik fuer Neustarts mitten im Einfahrweg ist implementiert, aber noch praktisch mit echten Abbruch- und Wiederanlauf-Szenarien zu bestaetigen.
- Die Default-Verdrahtung nimmt einen active-low Endschalter an; eine fail-safe-NC-Variante sollte spaeter geprueft werden.
- Die Hall-Pulszahl pro Umdrehung ist fuer den aktuellen Aufbau mit `2` Pulsen pro Trommelumdrehung bestaetigt.
- Die RC-Eingaenge werden weiterhin mit `pulseIn()` gelesen; die neue Neutral-Freigabe macht das Einschalten robuster, ersetzt aber keine spaetere interruptbasierte Messung.
- Auch mit der neuen RC-Loss-Entprellung bleiben `pulseIn()` und die Verdrahtung stoeranfaellig; die Massnahme ist eine Robustheitsverbesserung, kein Ersatz fuer saubere EMV-Fuehrung.
- Fuer oeffentliche Nachnutzung fehlt noch eine explizite Lizenzentscheidung.
