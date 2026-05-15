"""Tiny serial CSV logger for the RX board.

Use this when you only want a CSV file and do not need Foxglove/MCAP.
"""

from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path

import serial

from .schema import CSV_HEADER


def run(args: argparse.Namespace) -> None:
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    active_header = CSV_HEADER.copy()

    with serial.Serial(args.serial_port, args.baud, timeout=1) as ser, output_path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(active_header)
        f.flush()

        print(f"[serial] logging {args.serial_port} at {args.baud} to {output_path}")

        while True:
            raw = ser.readline()
            if not raw:
                continue

            line = raw.decode("utf-8", errors="replace").strip()
            if not line:
                continue

            if line.startswith("event,"):
                # The firmware's header is the source of truth once it appears.
                active_header = next(csv.reader([line]))
                continue

            if not line.startswith("rx_packet,"):
                if args.verbose:
                    print(f"[ignored] {line}", file=sys.stderr)
                continue

            try:
                row = next(csv.reader([line]))
            except csv.Error:
                if args.verbose:
                    print(f"[bad csv] {line}", file=sys.stderr)
                continue

            if len(row) < len(active_header):
                row = row + [""] * (len(active_header) - len(row))
            elif len(row) > len(active_header):
                row = row[: len(active_header)]

            writer.writerow(row)
            f.flush()
            if args.print_rows:
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
