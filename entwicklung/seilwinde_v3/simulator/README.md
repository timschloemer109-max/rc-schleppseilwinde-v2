# Deterministischer EX-Bus-Testmaster

`exbus_master.py` erzeugt deterministische Byte-Zeitlinien bei 125/250 kBaud. Seine
Paketbildung und CRC-Berechnung importieren keinen Firmwarecode. Die nativen Tests
prüfen den echten `core/exbus.cpp` gegen die eingecheckten Referenztelegramme.
Die Integrationstests im Gesamtbuild verbinden Protokoll und Steuerungskern.
Zeitlinien sind ein Hostmodell; elektrische UART-Pegel,
Leitungsfreigabe und reale Interruptlatenzen werden damit nicht emuliert.

Aus dem Repository-Root:

```powershell
python entwicklung/seilwinde_v3/simulator/exbus_master.py --self-test
python entwicklung/seilwinde_v3/simulator/exbus_master.py --scenario normal --baud 125000 --output entwicklung/seilwinde_v3/artifacts/master-normal.json
python entwicklung/seilwinde_v3/simulator/exbus_master.py --scenario loss --baud 250000 --output entwicklung/seilwinde_v3/artifacts/master-loss.json
python entwicklung/seilwinde_v3/simulator/exbus_master.py --scenario noise --output entwicklung/seilwinde_v3/artifacts/master-noise.json
python entwicklung/seilwinde_v3/simulator/exbus_master.py --scenario overload --output entwicklung/seilwinde_v3/artifacts/master-overload.json
python entwicklung/seilwinde_v3/tests/test_simulator.py --sanitize --compiler <clang++-Pfad>
python entwicklung/seilwinde_v3/build.py native --sanitize --compiler <clang++-Pfad>
```

Die Szenarien liefern Kanal-, Telemetrie- und JETIBOX-Anfragen mit wechselnden
Paket-IDs sowie reproduzierbarer Fragmentierung (Seed `0x5345494c`). `loss` setzt
Kanäle aus, während Telemetrie weitergeht; deren Empfang darf den RC-Failsafe nicht
auffrischen. `noise` ergänzt Header-/Längen-/CRC-Fehler, `overload` dichtes gültiges
Kanalaufkommen. Die Zeitlinien enthalten mindestens 4 s frische Neutralstellung,
anschließend RUN und abschließend Neutralstellung; damit übersteht der echte
Standardkern seine 3-s-Armingzeit vor der Startflanke. Jede Anfrage reserviert
anschließend das öffentliche 4-ms-Fenster.
Kanal 1 ist Geschwindigkeit, Kanal 2 neutral/RUN für diese Beispielzeitlinien.

Die Dateien `reference_telegrams.json` und `reference_telegrams.h` sind
eingecheckte Wahrheitsquellen. `--self-test` berechnet unabhängig die CRC16/CRC8
nach und prüft die Parität der beiden Dateien. Sieben Telegramme stammen wörtlich
aus den offiziellen Beispielen, vier sind unabhängig berechnete V3-Testvektoren.
`--write-fixtures --self-test` regeneriert sie absichtlich; diese Option gehört
nicht in eine automatische Prüfstrecke, die Änderungen sonst selbst akzeptierte.

`test_simulator.py` kompiliert `replay.cpp` zusammen mit den echten `core/`-
Quellen unter strengen Warnungen und standardmäßig Address-/UB-Sanitizer.
14 Replayfälle speisen die unabhängigen Masterbytes bei beiden Baudraten in die
echte Application: Normalbetrieb, Verlust trotz Telemetrie, Rauschen und dichtes
Aufkommen; zusätzlich Endschalter während RUN, Hall-/Stromwerte und volle
Loggerqueue. Signalrückkehr mit weiter aktivem RUN muss nach Verlust gesperrt
bleiben. Die Ergebnisdatei liegt unter `.build/simulator-sanitized/results.json`.
Compilerwahl wie bei `build.py`; lokales llvm-mingw unter `.build/tools/` wird bei
fehlendem PATH-Eintrag automatisch gefunden. `--no-sanitize` ist optional.

Der native Offline-Bridge versteht auf stdin `B <us> <byte>`,
`I <us> <hall-total> <end-active-0-or-1> <current-mA> <current-valid-0-or-1>`
und `T <us>`; Zeitstempel müssen aufsteigend sein. Der Steuerkern wird zwischen
Records mit 1-ms-Schritten und nach jedem Byte ausgeführt. Das ist injizierte
Modellzeit, kein Nachweis eines echten UART oder realer Endschalterlatenz.

Quellen und Grenzen stehen in [Protokollnachweis](../reports/exbus_protocol.md).
