# Pi Logger

Diese Komponente laeuft auf dem Raspberry Pi Zero 2 W und schreibt die CSV-Telemetrie der Hauptfirmware mit einem zusaetzlichen UTC-Zeitstempel nach `/var/log/seilwinde`.

## Zielpfade auf dem Pi

- Code: `/opt/seilwinde-logger`
- Logs: `/var/log/seilwinde`
- Dienst: `seilwinde-logger.service`

## Installation auf dem Pi

1. Repo auf den Pi kopieren oder klonen.
2. In dieses Verzeichnis wechseln.
3. `./install.sh` ausfuehren.

Danach startet `systemd` den Logger automatisch beim Booten.

## Verhalten

- gesucht werden standardmaessig `/dev/ttyUSB*` und `/dev/ttyACM*`
- jede neue serielle Sitzung erzeugt eine neue Datei `telemetry_YYYYMMDDTHHMMSSZ.csv`
- Debug-Zeilen, leere Zeilen und die Arduino-Headerzeile werden ignoriert
- gespeichert wird nur `telemetry` oder `event` im vereinbarten CSV-Format

## Schnelltests

- Dienststatus: `systemctl status seilwinde-logger`
- letzte Meldungen: `journalctl -u seilwinde-logger -n 50`
- Logs anzeigen: `ls -lah /var/log/seilwinde`
- Datei holen: `scp seilwinde@seilwinde-logger:/var/log/seilwinde/telemetry_*.csv .`
