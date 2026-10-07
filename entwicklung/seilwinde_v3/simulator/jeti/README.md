# JETI-Studio-Senderemulator an V3 koppeln

`WinchV3.lua` ist eine **reine Emulator-App**. Sie liest Senderbedienelemente und
verbindet sich über TCP ausschließlich mit `127.0.0.1:9876`. Der Python-Host erzeugt
EX-Bus-Kanal-, JETIBOX- und Telemetrietelegramme für den tatsächlichen portablen
`Application`-/Steuerungskern. In diesem Prozess gibt es keinen physischen Motorpfad.
Die Lua-App deaktiviert sich auf einem physischen Sender (`emulator ~= 1`).

## Einrichtung

1. Die Windows-64-Bit-Version von der [Herstellerseite](https://www.jetimodel.com/support/jeti-studio/jeti-studio.html)
   installieren/entpacken. Hier liegt Studio 1.2.3 unter
   `entwicklung/seilwinde_v3/.build/jeti-studio/app/JETI-Studio.exe`.
   Unter `Tools` den DC/DS-Emulator starten. Geprüft wurde `dc-sim2.exe` mit
   emuliertem Senderstand 6.04; die App setzt Emulator-Lua ab V5.00 und LuaSocket
   voraus. Die [tatsächlichen GUI-Ergebnisse](../../reports/jeti_studio_test.md) sind
   getrennt von dieser Wiederholungsanleitung dokumentiert.
2. Im Emulator `File > Open data folder` benutzen. Hersteller-Standard unter
   Windows: `%LOCALAPPDATA%\JETI-Studio\Emulator`, hier
   `C:\Users\Tim\AppData\Local\JETI-Studio\Emulator`.
   Maßgeblich ist der vom laufenden Emulator geöffnete Datenordner.
3. Ein eigenes virtuelles Testmodell anlegen, zum Beispiel `A`. `WinchV3.lua`
   in den Unterordner `Apps` kopieren. Beispiel aus dem Repository-Root für den
   oben genannten Windows-Standardpfad:

   ```powershell
   $winchEmulatorApps = Join-Path $env:LOCALAPPDATA 'JETI-Studio\Emulator\Apps'
   Copy-Item -LiteralPath 'entwicklung/seilwinde_v3/simulator/jeti/WinchV3.lua' -Destination $winchEmulatorApps
   ```

   Die App wird pro Sendermodell aktiviert. Nach Änderungen an der Lua-Datei in
   `User Applications > CMD` mit `Refresh` neu laden oder den Emulator neu starten.
4. Im Repository-Root den Host starten:

   ```powershell
   python entwicklung/seilwinde_v3/simulator/jeti_bridge.py --log entwicklung/seilwinde_v3/artifacts/jeti-studio/gui-session.jsonl
   ```

   Der Host baut den C++-Helfer beim Start. Optional `--compiler <voller Pfad zu
   clang++>` angeben; ohne Argument sucht er auch den lokalen LLVM-MinGW-Compiler
   unter `.build/tools/`. Standardmäßig laufen Address-/Undefined-Behavior-Sanitizer.
   `--log` schreibt die begrenzte JSONL-Testaufzeichnung; ohne diesen optionalen
   Parameter läuft dieselbe Bridge ohne Evidenzdatei. Erst fortfahren, wenn der
   Host `listening on 127.0.0.1:9876` meldet. Falls `--port` verwendet wird, beide
   Portliterale in Lua ebenfalls ändern; die Adresse bleibt Loopback.
5. Im Emulator `Applications > User Applications`, `+` und `Winch V3 virtual`
   aktivieren. Die CMD-Konsole zeigt Emulatorflag, Lua-/LuaSocket-Versionsstring,
   Eingangsbelegung und Verbindungs-/Zustandswechsel. Die tatsächliche Konsole
   bestätigte Senderstand `6.04`, `Lua 5.3`, `integer-max=2147483647`,
   `LuaSocket 3.0-rc1` und eine leere COM-Portliste. `CONNECTED` bestätigt den
   Hostkontakt über TCP; ein COM-Port ist dafür nicht nötig.
6. Die **getestete Ansicht** über `Applications > Winch V3 virtual` öffnen.
   Nach Aktivierung erscheint dieser eigene Menüeintrag im Applications-Menü.
   Das App-Formular bietet die vier Menütasten, Endschalter-/Pause-Checkboxen
   und den virtuellen Hall-Link; seine Anzeige kommt aus den tatsächlichen
   Hostantworten. Im GUI-Test wechselte SG nach `Rope metres`, danach zeigte
   ein F4-Doppelklick `FAST max us`: Der echte JETIBOX-Menüpfad funktioniert.

   Zusätzlich registriert die App ein ganzseitiges Lua-Telemetriefenster
   `Winch V3 virtual`. Dessen optionale Auswahl unter
   `Timers/Sensors > Displayed Telemetry` ist eine API-basierte Einrichtung;
   **dieser Auswahlpfad wurde im GUI-Test nicht ausprobiert**. Das registrierte
   Fenster zeichnet Hostzustand/PWM, RC-Frische, JETIBOX-Text und dekodierte EX-Werte.

Beim ersten Start blieb der lokale Emulator an einer Einschalt-/Timeout-Abfrage
hängen. Gelöst wurde dies an seiner **neu erzeugten lokalen Emulator-Konfiguration**:
Emulator schließen und im genannten Datenordner `Config.jsn` den vorhandenen
JSON-Wert `"fast-Start":0` auf `"fast-Start":1` ändern, danach erneut starten.
Die übrigen Einstellungen erhalten. Diese Einstellung betrifft den Simulatorstart,
nicht die Motorfreigabe oder Sicherheitslogik. Ein GUI-Menüpfad für diese Änderung
wurde in dieser Sitzung nicht geprüft.

## Bedienung

| Bedienung | Wirkung im virtuellen Test |
| --- | --- |
| P1 | Speed: −100 % → 1000 µs, 0 % → 1500 µs, +100 % → 2000 µs |
| SA (Dreistufenschalter) | −100 % LEARN, Mitte Neutral, +100 % RUN |
| SB / SC | virtuelle Licht-/Leistungskanäle; Hardwareadapter bleiben deaktiviert |
| SD > 50 % | simulierten Endschalter betätigen |
| SE > 50 % | TCP schließen, keine Kanalproben senden; Host tickt weiter |
| SF / SG neue positive Flanke | JETIBOX-Seite links / rechts |
| App-Formular F1–F4 | JETIBOX links / oben / unten / rechts |
| Formular „Virtual end stop“ | zusätzlichen virtuellen Endschalter setzen |
| Formular „Pause / lose RC“ | Verbindung pausieren, Wirkung wie SE |
| Formular Hall-Link | einen einzelnen simulierten Hall-Impuls zufügen |

Die Schalterwerte werden im Emulator über die sichtbaren Bedienelemente gesetzt:
SA braucht alle drei Zustände −1, 0, +1. Für den Start SA auf Mitte und SD/SE auf
0 beziehungsweise −1 setzen. Der Defaultkern verlangt zunächst 3 s Arming und
250 ms stabile Neutralstellung. P1 steuert die angeforderte FAST-Geschwindigkeit;
bei noch unbekannter Seillänge/Position fährt der Kern sicher mit SLOW 1280 µs.

Das App-Formular wird über `Applications > Winch V3 virtual` geöffnet. SD/SE und
Formularschalter sind jeweils ODER-verknüpft. Zum Rücksetzen beide Quellen lösen.
Wiederverbindung umgeht weder Neutralfreigabe noch erforderliche neue RUN-Flanke.
Nach Fehlern gemäß Steuerungskern quittieren; ein gehaltener RUN-Schalter darf
keinen automatischen Wiederanlauf verursachen. Hall-Injektion ist ein einzelner
virtueller Sensorimpuls, kein Motor-/Trommelmodell und keine Richtungsbestimmung.

Während `PAUSED` ist TCP absichtlich geschlossen. Das Lua-Fenster markiert RC als
`STALE` und zeigt gegebenenfalls den **letzten empfangenen** Zustand/PWM-Wert.
Der tatsächliche Failsafe-Stopp während dieser Unterbrechung ist im unabhängigen
Hostlog nachzuweisen. Nach Wiederverbindung ist wieder der aktuelle Hostzustand
sichtbar. Gespeicherte alte Bildschirmwerte sind kein aktueller Motorbefehl.

Die Konstanten `INPUTS` verwenden P1/SA/SB/SC direkt, damit ein leeres
Emulatormodell testbar ist. Zum Prüfen der wirklichen Modellzuordnung/Mischer
stattdessen `{"O1", "O2", "O3", "O4"}` konfigurieren und diese Servokanäle im
Sendermodell belegen. Fehlende Eingänge erzeugen **keine** erfundenen gültigen
Proben. Die Anzeige meldet `INPUT MISSING`; RC-Frische läuft auf dem Host ab.

## Virtuellen Sicherheitstest wiederholen

Für einen frischen Hostzustand die Bridge vor Beginn neu starten. Ein Neustart
allein des Lua-Frontends setzt den laufenden Steuerungskern nicht zurück.

1. SA=0, SD=0, SE=0 halten, Verbindung aufbauen und mindestens 3,5 s warten.
   Erwartet: `idle`, 1000 µs, RC frisch, Armstatus 3.
2. SA=+1 setzen und die Rampe abwarten. Bei ungelearnten Defaults:
   `slow`, 1280 µs. Das sind virtuelle PWM-Befehle, keine Bewegung eines Motors.
3. SA zurück auf 0: autonomes Einziehen muss weiterlaufen. SD=+1: Stopp auf
   1000 µs und `end_latched=1`. SD zurück auf 0 darf keinen Neustart auslösen.
4. SA=−1 setzen, ohne Hall-Injektion wieder auf 0: Lernen wird als
   `learn_rejected` abgelehnt; keine gültige neue Seillänge entsteht.
5. Nach gültiger Neutralphase SA=+1: erneuten aktiven Lauf bestätigen.
6. Bei weiter SA=+1 SE=+1 setzen. Die Verbindung schließt; im JSONL-Hostlog muss
   `rc_lost`, Zustand `fault` und 1000 µs erscheinen. Die Zeit ist ein Hostnachweis,
   keine gemessene Abschaltlatenz der Nano-R4-Hardware.
7. SE zurück auf 0, SA weiter +1: frische Kanäle kehren zurück, aber `fault`,
   1000 µs und Armstatus 2 bleiben. Fehlerquittierung und neue Neutral-/RUN-Folge
   müssen explizit erfolgen; Signalwiederkehr alleine darf nie starten.

Eine vollständige Lernfunktion mit Hall-Pulsen, FAST/SLOW-Umschaltung und alle
Protokollfehlerfälle bleiben zusätzlich Gegenstand der deterministischen Tests.
Dieser kurze GUI-Test ersetzt deren Abdeckung nicht.

## Unterstützte API und Grenzen

Die offizielle [JETI Lua API 1.5](https://github.com/JETImodel/Lua-Apps/blob/master/Doc/JETI%20DCDS_Lua_API_1.5.pdf)
beschreibt im Emulator vorinstalliertes LuaSocket (S. 6), Emulatorflag (S. 34),
Servoausgänge `O1`–`O24` und Sticks/Schalter (S. 43), ganzseitige Lua-Telemetrie
(S. 51), Formulare (S. 54), Checkbox-Callbacks (S. 124/125) sowie physische/virtuelle
COM-Ports (S. 151). Hier wird TCP verwendet, kein zusätzlicher COM-Treiber.
[LuaSocket](https://lunarmodules.github.io/luasocket/tcp.html) liefert nicht
blockierende `settimeout(0)`-I/O. Die App behandelt Teilübertragungen, begrenzt
Puffer/Schleifen und verwirft unplausible Hostantworten. Im Formular werden
Checkboxen gemäß Herstellerbeispiel invertiert und mit `form.setValue` bestätigt.
Der uint32-Wertebereich wird mit einer expliziten Fließkommagrenze geprüft, damit
`0xffffffff` in einem Lua-Build mit 32-Bit-Integer nicht zu `-1` überläuft. Die
Konsole gibt Lua-Version und `math.maxinteger` zur Überprüfung aus. Die virtuelle
Hall-Taste ist auf den positiven 32-Bit-Integerbereich begrenzt.

Die Lua-Schleife läuft laut JETI-API ungefähr alle 20–30 ms ohne Zeitgarantie.
Die App sendet frühestens alle 30 ms eine neu gelesene, fortlaufend nummerierte
Probe. Alte/wiederholte Sequenzen erneuern RC-Frische im Host nicht. Nach
Verbindungsabbruch läuft dessen Sicherheitszeit unabhängig weiter.

Das Lua-Fenster zeigt validierte Antworten des tatsächlichen EX-Bus-Kerns.
Es speist diese Werte **nicht** als native EX-Sensoren in `system.getSensors()` ein.
Die Hersteller-API dokumentiert keinen solchen Injektionspfad und keinen nativen
Empfänger-/EX-Bus-Anschluss des Emulators. Die Lua-Anzeige beweist daher keine
originale JETI-EX-Telemetriedekodierung, keinen JETIBOX-Funktransport, elektrische
Pegel, UART-Turnaround, Funkverlust oder RA4M1-Peripherie. Diese Prüfungen bleiben
am echten Aufbau erforderlich.
