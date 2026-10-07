"""Read-only logger parsing compatibility; no serial port or Pi required."""
import importlib.util
from pathlib import Path
import sys
import types
import unittest

# Avoid requiring pyserial or opening devices just to exercise the pure parser.
serial = types.ModuleType('serial')
serial.tools = types.ModuleType('serial.tools')
serial.tools.list_ports = types.ModuleType('serial.tools.list_ports')
sys.modules['serial'] = serial
sys.modules['serial.tools'] = serial.tools
sys.modules['serial.tools.list_ports'] = serial.tools.list_ports
root = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('passive_logger', root / 'pi/seilwinde-logger/logger.py')
logger = importlib.util.module_from_spec(spec)
spec.loader.exec_module(logger)

class LoggerTest(unittest.TestCase):
    def test_v2_normalizes_without_guessing(self):
        row = ['telemetry'] + ['0'] * 22
        self.assertEqual(logger.parse_arduino_line(','.join(row)), row + [''] * 11)

    def test_v3_preserves_fields(self):
        row = ['event'] + ['0'] * 22 + ['3.0'] + ['0'] * 10
        self.assertEqual(logger.parse_arduino_line(','.join(row)), row)

    def test_header_debug_and_partial(self):
        for line in [','.join(logger.ARDUINO_HEADER), ','.join(logger.V3_HEADER), '#diagnostic', '', 'telemetry,1,partial']:
            self.assertIsNone(logger.parse_arduino_line(line))

    def test_version_and_record_type(self):
        row = ['event'] + ['0'] * 22 + ['99'] + ['0'] * 10
        self.assertIsNone(logger.parse_arduino_line(','.join(row)))
        row[0] = 'command'; row[23] = '3.0'
        self.assertIsNone(logger.parse_arduino_line(','.join(row)))

if __name__ == '__main__': unittest.main()
