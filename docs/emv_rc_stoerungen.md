# EMV- und RC-Stoerungen

Diese Datei sammelt das aktuelle Stoerthema rund um laufenden Verbrennungsmotor, Geisterdrehungen der Winde und kurzzeitige RC-Aussetzer.

## Sicherheitsstatus

- Mit den aktuell beobachteten Stoerungen ist der Aufbau nicht flugreif.
- Solange Geisterdrehungen des Windenmotors bei laufendem Verbrenner auftreten, darf die Winde nicht als betriebssicher betrachtet werden.
- Kurzzeitige RC-Aussetzer koennen trotz Firmware-Entschaerfung weiterhin auf ein reales EMV-Problem im Aufbau hinweisen.
- Ziel bleibt: reproduzierbares Verhalten im Flug und kein unerwartetes Einziehen oder Anlaufen.

## Bisherige Beobachtungen

### 0. Aktueller Stoerverdacht: Killswitch der Zuendung

- Der aktuelle Hauptverdacht liegt auf dem Killswitch der Zuendung als Stoerquelle oder Stoerpfad.
- Naechster geplanter Vergleichstest:
- bisheriger Aufbau mit Killswitch im System
- gegen einen Test, bei dem die Zuendung testweise direkt vom Akku versorgt wird
- Ziel des Vergleichs:
- pruefen, ob sich die Stoerereignisse auf `D4 -> ESC` dadurch deutlich verringern oder verschwinden

### 1. Kurzzeitige RC-Aussetzer unter Motorstoerung

- Bei laufendem Verbrennungsmotor wurde ein vorzeitiger Stopp mit `reason=4` beobachtet.
- `reason=4` entspricht `RC_SIGNAL_LOSS`.
- Der Trigger stand dabei mechanisch weiter auf `RUN`.
- Wahrscheinlichkeitsbild: kurze ungueltige `pulseIn()`-Reads durch EMV, nicht bewusste Senderbewegung.

### 2. Firmware-Massnahme dagegen

- `RC_SIGNAL_LOSS` wird in der Hauptfirmware jetzt erst nach `3` aufeinanderfolgenden ungueltigen RC-Lesungen waehrend `RUN` gelatcht.
- Zusaetzliche Debug-Felder:
- `rcLossCnt`: aktueller Burst ungueltiger RC-Samples
- `rcRecov`: erfolgreich abgefangene Kurzstoerungen

### 3. Ergebnis nach Firmware-Anpassung

- Ein Einzieh-Test mit laufendem Verbrenner lief danach sauber bis zum Endschalter.
- Kein erneuter gelatchter `RC_SIGNAL_LOSS`.
- `rcRecov` stieg dabei auf `3`.
- Das zeigt: kurze Stoerungen sind noch vorhanden, fuehren aber nicht mehr sofort zum Fehlstopp.

### 4. Geisterdrehungen des Windenmotors bleiben bestehen

- Trotz stabilerer RC-Loss-Logik treten bei laufendem Verbrennungsmotor weiterhin Geisterdrehungen auf.
- Entscheidender Test:
- Wenn der ESC vom Arduino getrennt wird, verschwinden die Geisterdrehungen.
- Schlussfolgerung:
- Das Problem kommt sehr wahrscheinlich ueber den Pfad `Arduino -> ESC-Signal/GND`.
- Die Zustandsmaschine allein erklaert diese Bewegungen nicht.

### 5. Weitere Stoerindizien

- Im Stillstand wurden bereits falsche Hall-Pulse beobachtet.
- Das spricht dafuer, dass die Zuendanlage nicht nur RC, sondern mehrere Signale bzw. Referenzen im Aufbau stoert.
- In einem der Stoertests wurde der Arduino ueber `USB-5V` vom PC versorgt.
- Zum Empfaenger gingen dabei nur `GND` sowie die beiden RC-Signale fuer Trigger und Geschwindigkeit.
- Trotz dieser getrennten 5-V-Versorgung traten Stoerungen weiterhin auf.
- Das spricht gegen eine reine Stoerung ueber das ESC-BEC allein und staerkt den Verdacht auf Einkopplung ueber Signal- und Massebezuege.

### 6. ESP32-Sniffer-Nachweis direkt auf `D4 -> ESC`

- Mit dem Diagnosepaket `tools/esp32-logger` wurde das PWM-Signal `Arduino D4 -> ESC` passiv ueber einen `10k/10k`-Spannungsteiler auf einem ESP32 mitgeschnitten.
- Referenz mit Verbrenner aus:
- Datei: `logs/esp32_d4_motor_off_20260420_202147.csv`
- `1992` Datenzeilen, alle `OK`
- `0` `OUT_OF_RANGE`
- `0` `TIMEOUT`
- Pulsbereich nur `1001 .. 1011 us`
- Vergleich mit laufendem Verbrenner:
- Datei: `logs/esp32_d4_motor_on_20260420_201936.csv`
- `2001` Datenzeilen
- `1993` `OK`
- `7` `OUT_OF_RANGE`
- `1` `TIMEOUT`
- normale Pulse meist weiter bei etwa `1005 us`, aber einzelne Stoerereignisse bis `18245 us`
- Diese Stoerereignisse traten in einem Lauf auf, in dem der Windenmotor auch wieder kurz gezittert hat.
- Schlussfolgerung:
- Die Stoehrung ist nicht nur indirekt ueber das Fahrverhalten sichtbar, sondern direkt auf der ESC-Signalleitung `D4` messbar.

