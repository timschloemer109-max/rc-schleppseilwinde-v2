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

## Endschaltertest in der Hauptfirmware

Messlauf:

- Hauptfirmware `src/Schleppseilwinde_V2_0/Schleppseilwinde_V2_0.ino`
- Endschalter an `D7`, active-low gegen `GND`
- RC-Eingaenge fuer Trigger und Geschwindigkeit waren gueltig angeschlossen

Beobachtung:

- Beim Betaetigen des Endschalters sprang `endLatch` im Debug-Output von `0` auf `1`.
- `reason=1` bestaetigte den Stoppgrund `END_SWITCH`.
- Die Verriegelung blieb nach dem Loslassen des Schalters gesetzt, wie vorgesehen.
- Ein anschliessender kurzer `RESET/LEARN`-Vorgang setzte `endLatch` wieder auf `0`.
- Der kurze `RESET/LEARN`-Impuls fuehrte wegen `pulses=0` und zu kurzer Dauer zu keinem neuen Lernwert.

Ergebnis:

- Der Endschalter funktioniert elektrisch und logisch wie vorgesehen.
- Die Ruecksetzung ueber `RESET/LEARN` ist fuer den aktuellen Stand bestaetigt.

## Laufzeit-Failsafe der Hauptfirmware

- Der urspruengliche Wert von `20000 ms` war fuer das sehr langsame Aufwickeln mit losem, noch nicht sauber ausgelegtem Seil zu knapp.
- Fuer den aktuellen Zwischenstand wurde `MAX_RUN_TIME_MS` in der Hauptfirmware auf `60000 ms` erhoeht.
- Die Laufzeitbegrenzung bleibt damit aktiv, ist aber fuer den weiteren Werkbankaufbau grosszuegiger.

## Erstes langsames Aufwickeln mit Seil

Messlauf:

- Hauptfirmware `src/Schleppseilwinde_V2_0/Schleppseilwinde_V2_0.ino`
- Speed-Kanal im Bereich etwa `1200 us` bis `1230 us`
- Trigger auf `RUN`
- Endschalter aktiv angeschlossen

Beobachtung:

- Das Seil konnte langsam weiter aufgewickelt werden.
- Der Pulszaehler stieg im Lauf bis auf `122`.
- Der Endschalter stoppte den Vorgang am Ende sauber.
- Nach dem Lauf stand der Debug-Status auf `endLatch=1`, `faultLatch=0`, `reason=1`.
- Damit wurde der reale Aufrollvorgang ueber den Endschalter beendet und nicht ueber einen Fehler oder den Laufzeit-Failsafe.

Ergebnis:

- Der erste echte Seil-Aufrolltest mit angeschlossenem Endschalter war erfolgreich.
- Die Endschalterlogik funktioniert auch im realen Bewegungsablauf.

## Erste Lernfahrt mit voller Abrollstrecke

Messlauf:

- Hauptfirmware `src/Schleppseilwinde_V2_0/Schleppseilwinde_V2_0.ino`
- Trigger auf `RESET/LEARN`
- Seil vollstaendig von Hand ausgezogen
- Lernfahrt durch Rueckkehr des Triggers in die Mitte abgeschlossen

Beobachtung:

- Die Hall-Pulse wurden ueber die gesamte Ausziehstrecke sauber gezaehlt.
- Nach Abschluss stand `ropeAvg` stabil auf `321.0`.
- Die Steuerung war anschliessend wieder in `rcMode=0`, `state=0`, ohne Endschalter- oder Fault-Latch.

Ergebnis:

- Der erste echte Lernwert fuer die Seillaenge wurde erfolgreich gespeichert.
- Fuer den aktuellen Aufbau gilt damit ein gelernter Referenzwert von `321` Hall-Pulsen.

## Erster Einfahrtest mit aktivem Lernwert

Messlauf:

- Hauptfirmware `src/Schleppseilwinde_V2_0/Schleppseilwinde_V2_0.ino`
- gespeicherter Lernwert `ropeAvg = 321`
- Speed-Kanal waehrend des Laufs etwa `1380 us` bis `1400 us`
- Beobachtung bis zum Erreichen der ersten Endbedingung

Beobachtung:

- `RUN` startete bei `pulses=321`.
- Der Wechsel `RUN_FAST -> RUN_SLOW` erfolgte bei `pulses=579`.
- Das entspricht `258` Pulsen seit Start.
- Die Umschaltung passt damit praktisch exakt zum Sollwert `321 * 0.80 = 256.8`.
- Der Lauf endete regulaer am Endschalter bei `pulses=639`.
- Das entspricht `318` Pulsen seit Start und liegt damit nur knapp unter dem gelernten Wert.

Ergebnis:

