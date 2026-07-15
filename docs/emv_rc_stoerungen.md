# EMV- und RC-Stoerungen

Diese Datei sammelt das aktuelle Stoerthema rund um laufenden Verbrennungsmotor, Geisterdrehungen der Winde und kurzzeitige RC-Aussetzer.

## Sicherheitsstatus

- Neuer Stand vom 15.07.2026: Nahe am ESC wurden alle drei Adern der ESC-Leitung gemeinsam mehrfach durch einen Ferritkern gefuehrt; seitdem wurden keine Stoerungen oder Geisterdrehungen mehr beobachtet.
- Die frueheren Stoernachweise bleiben unten als Diagnosehistorie erhalten.
- Die Ferritloesung muss vor dem Flugbetrieb noch in wiederholten Motor-, Last- und Reichweitentests bestaetigt werden.
- Unabhaengig davon bestehen neue mechanische Defekte und eine auffaellige Motorerwaermung; der Gesamtaufbau ist deshalb noch nicht flugreif.
- Ziel bleibt: reproduzierbares Verhalten im Flug und kein unerwartetes Einziehen oder Anlaufen.

## Bisherige Beobachtungen

### 0. Aktueller Stoerverdacht: Killswitch der Zuendung

- Der urspruengliche Hauptverdacht lag auf dem Killswitch der Zuendung als Stoerquelle oder Stoerpfad.
- Nach Einbau eines neuen Killswitch treten weiterhin Stoerungen auf.
- Neuer Stand:
- Mit einem separaten Akku direkt an der Zuendung war `D4 -> ESC` im Vergleich fast sauber.
- Mit dem im Flieger verbauten Zuendakku samt verbauter Kabelstrecke treten auch ohne Killswitch, nur mit kurzer Verlaengerung direkt zur Zuendung, weiterhin Stoerungen auf.
- Damit ist der Killswitch allein nicht mehr die vollstaendige Erklaerung.
- Aktueller Hauptverdacht:
- die im Flieger verbaute Zuendakku-/Zuendversorgungskabelstrecke
- enge parallele Fuehrung von Zuendversorgung/Killswitch-Leitungen und Servo-/RC-Leitungen Richtung Motor
- eine bisher unbemerkte Verbindung oder Kopplung zwischen Zuendversorgungssystem und Empfaenger-/RC-System
- Geplante Pruefung:
- elektrische Verbindung zwischen Zuendakkusystem und Empfaengerakkusystem durchmessen
- Zuendversorgungskabel raeumlich von RC-/Servo-/Killswitch-Signalleitungen trennen und erneut mit ESP32-Sniffer vergleichen

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

### 10. Neuer Killswitch ersetzt, Stoerung bleibt bei verbauter Zuendakku-Kabelstrecke

- Ein neuer Killswitch wurde eingebaut.
- Die Stoerprobleme bestehen weiterhin.
- Wichtige Eingrenzung:
- Der fruehere saubere Test mit direkter Zundungsversorgung nutzte einen separaten Akku direkt an der Zuendung.
- Dieser Aufbau umging die im Flieger verbaute Zuendakku- und Kabelhardware.
- Mit dem im Flieger verbauten Zuendakku und dessen Kabelstrecke treten auch dann Stoerungen auf, wenn der Akku mit einer kurzen Verlaengerung direkt an die Zuendung angeschlossen wird und der Killswitch nicht im Strompfad liegt.
- Beobachtung am Kabelbaum:
- Servo- und Killswitch-Leitungen nach vorne zum Motor laufen einige Zentimeter eng parallel zur Versorgungsleitung fuer Zuendung/Killswitch.
- Aktuelle Arbeitsannahme:
- Die verbaute Zuendversorgungsleitung oder deren Fuehrung koppelt Stoerungen in die RC-/Servo-Signalumgebung ein.
- Eine direkte elektrische Verbindung zwischen Zuendakku- und Empfaengerakkusystem ist noch nicht nachgewiesen und soll gezielt durchgemessen werden.

