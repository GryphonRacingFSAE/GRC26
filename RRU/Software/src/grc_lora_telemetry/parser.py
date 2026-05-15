"""CSV parser for the new LoRa RX serial dump.

The receiver firmware prints a CSV header and one row per received telemetry
packet. This parser accepts that stream and returns typed dictionaries suitable
for Foxglove and MCAP logging.
"""

from __future__ import annotations

import csv
from dataclasses import dataclass
from typing import Iterable

from .schema import CSV_HEADER, FLOAT_FIELDS, INT_FIELDS, STRING_FIELDS, TOPICS


@dataclass(slots=True)
class ParsedTelemetry:
    packet_type: str
    topic: str
    timestamp_ns: int
    payload: dict


class TelemetryCsvParser:
    """Stateful parser that learns the CSV header from the serial stream."""

    def __init__(self) -> None:
        self.header: list[str] = CSV_HEADER.copy()

    def parse_line(self, raw_line: bytes | str) -> ParsedTelemetry | None:
        if isinstance(raw_line, bytes):
            line = raw_line.decode("utf-8", errors="replace").strip()
        else:
            line = raw_line.strip()

        if not line:
            return None

        # Ignore firmware boot/debug logs without failing the server.
        if not self._looks_like_csv(line):
            return None

        try:
            row = next(csv.reader([line]))
        except csv.Error:
            return None

        if not row:
            return None

        # Receiver prints a header once. Accept it and continue.
        if row[0] == "event":
            self.header = [field.strip() for field in row]
            return None

        # Ignore malformed packet rows.
        if row[0] != "rx_packet":
            return None

        # Pad/truncate to match the header, so old/new firmware revisions do not
        # immediately crash the bridge when fields are added or removed.
        if len(row) < len(self.header):
            row = row + [""] * (len(self.header) - len(row))
        elif len(row) > len(self.header):
            row = row[: len(self.header)]

        raw = dict(zip(self.header, row))
        typed = self._convert_row(raw)

        packet_type = str(typed.get("packet_type") or "").upper()
        if packet_type not in TOPICS:
            return None

        topic, fields = TOPICS[packet_type]

        # Foxglove does not like null values when the schema says "number" or
        # "string". Keep only fields that have real values.
        payload = {
            field: typed.get(field)
            for field in fields
            if typed.get(field) is not None
        }

        # Force foxglove_server.py to use current host wall-clock time.
        # The original rx_ms value is still included in the payload, so you can
        # plot/debug it if needed.
        timestamp_ns = 0

        return ParsedTelemetry(
            packet_type=packet_type,
            topic=topic,
            timestamp_ns=timestamp_ns,
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

            # Unknown fields are preserved as strings so firmware can add fields
            # without breaking the bridge.
            out[key] = value

        # Numeric aliases for bitfields. These are easier to plot/filter in Foxglove.
        out["alert_flags"] = _parse_hex_or_none(str(out.get("alert_flags_hex") or ""))
        out["status_bits"] = _parse_hex_or_none(str(out.get("status_bits_hex") or ""))

        return out


def _parse_int(value: str) -> int | None:
    try:
        return int(value, 0)
    except ValueError:
        return None


def _parse_float(value: str) -> float | None:
    try:
        return float(value)
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