### 7. Vergleich mit direkter Zundungsversorgung

- Es wurde ein weiterer 60-s-Lauf mit laufendem Verbrenner aufgenommen, nachdem die Zuendung testweise direkt vom Akku versorgt wurde.
- Datei: `logs/esp32_d4_motor_on_direct_ignition_20260420_203212.csv`
- Ergebnis:
- `2989` Datenzeilen
- `2988` `OK`
- `0` `OUT_OF_RANGE`
- `1` `TIMEOUT`
- Pulsbereich nur `1002 .. 1012 us`
- Der eine `TIMEOUT`-Eintrag hat wieder ein unplausibles `period`-Feld (`4294967294`) und ist deshalb eher als Logger-/Ueberlauf-Artefakt zu werten als als harter Stoernachweis.
- Gegenueber den Laeufen mit Killswitch-Pfad ist das Signal damit deutlich sauberer.
- Arbeitsannahme:
- Der bisherige Killswitch oder sein Einbaupfad ist sehr wahrscheinlich Teil der Stoerursache oder koppelt die Stoerung stark in das ESC-Signal ein.

### 8. Stoernachweis schon auf dem Weg vom Empfaenger zum Arduino

- Der ESP32-Sniffer wurde danach auf einen Empfaengerkanal vor dem Arduino umgeklemmt.
- Referenz ohne Verbrenner:
- Datei: `logs/esp32_rx_channel_motor_off_20260420_204044.csv`
- `2144` `OK`
- nur ein einzelner `TIMEOUT` mit unplausiblem `period`-Feld (`4294967294`), also sehr wahrscheinlich derselbe Logger-/Ueberlauf-Artefakt wie in frueheren Referenzlaeufen
- Pulsbereich nur `1517 .. 1524 us`
- Periodendauer `13995 .. 14007 us`
- Vergleich mit laufendem Verbrenner und wieder angeschlossenem Killswitch:
- Datei: `logs/esp32_rx_channel_motor_on_killswitch_20260420_204232.csv`
- `4286` `OK`
- `11` `OUT_OF_RANGE`
- keine echten `TIMEOUT`
- normale Pulse meist weiter bei etwa `1520 us`, aber einzelne Stoerpulse bis `12117 us`
- dazu ein kurzer Fehlwert `632 us` bei `period=888`
- Schlussfolgerung:
- Die Stoerung ist nicht nur auf `D4 -> ESC` sichtbar, sondern bereits auf dem Weg vom Empfaenger zum Arduino messbar.
- Damit ist das Problem nicht erst ein Arduino-/ESC-Ausgangsproblem, sondern beeinflusst die RC-Signalumgebung selbst.

### 9. Messung direkt am Killswitch-Stecker Richtung Empfaenger

- Der ESP32-Sniffer wurde anschliessend direkt am Killswitch-Stecker auf der Seite zum Empfaenger angeschlossen.
- Datei: `logs/esp32_killswitch_rxplug_30s_20260420_205018.csv`
- Ergebnis:
- `2144` `OK`
- `2` `OUT_OF_RANGE`
- `0` `TIMEOUT`
- normales Signal bei etwa `2090 us`
- Stoerpulse:
- `12702 us`
- `3217 us`
- Zusaetzliche Beobachtung am realen Aufbau:
- Die Kontroll-LED des Killswitch ging waehrend des Stoerfalls sichtbar aus und wieder an.
- Schlussfolgerung:
- Der Killswitch ist nicht nur ein indirekter Verdachtskandidat, sondern zeigt selbst auffaelliges Verhalten und liefert auch direkt an seinem Empfaenger-Stecker messbare Stoerpulse.

## Aktuelle technische Bewertung

### Wahrscheinlichste Ursache

- EMV-Einkopplung auf ESC-Signalleitung
- oder verschobene Masse-Referenz zwischen Arduino, Empfaenger und ESC
- oder Stoerung ueber die 5-V-Versorgung

### Weniger wahrscheinlich

- legitimer `RUN` aus der Firmware
- reines Triggerproblem als alleinige Ursache fuer die Geisterdrehungen

## Was bereits verifiziert ist

