#!/usr/bin/env python3
"""Compile/link-only API qualification. Never opens serial, uploads or runs hardware."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(os.path.abspath(__file__)).parents[1]  # Preserve optional SUBST alias.
SKETCH = '''#include <Arduino.h>
#include <EEPROM.h>
#include <Servo.h>
#include "r4_exbus_uart.hpp"
#include "r4_esc_pwm.hpp"
#include "r4_flash_storage.hpp"
R4ExBusUart exbus;
R4EscPwm esc;
R4FlashStorage storage;
volatile uint32_t hall=0;
void onHall() { ++hall; }
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(2,INPUT_PULLUP);
  pinMode(7,INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2),onHall,FALLING);
  analogReadResolution(12);
  (void)EEPROM.length(); // Capacity API only, never EEPROM write/open.
  (void)storage.begin();
  (void)exbus.begin(125000);
  (void)esc.begin(4);
  esc.stopNow();
}
void loop() {
  storage.service(millis());
  exbus.service(micros());
  R4ExBusUart::Byte byte;
  for (unsigned n=0;n<64 && exbus.pop(byte);++n) {}
  if (digitalRead(7)==LOW) esc.stopNow();
}
'''

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arduino-cli', default='arduino-cli')
    parser.add_argument('--config-file')
    args = parser.parse_args()
    stage = ROOT / '.build' / 'qualification' / 'NanoR4_Qualification'
    headers = stage / 'src' / 'winch'
    headers.mkdir(parents=True, exist_ok=True)
    for name in ('persistence.hpp', 'settings.hpp'):
        shutil.copyfile(ROOT / 'core' / name, headers / name)
    for stem in ('r4_exbus_uart', 'r4_esc_pwm', 'r4_flash_storage'):
        for suffix in ('.hpp', '.cpp'):
            shutil.copyfile(ROOT / 'firmware' / 'Schleppseilwinde_V3_0' / (stem + suffix), stage / (stem + suffix))
    (stage / 'NanoR4_Qualification.ino').write_text(SKETCH, encoding='utf-8')
    command = [args.arduino_cli]
    if args.config_file:
        command += ['--config-file', args.config_file]
    command += ['compile', '--fqbn', 'arduino:renesas_uno:nanor4', '--warnings', 'all',
                '--build-path', str(ROOT / '.build' / 'qualification' / 'build'), str(stage)]
    print(subprocess.list2cmdline(command), flush=True)
    subprocess.run(command, check=True)

if __name__ == '__main__':
    main()
