#!/usr/bin/env python3
"""Deterministic EX Bus testmaster; independent implementation, no DUT imports.

Official vector bytes are transcribed from JETI EX Bus 1.21 pages 6/7 and
EX telemetry 1.07 page 8. CRC16 uses the official algebraic update instead of
the DUT's reflected bit loop. The checked-in fixtures are the test authority.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import random

ROOT = Path(__file__).resolve().parent


def crc16_documented(data: bytes) -> int:
    crc = 0
    for byte in data:
        byte ^= crc & 255
        byte ^= (byte << 4) & 255
        crc = ((byte << 8) | (crc >> 8)) ^ (byte >> 4) ^ (byte << 3)
        crc &= 65535
    return crc


def crc8_polynomial(data: bytes) -> int:
    # Polynomial division of the full bit string, independent of DUT byte loop.
    # Appending eight zero bits gives the unreflected CRC with init=0.
    dividend = int.from_bytes(data, 'big') << 8
    for bit in range(dividend.bit_length() - 1, 7, -1):
        if dividend & (1 << bit):
            dividend ^= 0x107 << (bit - 8)
    return dividend


def finish_bus(body: bytes) -> bytes:
    return body + crc16_documented(body).to_bytes(2, 'little')


def packet(header: int, packet_id: int, block_id: int, data: bytes, release=True) -> bytes:
    assert len(data) <= 56
    return finish_bus(bytes([header, 1 if release else 3, len(data) + 8,
                             packet_id & 255, block_id, len(data)]) + data)


def channels(packet_id: int, values_us: list[int], release=False) -> bytes:
    assert 1 <= len(values_us) <= 24 and all(0 <= v <= 8191 for v in values_us)
    return packet(0x3e, packet_id, 0x31,
                  b''.join((value * 8).to_bytes(2, 'little') for value in values_us), release)


def telemetry_request(packet_id: int) -> bytes:
    return packet(0x3d, packet_id, 0x3a, b'')


def jetibox_request(packet_id: int, buttons=0xf0) -> bytes:
    assert buttons & 15 == 0
    return packet(0x3d, packet_id, 0x3b, bytes([buttons]))


def ex_header(manufacturer: int, device: int) -> bytes:
    return manufacturer.to_bytes(2, 'little') + device.to_bytes(2, 'little') + b'\x00'


def ex_data(manufacturer: int, device: int, values: list[tuple[int, int, int]]) -> bytes:
    body = bytearray(ex_header(manufacturer, device))
    for identifier, value, decimal in values:
        magnitude = abs(value)
        assert 1 <= identifier <= 15 and 0 <= decimal <= 3 and magnitude <= 536870911
        width, kind = next((width, kind) for width, kind in [(1, 0), (2, 1), (3, 4), (4, 8)]
                           if magnitude < 1 << (8 * width - 3))
        encoded = bytearray(magnitude.to_bytes(width, 'little'))
        encoded[-1] |= (0x80 if value < 0 else 0) | decimal * 32
        body.append(identifier * 16 + kind)
        body.extend(encoded)
    assert len(body) + 3 <= 28
    content = bytes([0x40 + len(body) + 1]) + body
    return b'\x9f' + content + bytes([crc8_polynomial(content)])


def ex_text(manufacturer: int, device: int, identifier: int, label: bytes, unit: bytes) -> bytes:
    body = ex_header(manufacturer, device) + bytes([identifier, len(label) * 8 + len(unit)]) + label + unit
    assert len(label) and len(unit) <= 7 and len(body) + 3 <= 28
    content = bytes([len(body) + 1]) + body
    return b'\x9f' + content + bytes([crc8_polynomial(content)])


def vectors() -> dict[str, bytes]:
    official_channel = bytes.fromhex('3e 03 28 06 31 20' + ' 82 1f' * 16 + ' 4f e2')
    official_telemetry = bytes.fromhex('3d 01 08 06 3a 00 98 81')
    official_jetibox = bytes.fromhex('3d 01 09 88 3b 01 f0 a3 24')
    official_response = bytes.fromhex('3b 01 20 08 3a 18 9f 56 00 a4 51 55 ee 11 30 20 21 00 40 34 a3 28 00 41 00 00 51 18 00 09 91 d6')
    official_box_response = bytes.fromhex('3b 01 28 88 3b 20 43 65 6e 74 72 61 6c 20 42 6f 78 20 31 30 30 3e 20 20 20 34 2e 38 56 20 20 31 30 34 30 6d 41 68 eb de')
    official_ex_data = bytes.fromhex('9f 4c a1 a8 5d 55 00 11 e8 23 21 1b 00 f4')
    official_ex_text = bytes.fromhex('9f 0f a1 a8 5d 55 00 02 2a 54 65 6d 70 2e b0 43 28')
    own_ex = ex_data(0xa400, 0x0300, [(1, 12345, 3), (2, -1234, 2), (3, 1000, 0),
                                   (4, 31, 3), (5, -536870911, 0)])
    own_text = ex_text(0xa400, 0x0300, 1, b'Remain est', b'm')
    return {
        'official_channels': official_channel,
        'official_telemetry_request': official_telemetry,
        'official_jetibox_request': official_jetibox,
        'official_telemetry_response': official_response,
        'official_jetibox_response': official_box_response,
        'official_ex_data': official_ex_data,
        'official_ex_text': official_ex_text,
        'independent_channels': channels(0x42, [1000, 1500, 2000, 1250]),
        'independent_ex_data': own_ex,
        'independent_ex_text': own_text,
        'independent_telemetry_response': packet(0x3b, 0x42, 0x3a, own_ex),
    }


def rendered_header(references: dict[str, bytes]) -> str:
    lines = ['#pragma once', '#include <cstddef>', '#include <cstdint>', '',
             '// Generated by independent exbus_master.py, official vectors remain literal.',
             'namespace reference {']
    for name, data in references.items():
        lines.append('constexpr uint8_t ' + name + '[] = {' + ', '.join(f'0x{b:02x}' for b in data) + '};')
    lines.append('} // namespace reference\n')
    return '\n'.join(lines)


def self_test() -> None:
    refs = vectors()
    for name, data in refs.items():
        if 'ex_data' in name or 'ex_text' in name:
            assert (data[1] & 63) == len(data) - 2, name
            assert crc8_polynomial(data[1:-1]) == data[-1], name
        else:
            assert data[2] == len(data), name
            assert crc16_documented(data[:-2]) == int.from_bytes(data[-2:], 'little'), name
    # Official data example deliberately uses int14 for both values. Construct
    # those fields independently; our canonical encoder may use int6 for 27.
    content = b'\x4c' + ex_header(0xa8a1, 0x555d) + b'\x11' + (1000 | 0x2000).to_bytes(2, 'little') + b'\x21' + (27).to_bytes(2, 'little')
    assert b'\x9f' + content + bytes([crc8_polynomial(content)]) == refs['official_ex_data']
    assert ex_text(0xa8a1, 0x555d, 2, b'Temp.', b'\xb0C') == refs['official_ex_text']
    json_refs = json.loads((ROOT / 'reference_telegrams.json').read_text(encoding='utf-8'))
    assert json_refs == {name: data.hex(' ') for name, data in refs.items()}
    assert (ROOT / 'reference_telegrams.h').read_text(encoding='utf-8') == rendered_header(refs)
    print(f'PASS: {len(refs)} independent/reference telegrams, CRC16/CRC8 and checked-in fixture parity')


def scenario(name: str, baud: int, seed: int) -> list[dict]:
    """Output a deterministic byte timeline, not a UART/hardware emulation."""
    rng = random.Random(seed)
    events = []
    byte_us = 10_000_000 // baud
    time_us = 0
    def emit(data: bytes, description: str, gap_us=0):
        nonlocal time_us
        time_us += gap_us
        # Deliberate chunking tests receiving partial frames. Byte times stay exact.
        offset = 0
        while offset < len(data):
            size = min(rng.randint(1, 7), len(data) - offset)
            events.append({'at_us': time_us, 'byte_interval_us': byte_us,
                           'hex': data[offset:offset + size].hex(' '), 'description': description})
            offset += size
            time_us += byte_us * size
    index = 0
    while time_us < 8_000_000:
        # Default real core needs 3 s arming + stable fresh neutral. Four seconds
        # of neutral make the scenario reach RUN even at high/overload byte rates.
        trigger = 1500 if time_us < 4_000_000 or time_us >= 6_000_000 else 2000
        if name == 'loss' and 4_600_000 <= time_us < 5_500_000:
            emit(telemetry_request(index), 'telemetry only: channel age must expire', 10_000)
            index += 1
            continue
        if name == 'noise' and index % 5 == 0:
            emit(bytes([0, 255, 0x3e, 2, 0x3d, 1, 255]), 'noise / invalid header-length')
            broken = bytearray(channels(index, [1500, trigger]))
            broken[-1] ^= 0x80
            emit(bytes(broken), 'CRC corruption', 7000)
        emit(channels(index, [1500, trigger]), 'fresh channels', 7000 if name != 'overload' else 0)
        emit(telemetry_request(index), 'telemetry request', 200)
        time_us += 4000 # Master releases bus for the response slot.
        emit(jetibox_request(index, 0xf0 if index % 2 == 0 else 0xe0), 'JETIBOX request')
        time_us += 4000
        index += 1
    return events


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--write-fixtures', action='store_true')
    parser.add_argument('--scenario', choices=['normal', 'loss', 'noise', 'overload'], default='normal')
    parser.add_argument('--baud', type=int, choices=[125000, 250000], default=125000)
    parser.add_argument('--seed', type=int, default=0x5345494c)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.write_fixtures:
        refs = vectors()
        (ROOT / 'reference_telegrams.json').write_text(json.dumps({n: b.hex(' ') for n, b in refs.items()}, indent=2) + '\n', encoding='utf-8')
        (ROOT / 'reference_telegrams.h').write_text(rendered_header(refs), encoding='utf-8')
    if args.self_test:
        self_test()
        return
    result = {'schema': 'winch-exbus-scenario-1', 'baud': args.baud, 'seed': args.seed,
              'scenario': args.scenario, 'events': scenario(args.scenario, args.baud, args.seed)}
    output = json.dumps(result, indent=2) + '\n'
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding='utf-8')
    elif not args.write_fixtures:
        print(output, end='')


if __name__ == '__main__':
    main()