- Die pulsgesteuerte Umschaltung auf die langsame Endphase funktioniert mit echtem Lernwert wie vorgesehen.
- Der reale Einfahrweg passt sehr gut zum gelernten Ausziehwert.
- Der Endschalter beendet den Einfahrvorgang weiterhin sauber als harte Stop-Bedingung.

## Einfahrtest mit Rueckkehr des Triggers in die Mitte

Messlauf:

- Hauptfirmware `src/Schleppseilwinde_V2_0/Schleppseilwinde_V2_0.ino`
- gespeicherter Lernwert `ropeAvg = 320.4`
- Start bei `pulses=960`
- Beobachtung bis zum Erreichen der ersten Endbedingung

Beobachtung:

- `RUN` startete bei `pulses=960`.
- Der Trigger wurde waehrend des Laufs in die Mitte zurueckgenommen.
- Trotz `rcMode=0` lief die Winde weiter, zunaechst in `state=1` und spaeter nach Umschaltung in `state=2`.
- Der Wechsel `RUN_FAST -> RUN_SLOW` erfolgte bei `pulses=1216`.
- Das entspricht `256` Pulsen seit Start und passt damit exakt zu `320.4 * 0.80`.
- Der Lauf endete regulaer am Endschalter bei `pulses=1293`.
- Das entspricht `333` Pulsen seit Start.

Ergebnis:

- Die Winde laeuft nach Rueckkehr des Triggers in die Mittelstellung autonom weiter.
- Die pulsgesteuerte Umschaltung auf `RUN_SLOW` funktioniert weiterhin mit dem geglaetteten Lernwert.
- Auch in diesem Ablauf bleibt der Endschalter die erste harte Endbedingung.

## Beobachtungsregel fuer Fahrtests

- Bei Live-Beobachtungen werden Fahrten im Debug-Log bis zur ersten Endbedingung verfolgt.
- Als Endbedingungen gelten dabei insbesondere Endschalter-Stopp, Fault-Latch oder sonstige Stop-Gruende.
- Dadurch lassen sich Umschaltpunkt, Laufverhalten und reale Beendigungsursache je Durchgang klar vergleichen.
- Beim Abwickeln wird standardmaessig so lange beobachtet, bis der Trigger wieder in der Mittelstellung steht, sofern vor dem Test nichts anderes vereinbart wurde.

## RC-Signalverlust waehrend des Einfahrens

Messlauf:

- Hauptfirmware `src/Schleppseilwinde_V2_0/Schleppseilwinde_V2_0.ino`
- gespeicherter Lernwert `ropeAvg = 320.9`
- Start bei `pulses=1616`
- RC-Signalverlust absichtlich waehrend `RUN_FAST` provoziert

Beobachtung:

- `RUN` startete regulaer und die Winde lief zunaechst normal an.
- Der Pulszaehler stieg dabei von `1616` auf `1668`.
- Nach dem ungültigen RC-Zustand meldete die Firmware `Stop: RC-Signal ungueltig`.
- Der Stopp erfolgte noch vor der Umschaltung auf `RUN_SLOW`.

Ergebnis:

- Die Hauptfirmware reagiert waehrend des Laufs korrekt auf RC-Signalverlust.
- Der Antrieb wird in diesem Fall sicher gestoppt und nicht weiter autonom fortgesetzt.

## Kurze Lernfahrt unterhalb der neuen Schwelle

Messlauf:

- Hauptfirmware `src/Schleppseilwinde_V2_0/Schleppseilwinde_V2_0.ino`
- gespeicherter Lernwert `ropeAvg = 320.9`
- absichtlich kurzer `RESET/LEARN`-Vorgang mit nur kleinem Ausziehweg

Beobachtung:

- Die Lernfahrt wurde nach etwa `4909 ms` beendet.
- Es wurden dabei nur `16` Pulse als Lernstrecke erkannt.
- Der Debug-Output meldete `Lernwert verworfen. pulses=16 durationMs=4909 durationOk=1 pulsesOk=0 relativeOk=0`.

Ergebnis:

- Die zu kurze Lernfahrt wurde korrekt nicht als neue Seillaenge uebernommen.
- Sowohl die Mindestpulszahl als auch die neue relative Schwelle griffen wie gewuenscht.

## Neustart mitten im Einfahrweg mit Restweg-Logik

Messlauf:

- Hauptfirmware `src/Schleppseilwinde_V2_0/Schleppseilwinde_V2_0.ino`
- gespeicherter Lernwert `ropeAvg = 320.9`
- bekannte Startposition nach Teil-Auszug: `pos = 143.0`
- erster `RUN`, danach manueller Abbruch ueber `RESET/LEARN`
- anschliessend zweiter `RUN` bis zum Endschalter

Beobachtung:

