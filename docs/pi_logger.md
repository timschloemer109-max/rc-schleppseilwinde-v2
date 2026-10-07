# Raspberry-Pi-Zero-2-W-Logger

Diese Datei beschreibt den passiven Logger und das vorgesehene CSV-Format für die Hauptfirmware. Codeabgleich am 2026-10-07: Der main-Sketch `src/Schleppseilwinde_V2_0/Schleppseilwinde_V2_0.ino` liefert Text-Debug, noch nicht das unten beschriebene CSV. Die Firmwareausgabe ist daher eine offene Entwicklungsaufgabe, kein bestätigter Ist-Stand.

## Zielbild

V3 implementiert CSV mit Bootheader, 250-ms-Telemetrie und Zustandsereignissen. Die historischen 23 Felder werden ergänzt um `schema_version,position_valid,remaining_mm,remaining_percent,power_enabled,light_warning,log_dropped,bus_errors,loop_max_us,actual_esc_output_us,hardware_inhibited`. Version `3.0`, fehlende Stromkalibrierung als leeres Feld. `esc_output_us` ist Steuerungs-Sollwert, `actual_esc_output_us` Adapter-Sollwert nach Sperren; keine elektrischen Messwerte. `rc_mode/rc_speed_us/rc_trigger_us` zeigen letzte plausible Buswerte, Gültigkeitsfelder tatsächlich akzeptierte Kanaldaten. Standardprofil sperrt Motor.

Pi-Parser akzeptiert V2-23 und V3-34 Spalten und normalisiert auf 34 plus UTC. Fehlende V2-Felder bleiben leer; unbekannte Versionen, Debug/Header/Teilzeilen werden verworfen. USB höchstens 32 Byte je loop, 2048-Byte-Ring verwirft bei Überlast ganze neue Zeilen. Ereignisse bestmöglich, `log_dropped` zeigt Verlust. Parser-Hosttests bestanden; echtes Pi/USB-Ende-zu-Ende offen. [V3-README](../entwicklung/seilwinde_v3/README.md), [Bericht](../entwicklung/seilwinde_v3/reports/test_report.md).

- Der Arduino bleibt allein fuer Steuerung und Sicherheitslogik verantwortlich.
- Der Pi Zero 2 W zeichnet nur die serielle CSV-Telemetrie mit Zeitstempel auf.
- Jede serielle Sitzung erzeugt eine eigene CSV-Datei.
- Der Abruf der Daten erfolgt spaeter ueber `SSH`, `scp` oder `sftp`.

## Grenzen

Für [V3.0](software_v3_0.md) sind auf Nano R4 USB-`Serial` für den Logger und `Serial1` für EX Bus getrennt zu prüfen. Ausgaben müssen zeitlich begrenzt sein; abgezogener USB und volle Puffer dürfen die Steuerung nicht blockieren. Der Pi hat eine eigene Versorgung; USB-Rückspeisung zur RC→5-V-Steuerungsversorgung vor Anschluss prüfen.

- Der Pi ersetzt keine Endschalter-, Strom- oder Failsafe-Funktion.
- Der Pi liest keine zusaetzlichen GPIOs als primaere Sicherheitsquelle ein.
- Die Testbench nutzt weiterhin ihr eigenes Werkbank-CSV und ist in diesem ersten Schritt nicht auf denselben Logger umgestellt.

## Verzeichnis im Repo

- Pi-Code: `pi/seilwinde-logger/`
- Projekt-Doku: `README.md`, `docs/hardware.md`, `docs/decisions.md`, `docs/todo.md`

## Zielsystem auf dem Pi

- Betriebssystem: `Raspberry Pi OS Lite (64-bit, Trixie)`
- Hostname: `seilwinde-logger`
- Benutzer: `seilwinde`
- Zeitzone: `Europe/Berlin`
- Logger-Code: `/opt/seilwinde-logger`
- Log-Verzeichnis: `/var/log/seilwinde`
- Dienst: `seilwinde-logger.service`

## Verkabelung

- Arduino Nano per USB-Kabel an den `USB`-Datenport des Pi Zero 2 W anschliessen.
- Nicht an `PWR IN` anschliessen.
- Der Pi braucht eine eigene stabile 5-V-Versorgung.
- Der Logger darf mechanisch und elektrisch ausfallen, ohne die Winden-Sicherheitslogik zu beeinflussen.

## Serielle Schnittstelle (Ziel nach Firmware-Implementierung)

- Baudrate: `115200`
- die Hauptfirmware sendet genau eine CSV-Kopfzeile nach dem Boot
- periodische Zeilen: `record_type = telemetry`
- Ereigniszeilen: `record_type = event`
- optionale Text-Debug-Zeilen beginnen mit `#` und werden vom Pi ignoriert

## CSV-Schema vom Arduino

```text
record_type,arduino_ms,run_state,stop_reason,esc_arm_state,rc_mode,rc_speed_us,rc_speed_valid,rc_trigger_us,rc_trigger_valid,esc_output_us,current_a,hall_pulse_count,delta_pulses,pulses_per_s,last_hall_age_ms,rope_pulses_avg,end_switch_active,end_switch_latched,fault_latched,learning_active,event_name,event_value
```

## CSV-Schema auf dem Pi

Der Pi schreibt denselben Datensatz plus einen UTC-Zeitstempel vorn an:

```text
host_utc,record_type,arduino_ms,run_state,stop_reason,esc_arm_state,rc_mode,rc_speed_us,rc_speed_valid,rc_trigger_us,rc_trigger_valid,esc_output_us,current_a,hall_pulse_count,delta_pulses,pulses_per_s,last_hall_age_ms,rope_pulses_avg,end_switch_active,end_switch_latched,fault_latched,learning_active,event_name,event_value
```

## Feste Event-Namen

- `boot`
- `run_start`
- `run_slow`
- `stop`
- `learn_start`
- `learn_accept`
- `learn_reject`
- `rc_signal_loss`

## Installation auf dem Pi

1. Raspberry Pi OS Lite auf die SD-Karte schreiben und SSH sowie WLAN im Imager aktivieren.
2. Den Pi booten und per SSH verbinden.
3. Dieses Repo auf den Pi kopieren oder klonen.
4. Im Verzeichnis `pi/seilwinde-logger/` `./install.sh` ausfuehren.
5. Mit `systemctl status seilwinde-logger` pruefen, ob der Dienst laeuft.

## Windows-Flash-Helfer

Im Repo liegt zusaetzlich ein vorbereiteter Windows-Helfer:

- `tools/prepare_pi_sd.ps1`

Der Helfer ist bewusst parameterisiert und schreibt keine echten WLAN- oder Login-Geheimnisse ins Repo. Er ist fuer die SD-Karte auf dem Windows-Laufwerk `E:` gedacht und erzeugt die noetigen `cloud-init`-Dateien fuer Hostname, Benutzer, WLAN und optionalen SSH-Key.

## Remote-Abruf

- Einzeldatei holen:

```bash
scp seilwinde@seilwinde-logger:/var/log/seilwinde/telemetry_20260416T120000Z.csv .
```

- Alle Logs holen:

```bash
scp seilwinde@seilwinde-logger:/var/log/seilwinde/telemetry_*.csv .
```

- Interaktiv durchsuchen:

```bash
sftp seilwinde@seilwinde-logger
```

## Betriebspruefung

- `systemctl status seilwinde-logger`
- `journalctl -u seilwinde-logger -n 50`
- `ls -lah /var/log/seilwinde`
- Nano abziehen und wieder anstecken: es muss eine neue Session-Datei entstehen
