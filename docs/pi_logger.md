# Raspberry-Pi-Zero-2-W-Logger

Diese Datei beschreibt den passiven Logger fuer die Hauptfirmware `src/Schleppseilwinde_V2_0/Schleppseilwinde_V2_0.ino`.

## Zielbild

- Der Arduino bleibt allein fuer Steuerung und Sicherheitslogik verantwortlich.
- Der Pi Zero 2 W zeichnet nur die serielle CSV-Telemetrie mit Zeitstempel auf.
- Jede serielle Sitzung erzeugt eine eigene CSV-Datei.
- Der Abruf der Daten erfolgt spaeter ueber `SSH`, `scp` oder `sftp`.

## Grenzen

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

## Inbetriebnahmestand 15.07.2026

- Raspberry Pi OS Lite 64-bit mit Debian 13 (`trixie`) ist installiert.
- Heim-WLAN und mobiler Hotspot sind als NetworkManager-Profile eingerichtet; das Heim-WLAN hat die hoehere Autoconnect-Prioritaet.
- Der Logger liegt unter `/opt/seilwinde-logger` und `seilwinde-logger.service` startet nach einem Reboot erfolgreich.
- Der temporaer fuer die Einrichtung verwendete USB-Gadget-Modus ist wieder deaktiviert; der USB-Datenport ist damit fuer den Arduino frei.
- Ohne angeschlossenen Arduino bleibt der Dienst aktiv und wartet erwartungsgemaess auf `/dev/ttyUSB*` oder `/dev/ttyACM*`.
- Offen ist der End-to-End-Test mit angeschlossenem Nano und einer echten CSV-Session.

## Verkabelung

- Arduino Nano per USB-Kabel an den `USB`-Datenport des Pi Zero 2 W anschliessen.
- Nicht an `PWR IN` anschliessen.
- Der Pi braucht eine eigene stabile 5-V-Versorgung.
- Der Logger darf mechanisch und elektrisch ausfallen, ohne die Winden-Sicherheitslogik zu beeinflussen.

## Serielle Schnittstelle

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

Optional kann bereits beim Schreiben ein zweites WLAN, beispielsweise ein mobiler Hotspot fuer den Feldeinsatz, mit `-SecondaryWifiSsid` und `-SecondaryWifiPassword` hinterlegt werden. Falls die Karte nach einem Schreibversuch keinen Laufwerksbuchstaben mehr besitzt, kann der zuvor eindeutig gepruefte USB-Datentraeger mit `-DiskNumber` angegeben werden. Der Helfer verweigert Boot-, System- und Nicht-USB-Datentraeger, laedt das aktuelle Lite-Image zuerst lokal herunter und prueft dessen SHA-256-Hash gegen die offizielle Raspberry-Pi-Pruefsumme.

Der vorbereitete Headless-Start aktiviert SSH ueber `enable_ssh: true` in `user-data`. Bei Bedarf kann zusaetzlich die klassische Datei `ssh` auf `bootfs` als redundanter Aktivierungsweg angelegt werden. Nach dem ersten Boot wird der Logger weiterhin wie unten beschrieben per SSH installiert; das Schreiben der SD-Karte allein installiert den Logger-Dienst noch nicht.

Beim verwendeten Trixie-Image meldete Cloud-Init das fehlende optionale Modul `cc_netplan_nm_patch`; die WLAN-Profile aus `network-config` wurden dadurch nicht in NetworkManager angelegt und WLAN blieb softwareseitig blockiert. Fuer diesen Pi wurden das WLAN-Land `DE`, die Funkfreigabe und beide Verbindungsprofile deshalb einmalig ueber den temporaeren USB-Gadget-Zugang gesetzt. Nach Abschalten des Gadget-Modus und einem Reboot kam der Pi selbststaendig ueber WLAN zurueck.

Unter Windows wurde das USB-Gadget zunaechst faelschlich als serielles Geraet gebunden. Der signierte offizielle `Raspberry Pi USB RNDIS Driver` band `VID 2E8A / PID 0013` korrekt als Netzwerkadapter. Der Gadget-Modus ist nur ein Einrichtungs- und Diagnoseweg und darf im Loggerbetrieb nicht aktiv bleiben, weil der USB-Datenport fuer den Arduino benoetigt wird.

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
