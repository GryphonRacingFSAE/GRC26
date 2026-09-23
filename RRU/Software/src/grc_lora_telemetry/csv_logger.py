"""Read the RRU serial stream, validate telemetry, and write CSV recordings."""

from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path
from typing import TextIO

import serial

from .parser import TelemetryCsvParser


class TelemetryCsvWriter:
    """Record complete rows together with the header that actually describes them."""

    def __init__(self, output: TextIO) -> None:
        self.output = output
        self.writer = csv.writer(output)
        self.header: list[str] | None = None
        self.parser = TelemetryCsvParser()

    def write_line(self, line: str) -> list[str] | None:
        line = line.strip()
        parsed = self.parser.parse_line(line)
        if line.startswith("event,") and self.parser.header_valid:
            self._write_header(self.parser.header)
            return None
        if parsed is None:
            return None
        self._write_header(self.parser.header)
        row = next(csv.reader([line], strict=True))
        self.writer.writerow(row)
        self.output.flush()
        return row

    def _write_header(self, header: list[str]) -> None:
        if self.header != header:
            self.header = header.copy()
            self.writer.writerow(header)
            self.output.flush()


def run(args: argparse.Namespace) -> None:
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with serial.Serial(args.serial_port, args.baud, timeout=1) as ser, output_path.open("w", newline="") as f:
        writer = TelemetryCsvWriter(f)

        print(f"[serial] logging {args.serial_port} at {args.baud} to {output_path}")

        while True:
            raw = ser.readline()
            if not raw:
                continue

            line = raw.decode("utf-8", errors="replace").strip()
            if not line:
                continue

            row = writer.write_line(line)
            if row is None:
                if args.verbose and not line.startswith("event,"):
                    print(f"[ignored] {line}", file=sys.stderr)
            elif args.print_rows:
                print(",".join(row))


def build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Record GRC LoRa RX serial CSV to a file.")
    p.add_argument("serial_port", help="Serial port for RX board")
    p.add_argument("output", help="Output CSV path")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--print-rows", action="store_true")
    p.add_argument("--verbose", action="store_true")
    return p


def cli() -> None:
    try:
        run(build_arg_parser().parse_args())
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    cli()