- Endschalter bleibt harte Abschaltbedingung und funktioniert.
- Ein regulaerer Einzug bis Endschalter wurde mehrfach geloggt.
- Die neue RC-Loss-Entprellung reduziert Fehlstopps durch kurze Einzel-Aussetzer.
- Wird das ESC-Signal vom Arduino getrennt, verschwinden die Geisterdrehungen.
- Auch mit getrennter Arduino-Versorgung ueber `USB-5V` bleiben Stoerungen moeglich, solange Signal und gemeinsame Masse zum RC-/ESC-Pfad verbunden sind.
- Ohne laufenden Verbrenner ist `D4 -> ESC` im ESP32-Mitschnitt praktisch stoerfrei.
- Mit laufendem Verbrenner zeigen sich auf `D4 -> ESC` direkt `OUT_OF_RANGE`-Pulse und mindestens ein `TIMEOUT`.
- Mit direkter Zundungsversorgung ohne den bisherigen Killswitch-Pfad wird `D4 -> ESC` im 60-s-Vergleich fast vollstaendig sauber.
- Ohne laufenden Verbrenner ist auch der gemessene Empfaengerkanal vor dem Arduino praktisch stoerfrei.
- Mit laufendem Verbrenner und Killswitch zeigen sich auf dem Empfaengerkanal selbst bereits `OUT_OF_RANGE`-Stoerpulse.
- Direkt am Killswitch-Stecker Richtung Empfaenger wurden ebenfalls Stoerpulse gemessen.
- Die Killswitch-Kontroll-LED zeigte im Stoerfall sichtbares Fehlverhalten.

## Offene Risiken

- Unerwartetes kurzes Anlaufen der Winde im Flug
- erneute RC-Aussetzer unter staerkerer Stoerbelastung
- Stoereinkopplung nicht nur auf RC, sondern auch auf Hall- oder ESC-Signal
- scheinbar erfolgreicher Bodentest trotz verbleibender Sicherheitsreserve-Luecken

## Naechste Massnahmen in empfohlener Reihenfolge

### A. Leitungsfuehrung und Signalintegritaet

- ESC-Signal `D4` zusammen mit `GND` als verdrilltes Paar fuehren.
- Diese Leitung maximal weit weg von Zuendkabel, Zuendmodul und Motorleitungen legen.
- Gemeinsame Massefuehrung prufen, keine stoerenden Rueckstroeme ueber empfindliche Signalmasse fuehren.

### B. Einfache Hardware-Nachbesserungen

- `220` bis `470 Ohm` Serienwiderstand in die ESC-Signalleitung, moeglichst nah am Arduino.
- `4.7k` bis `10k` Pulldown von ESC-Signal nach `GND`, moeglichst nah am ESC.
- Ferrit auf ESC-Signalleitung pruefen.
- Ferrit oder zusaetzliche Beruhigung auf der 5-V-Zuleitung pruefen.

### C. Versorgung und Bezugspotential

- Test mit sauberer separater 5-V-Versorgung fuer Arduino und Empfaenger.
- Spannungsverhalten und Massebezug bei laufendem Verbrenner weiter beobachten.

### D. Weitere Messungen

- Wenn verfuegbar: ESC-Signal `D4` mit Oszi oder Logikanalysator messen.
- Fuer laengere Feldmessungen steht jetzt zusaetzlich ein ESP32-Sniffer-Paket unter `tools/esp32-logger` zur Verfuegung.
- Vergleichslauf mit direkter Zundungsversorgung ohne den bisherigen Killswitch-Pfad durchfuehren und gegen die vorhandenen Referenzdateien abgleichen.
- Besonderes Interesse:
- Signalform im Idle bei laufendem Verbrenner
- Stoerspitzen oder Pegelverschiebungen
- Verhalten waehrend Geisterdrehung
- Wiederholungsmessungen mit ESP32-Sniffer nach jeder Hardware-Aenderung direkt gegen die Referenzdateien vergleichen.
- Den Killswitch-Pfad nun gezielt in Teilstufen wieder einbauen, um den genauen Stoerpfad einzugrenzen.
- Falls noetig, direkt am Killswitch-Stecker messen, um zu unterscheiden zwischen Stoereinkopplung vor dem Modul, im Modul oder auf seinem Leitungsweg.
- Austausch gegen einen neuen Killswitch vorbereiten und den gleichen Messablauf danach wiederholen.

### E. Firmware nur als zweite Linie

- Keine weitere reine Firmware-Kosmetik als Hauptloesung fuer Geisterdrehungen einplanen.
- Das Restproblem sieht nach Hardware-/EMV-Thema aus.
- Ein moeglicher Test spaeter waere `ESC detach im IDLE`, aber nur vorsichtig und erst nach Hardware-Massnahmen, da manche ESCs auf Signalverlust unguenstig reagieren.

## Testziele vor weiterem Flugbetrieb

- Kein Geisterdrehen mehr bei laufendem Verbrennungsmotor
- Kein gelatchter `RC_SIGNAL_LOSS` mehr in wiederholten Einzieh-Tests
- `rcRecov` moeglichst niedrig und erklaerbar
- keine auffaelligen Hall-Fehlpulse im Stillstand
- wiederholbare Einzuege bis Endschalter ohne Reset-Zwischenschritt

## Praktische Merksaetze fuer die naechsten Tests

- Wenn die Geisterdrehung mit abgezogenem ESC-Signal verschwindet, liegt das Stoerproblem sehr wahrscheinlich im Signal-/Massepfad zum ESC.
- Wenn `rcRecov` hochzaehlt, sind weiterhin kurze RC-Stoerungen vorhanden, auch wenn die Fahrt sauber durchlaeuft.
- Ein erfolgreicher Endschalterlauf allein bedeutet noch nicht, dass das EMV-Thema geloest ist.
