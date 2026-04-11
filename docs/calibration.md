# Kalibrierung und Messprotokoll

Diese Datei sammelt die real gemessenen Werte des aktuellen Werkbankaufbaus. Sie ergaenzt `docs/hardware.md` und trennt feste Hardwareangaben von einstell- oder aufbauabhaengigen Messwerten.

## Testaufbau

- Arduino Nano mit `ATmega328P`
- Test-Sketch: `src/Schleppseilwinde_Testbench/Schleppseilwinde_Testbench.ino`
- Hall-Sensor: `KY-003` mit `A3144`
- Magnete: `2` Stueck, `180 Grad` versetzt, gleiche Polrichtung
- ESC: `Turnigy Plush 30A`
- Stromsensor: `ACS712`
- Endschalter beim Werkbanktest noch nicht montiert
- erster Motorlauf ohne Seil

## Bestaetigte Hall-Geometrie

- Handtest mit `5` vollen Trommelumdrehungen ergab `10` Hall-Pulse.
- Damit ist die Annahme `2` Hall-Pulse pro Trommelumdrehung fuer den aktuellen Aufbau bestaetigt.

## ACS712-Nullpunkt

- Vor dem ersten Motorlauf mit angeschlossenem Motorstrom wurde ein Nullpunkt von etwa `2.205 V` gemessen.
- Bei reinen Leerlaufmessungen nach dem Stillstand lagen die gefilterten Stromwerte typischerweise im Bereich von etwa `0.36 A` bis `0.43 A`.
- Die Vorzeichenrichtung des ACS712 ist im aktuellen Aufbau noch nicht normiert; negative Werte bedeuten hier daher nicht automatisch Rueckleistung oder einen Fehler.

## Anlauf- und Leerlaufwerte

- `1080 us`: noch keine Hall-Pulse beobachtet
- `1120 us`: erste reproduzierbare Hall-Pulse, vorsichtige Anlaufschwelle
- `1160 us`: stabilere langsame Testfahrt
- `1200 us`: gut reproduzierbarer Konstantlauf fuer Werkbanktests

## Sweep ohne Last

Messlauf:

- Kommando: `SWEEP 1120 1240 10 500`
- CSV-Log: temporär unter `%TEMP%`, Dateiname `seilwinde_sweep_20260411_175536.csv`

Beobachtung:

- Ab `1120 us` laeuft die Trommel reproduzierbar an.
- Zwischen `1120 us` und etwa `1170 us` lag die Pulsrate meist bei `10 pulses/s`.
- Ab etwa `1180 us` bis `1240 us` traten haeufig `20 pulses/s` auf.
- Bei `2` Pulsen pro Umdrehung entspricht das grob `300 rpm` bis `600 rpm`.
- Der gefilterte Strom blieb im Sweep unter etwa `0.7 A`.

## Konstantlauf bei `1200 us`

Messlauf:

- Kommando: `SET 1200`
- Haltezeit: mehrere Sekunden unterhalb der eingebauten Maximal-Laufzeit
- CSV-Log: temporär unter `%TEMP%`, Dateiname `seilwinde_hold1200_20260411_175918.csv`

Beobachtung:

- schneller und reproduzierbarer Anlauf
- danach recht stabile Pulsrate zwischen `10` und `20 pulses/s`
- entsprechend grob `300 rpm` bis `600 rpm`, im Mittel etwa `450 rpm`
- gefilterter Strom waehrend des Konstantlaufs grob `0.2 A` bis `0.45 A`
- kein Stall und kein Laufzeit-Failsafe im Testfenster

## Lasttest bei `1200 us`

Messlauf:

- Kommando: `SET 1200`
- Last: Trommel waehrend des Laufs vorsichtig von Hand gebremst
- CSV-Log: temporär unter `%TEMP%`, Dateiname `seilwinde_load1200_20260411_181100.csv`

Beobachtung:

- Die Trommel drehte trotz Last weiter, also keine echte Blockade.
- Der gefilterte Strom stieg unter Last deutlich an und erreichte grob bis etwa `1.8 A`.
- Die Pulsrate blieb unter Last meist bei einzelnen Impulsen mit laengeren Luecken, also deutlich langsamer als im Leerlauf.
- Es wurde kein Stall-Fault ausgeloest, weil unter Last weiterhin Hall-Impulse ankamen.
- Das Verhalten passt zur Sicherheitsregel des Projekts: hoher Strom allein fuehrt nicht zum Fehler.

## Stalltest bei `1200 us`

Messlauf:

- Kommando: `SET 1200`
- Last: Trommel kurzzeitig so stark gebremst, dass keine Hall-Impulse mehr ankamen
- CSV-Log: temporär unter `%TEMP%`, Dateiname `seilwinde_stall1200_20260411_181312.csv`

Beobachtung:

- Zunaechst kamen unter Last noch Hall-Impulse an.
- Danach blieb der Pulszaehler bei `276` stehen und `last_hall_ms` lief weiter hoch.
- Gleichzeitig stieg der gefilterte Strom deutlich an und erreichte vor dem Stopp grob `6.7 A`.
- Der Testbench-Sketch loeste darauf korrekt `EVENT,stop,stall_timeout` aus.
- Damit ist die Werkbank-Logik fuer `hoher Strom plus fehlende Hall-Impulse` im Grundsatz bestaetigt.
- Die nachlaufende Stromkurve nach dem Stopp zeigt die starke Filterung des ACS712-Signals; fuer spaetere Schwellwerte sollte deshalb immer mit echten Lastmessungen weiter kalibriert werden.

## Vorlaeufige Arbeitswerte fuer weitere Tests

- Hall-Pulse pro Umdrehung: `2`
- vorsichtige Anlaufschwelle: `1120 us`
- brauchsame langsame Testfahrt: `1160 us` bis `1180 us`
- zuegigere, aber noch kontrollierte Testfahrt: `1200 us` bis `1240 us`

## Offene Punkte fuer die naechsten Lasttests

- Stromwerte unter staerkerer und reproduzierbarer Last messen
- pruefen, wie stark die Pulsrate unter Last im Vergleich zum Leerlauf einbricht
- Vergleich zwischen hoher Last mit drehender Trommel und echtem Stall weiter absichern
- spaeter Schwellwerte fuer `MAX_CURRENT_A` und Stall sauber aus mehreren Messungen ableiten
