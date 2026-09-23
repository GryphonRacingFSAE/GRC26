"""CSV parser for the GRC26 LoRa receiver serial stream.

The receiver firmware prints a CSV header and one row per received telemetry
packet. This parser validates the current schema and returns typed measurements
for the telemetry software's CSV logging path.
"""

from __future__ import annotations

import csv
import math
from dataclasses import dataclass
from typing import Iterable

from .schema import CSV_HEADER, FLOAT_FIELDS, INT_FIELDS, PACKET_FIELDS, SCHEMA_VERSION, STRING_FIELDS, is_current_header


@dataclass(slots=True)
class ParsedTelemetry:
    packet_type: str
    payload: dict


class TelemetryCsvParser:
    """Stateful parser that learns the CSV header from the serial stream."""

    def __init__(self) -> None:
        self.header: list[str] = CSV_HEADER.copy()
        self.header_valid = True

    def parse_line(self, raw_line: bytes | str) -> ParsedTelemetry | None:
        if isinstance(raw_line, bytes):
            line = raw_line.decode("utf-8", errors="replace").strip()
        else:
            line = raw_line.strip()

        if not line:
            return None

        # Ignore firmware boot/debug logs without interrupting logging.
        if not self._looks_like_csv(line):
            return None

        try:
            row = next(csv.reader([line], strict=True))
        except csv.Error:
            if line.startswith("event,"):
                self.header_valid = False
            return None

        if not row:
            return None

        # Receiver prints a header once. Accept it and continue.
        if row[0] == "event":
            header = [field.strip() for field in row]
            self.header_valid = is_current_header(header)
            if self.header_valid:
                self.header = header
            return None

        # Ignore malformed packet rows.
        if row[0] != "rx_packet":
            return None

        # A late serial connection may use the current fixed header. After an
        # incompatible header, require a valid one before interpreting more rows.
        if not self.header_valid or len(row) != len(CSV_HEADER):
            return None

        raw = dict(zip(self.header, row))
        typed = self._convert_row(raw)
        if typed.get("schema_version") != SCHEMA_VERSION:
            return None

        packet_type = str(typed.get("packet_type") or "").upper()
        if packet_type not in PACKET_FIELDS:
            return None

        fields = PACKET_FIELDS[packet_type]

        # Preserve missing measurements as absent values, never false zeroes.
        payload = {
            field: typed.get(field)
            for field in fields
            if typed.get(field) is not None
        }

        return ParsedTelemetry(
            packet_type=packet_type,
            payload=payload,
        )

    @staticmethod
    def _looks_like_csv(line: str) -> bool:
        # Header or packet rows. This filters startup logs such as "[LoRaTask] Started".
        return line.startswith("event,") or line.startswith("rx_packet,")

    def _convert_row(self, row: dict[str, str]) -> dict:
        out: dict = {}

        for key, value in row.items():
            value = value.strip()

            if value == "":
                out[key] = None
                continue

            if key in STRING_FIELDS:
                out[key] = value
                continue

            if key in INT_FIELDS:
                out[key] = _parse_int(value)
                continue

            if key in FLOAT_FIELDS:
                out[key] = _parse_float(value)
                continue

        # Numeric aliases let consumers inspect the packet validity bitfields.
        for field in ("alert_flags", "status_bits", "received_mask", "fresh_mask"):
            out[field] = _parse_hex_or_none(str(out.get(f"{field}_hex") or ""))

        return out


def _parse_int(value: str) -> int | None:
    try:
        return int(value, 16 if value.lower().startswith(("0x", "+0x", "-0x")) else 10)
    except ValueError:
        return None


def _parse_float(value: str) -> float | None:
    try:
        parsed = float(value)
        return parsed if math.isfinite(parsed) else None
    except ValueError:
        return None


def _parse_hex_or_none(value: str) -> int | None:
    if not value:
        return None

    try:
        return int(value, 16)
    except ValueError:
        return None


def parse_lines(lines: Iterable[str]) -> list[ParsedTelemetry]:
    parser = TelemetryCsvParser()
    packets: list[ParsedTelemetry] = []

    for line in lines:
        parsed = parser.parse_line(line)
        if parsed is not None:
            packets.append(parsed)

    return packets