- Vor dem ersten `RUN` stand die geschaetzte Position auf `143.0`.
- Beim manuellen Abbruch lag die geschaetzte Restposition bei etwa `63.0`.
- Nach dem kurzen `RESET/LEARN` und dem Verwerfen der Mini-Lernfahrt stand die Position vor dem zweiten Start bei `65.0`.
- Beim zweiten `RUN` erfolgte `Wechsel auf SLOW` bereits bei `pos=41.0`.
- Die langsame Endphase begann damit deutlich frueher als bei einem Vollstart von weiter aussen.
- Der zweite Lauf endete regulaer am Endschalter bei `pos=0.0`, `endLatch=1`, `reason=1`.

Ergebnis:

- Die neue Restweg-Logik funktioniert im realen Restart-Szenario.
- Nach einem manuellen Abbruch mitten im Einfahrweg wird der verbleibende Weg korrekt weiterverwendet.
- Ein Neustart behandelt den Restweg damit nicht mehr wie einen kompletten neuen Vollweg.

## EEPROM- und Neustarttest der gespeicherten Seillaenge

Messlauf:

- Arduino kurz von USB getrennt und wieder verbunden
- Hauptfirmware startet anschliessend neu
- Auswertung des Startlogs direkt nach dem Reboot

Beobachtung:

- Im Startlog wurde `Rope avg pulses: 320.9` ausgegeben.
- Auch die anschliessenden Debug-Zeilen zeigten weiter `ropeAvg=320.9`.
- Beim Neustart stand die Winde am Endschalter, daher war direkt `pos=0.0`, `endLatch=1`, `reason=1` sichtbar.
- Obwohl der Trigger bereits auf `RUN` stand, startete die Winde dadurch nicht ungewollt neu.

Ergebnis:

- Die gespeicherte Seillaenge wird nach einem Arduino-Neustart korrekt aus dem EEPROM geladen.
- Der Neustart an aktiver Endlage fuehrt nicht zu einem unbeabsichtigten Wiederanlauf.

## Rasentest auf Rasen mit neuem Lernwert

Messlauf:

- Hauptfirmware `src/Schleppseilwinde_V2_0/Schleppseilwinde_V2_0.ino`
- zunaechst `RESET/LEARN` auf Rasen mit vollstaendigem manuellem Ausziehen
- uebernommener Ausziehwert `337` Pulse
- geglaetteter Lernwert danach `ropeAvg = 347.1`
- anschliessend `RUN` bis zur ersten Endbedingung beobachtet

Beobachtung:

- Die Lernfahrt wurde mit `Lernwert uebernommen: 337 Pulse, Mittelwert: 347.1` sauber abgeschlossen.
- Vor dem Einziehen lag die geschaetzte Startposition bei etwa `pos=337.0`.
- Der erste Fahr-Logeintrag in `RUN_FAST` lag bei `pulses=338`, `pos=336.0`.
- Der hoechste beobachtete Strombetrag lag bei etwa `5.03 A` in `RUN_FAST` bei `pulses=512`, `pos=162.0`.
- Der Wechsel `RUN_FAST -> RUN_SLOW` erfolgte bei `pulses=630`, `pos=44.0`.
- Das entspricht noch rund `12.7 %` Restweg relativ zum geglaetteten Lernwert `347.1`.
- Der Lauf endete regulaer am Endschalter bei `pulses=673`, `pos=0.0`, `endLatch=1`, `reason=1`.
- Ein `faultLatch` trat im gesamten Rasentest nicht auf.

Ergebnis:

- Das manuelle Ausziehen auf Rasen wurde sauber als neue Lernfahrt uebernommen.
- Der anschliessende Einziehvorgang lief mit dem neuen Lernwert stabil bis zum Endschalter durch.
- Auch mit kurzzeitig bis etwa `5 A` beobachtetem Motorstrom blieb die Winde in diesem Test frei von Fehlabschaltung.

## Vorlaeufige Arbeitswerte fuer weitere Tests

- Hall-Pulse pro Umdrehung: `2`
- vorsichtige Anlaufschwelle: `1120 us`
- bisher brauchsame langsame Testfahrt auf dem Werkbankstand: `1160 us` bis `1180 us`
- aktueller Hauptfirmware-Wert fuer mehr Drehmoment in `RUN_SLOW`: `1280 us`
- zuegigere, aber noch kontrollierte Testfahrt: `1200 us` bis `1240 us`

## Offene Punkte fuer die naechsten Lasttests

- Stromwerte unter staerkerer und reproduzierbarer Last messen
- pruefen, wie stark die Pulsrate unter Last im Vergleich zum Leerlauf einbricht
- Vergleich zwischen hoher Last mit drehender Trommel und echtem Stall weiter absichern
- spaeter Schwellwerte fuer `MAX_CURRENT_A` und Stall sauber aus mehreren Messungen ableiten
- weitere Aufrolltests mit sauber ausgelegtem Seil dokumentieren
- weitere Abwickeltests im Freifeld mit Laengenvergleich gegen den Lernwert dokumentieren