## Aktuelle technische Bewertung

### Wahrscheinlichste Ursache

- EMV-Einkopplung auf ESC-Signalleitung
- oder verschobene Masse-Referenz zwischen Arduino, Empfaenger und ESC
- oder Stoerung ueber die 5-V-Versorgung
- oder Einkopplung von der verbauten Zuendakku-/Zuendversorgungsleitung in parallel gefuehrte RC-/Servo-Leitungen

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
- Ein neuer Killswitch allein behebt das Problem nicht.
- Die im Flieger verbaute Zuendakku- und Kabelstrecke bleibt auch ohne Killswitch als Stoerpfad auffaellig.

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
- Zuendakku-/Zuendversorgungsleitung getrennt von Servo-, RC- und Killswitch-Signalleitungen fuehren.
- Parallelfuehrungen zwischen Zuendversorgung und RC-/Servoleitungen vermeiden, besonders im vorderen Motorbereich.
- Wenn Kreuzungen unvermeidbar sind, Leitungen moeglichst kurz und rechtwinklig kreuzen.

### B. Einfache Hardware-Nachbesserungen

- `220` bis `470 Ohm` Serienwiderstand in die ESC-Signalleitung, moeglichst nah am Arduino.
- `4.7k` bis `10k` Pulldown von ESC-Signal nach `GND`, moeglichst nah am ESC.
- Ferrit auf ESC-Signalleitung pruefen.
- Ferrit oder zusaetzliche Beruhigung auf der 5-V-Zuleitung pruefen.
- Ferrit auf die Zuendakku-/Zuendversorgungsleitung pruefen, moeglichst nahe am Stoerpfad.
- Ferrit auf Servo-/RC-Buendel Richtung Motor pruefen, wenn die Leitungsfuehrung allein nicht reicht.

### C. Versorgung und Bezugspotential

- Test mit sauberer separater 5-V-Versorgung fuer Arduino und Empfaenger.
- Spannungsverhalten und Massebezug bei laufendem Verbrenner weiter beobachten.
- Zuendakku- und Empfaengerakku-System mit Multimeter auf unerwuenschte Verbindung durchmessen.
- Zwischen den Systemen sowohl Widerstand als auch moegliche Spannungspotentiale im ausgeschalteten und eingeschalteten Zustand pruefen.

### C2. Entstoeroptionen direkt an Winde, Arduino und ESC

- ESC-Signalleitung `D4` weiter als verdrilltes Paar mit Masse fuehren.
- Serienwiderstand `220 .. 470 Ohm` nahe am Arduino in `D4` einsetzen, falls noch nicht umgesetzt.
- Pulldown `4.7k .. 10k` von ESC-Signal nach `GND` nahe am ESC einsetzen, falls noch nicht umgesetzt.
- Zusaetzlichen Stuetzkondensator an der 5-V-Schiene nahe am Arduino pruefen, z. B. `100 nF` plus `47 .. 220 uF`.
- Hall-Sensor lokal mit `100 nF` direkt zwischen `VCC` und `GND` stuetzen.
- RC-Eingaenge `D3` und `D5` nur mit sauberer gemeinsamer Masse fuehren; bei Bedarf Signalleitung jeweils mit GND verdrillen.
- Arduino/Empfaenger-5-V-Leitung und ESC-/Motorleistung klar getrennt fuehren.
- Alle Massestellen sternfoermig bzw. mit klar definiertem Rueckweg pruefen; keine Motor- oder Zuendstroeme ueber Arduino-/Signalmasse fuehren.

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
- Nach Umlegen der Zuendversorgung den bestehenden ESP32-Messablauf wiederholen:
- `D4 -> ESC`
- Empfaengerkanal vor Arduino
- Killswitch-/Zuendungsnahe Messpunkte nach Bedarf

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
