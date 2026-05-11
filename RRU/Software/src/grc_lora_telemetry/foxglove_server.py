"""Foxglove websocket + MCAP bridge for GRC LoRa telemetry CSV serial output."""

from __future__ import annotations

import argparse
import asyncio
import datetime as dt
import json
from pathlib import Path
from typing import BinaryIO

from foxglove_websocket.server import FoxgloveServer, FoxgloveServerListener
from foxglove_websocket.types import ChannelId
from mcap.writer import Writer
from serial_asyncio import open_serial_connection

from .parser import ParsedTelemetry, TelemetryCsvParser
from .schema import TOPICS, json_schema_for_fields

DEFAULT_HOST = "0.0.0.0"
DEFAULT_PORT = 8765
DEFAULT_BAUD = 115200
DEFAULT_LOG_DIR = "logs"


class Listener(FoxgloveServerListener):
    async def on_subscribe(self, server: FoxgloveServer, channel_id: ChannelId):
        print(f"[foxglove] first client subscribed to channel {channel_id}")

    async def on_unsubscribe(self, server: FoxgloveServer, channel_id: ChannelId):
        print(f"[foxglove] last client unsubscribed from channel {channel_id}")


class TelemetryMcapWriter:
    def __init__(self, log_dir: Path, enabled: bool = True) -> None:
        self.enabled = enabled
        self.log_dir = log_dir
        self.file: BinaryIO | None = None
        self.writer: Writer | None = None
        self.channel_ids: dict[str, int] = {}

    def start(self) -> None:
        if not self.enabled:
            return

        self.log_dir.mkdir(parents=True, exist_ok=True)
        timestamp = dt.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        path = self.log_dir / f"grc_lora_telemetry_{timestamp}.mcap"
        self.file = path.open("wb")
        self.writer = Writer(self.file)
        self.writer.start()

        for packet_type, (topic, fields) in TOPICS.items():
            schema = json_schema_for_fields(packet_type, fields)
            schema_id = self.writer.register_schema(
                name=packet_type,
                encoding="jsonschema",
                data=json.dumps(schema).encode("utf-8"),
            )
            channel_id = self.writer.register_channel(
                schema_id=schema_id,
                topic=topic,
                message_encoding="json",
            )
            self.channel_ids[topic] = channel_id

        print(f"[mcap] logging to {path}")

    def write(self, parsed: ParsedTelemetry) -> None:
        if not self.enabled or self.writer is None:
            return

        channel_id = self.channel_ids.get(parsed.topic)
        if channel_id is None:
            return

        data = json.dumps(parsed.payload, separators=(",", ":")).encode("utf-8")
        timestamp_ns = parsed.timestamp_ns or _now_ns()
        self.writer.add_message(
            channel_id=channel_id,
            log_time=timestamp_ns,
            publish_time=timestamp_ns,
            data=data,
        )

    def close(self) -> None:
        if self.writer is not None:
            self.writer.finish()
            self.writer = None
        if self.file is not None:
            self.file.close()
            self.file = None


async def add_channels(server: FoxgloveServer) -> dict[str, ChannelId]:
    channel_ids: dict[str, ChannelId] = {}

    for packet_type, (topic, fields) in TOPICS.items():
        schema = json_schema_for_fields(packet_type, fields)
        channel_id = await server.add_channel(
            {
                "topic": topic,
                "encoding": "json",
                "schemaName": packet_type,
                "schema": json.dumps(schema),
                "schemaEncoding": "jsonschema",
            }
        )
        channel_ids[topic] = channel_id
        print(f"[foxglove] added {topic}")

    return channel_ids


async def run_server(args: argparse.Namespace) -> None:
    parser = TelemetryCsvParser()
    mcap = TelemetryMcapWriter(Path(args.log_dir), enabled=not args.no_mcap)
    mcap.start()

    try:
        async with FoxgloveServer(
            args.host,
            args.websocket_port,
            "GRC-LoRa-Telemetry",
            capabilities=[],
            supported_encodings=["json"],
        ) as server:
            server.set_listener(Listener())
            channel_ids = await add_channels(server)

            print(f"[serial] opening {args.serial_port} at {args.baud}")
            reader, _writer = await open_serial_connection(
                url=args.serial_port,
                baudrate=args.baud,
            )

            print(f"[foxglove] websocket ws://{args.host}:{args.websocket_port}")
            print("[serial] waiting for telemetry CSV rows...")

            while True:
                raw_line = await reader.readline()
                parsed = parser.parse_line(raw_line)
                if parsed is None:
                    if args.verbose:
                        text = raw_line.decode("utf-8", errors="replace").strip()
                        if text:
                            print(f"[ignored] {text}")
                    continue

                if args.print_rows:
                    print(json.dumps({"topic": parsed.topic, **parsed.payload}, separators=(",", ":")))

                payload = json.dumps(parsed.payload, separators=(",", ":")).encode("utf-8")
                timestamp_ns = parsed.timestamp_ns or _now_ns()

                await server.send_message(
                    channel_ids[parsed.topic],
                    timestamp_ns,
                    payload,
                )

                mcap.write(parsed)
    finally:
        mcap.close()


def _now_ns() -> int:
    return int(dt.datetime.now(tz=dt.UTC).timestamp() * 1_000_000_000)


def build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Bridge GRC LoRa RX CSV serial telemetry into Foxglove and MCAP logs."
    )
    p.add_argument("serial_port", help="Serial port for the RX board, for example COM7 or /dev/ttyUSB0")
    p.add_argument("--baud", type=int, default=DEFAULT_BAUD, help=f"Serial baud rate, default {DEFAULT_BAUD}")
    p.add_argument("--host", default=DEFAULT_HOST, help=f"Websocket bind host, default {DEFAULT_HOST}")
    p.add_argument("--websocket-port", type=int, default=DEFAULT_PORT, help=f"Foxglove websocket port, default {DEFAULT_PORT}")
    p.add_argument("--log-dir", default=DEFAULT_LOG_DIR, help=f"MCAP log folder, default {DEFAULT_LOG_DIR}")
    p.add_argument("--no-mcap", action="store_true", help="Disable MCAP logging")
    p.add_argument("--print-rows", action="store_true", help="Print parsed JSON rows to stdout")
    p.add_argument("--verbose", action="store_true", help="Print ignored serial/debug lines")
    return p


def cli() -> None:
    args = build_arg_parser().parse_args()
    try:
        asyncio.run(run_server(args))
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    cli()
