"""Host protocol regressions. Run with python -m unittest discover -s tests -v."""

from contextlib import redirect_stdout
import csv
import io
from pathlib import Path
import re
import tempfile
import unittest
from unittest.mock import patch

from grc_lora_telemetry.csv_logger import TelemetryCsvWriter, cli
from grc_lora_telemetry.parser import TelemetryCsvParser, parse_lines
from grc_lora_telemetry.schema import (
    CSV_HEADER,
    PACKET_FIELDS,
)


def csv_line(values):
    stream = io.StringIO()
    csv.writer(stream).writerow(values)
    return stream.getvalue().strip()


def packet_line(packet_type, values=None, header=CSV_HEADER):
    data = {
        "event": "rx_packet", "packet_type": packet_type, "rx_count": "12",
        "rx_ms": "4000", "rssi_dbm": "-42.5", "snr_db": "9.25",
        "radio_len": {"FAST": 35, "SLOW": 31, "EVENT": 33, "SENSORS": 59}.get(packet_type, 0),
        "seq": "9", "tx_ms": "3980", "schema_version": "3",
        "received_mask_hex": "0xFFFF", "fresh_mask_hex": "0xFFFF",
    }
    data.update(values or {})
    return csv_line([data.get(field, "") for field in header])


# Engineering values chosen independently from the wire packers, including
# signed extremes, seven-place GPS precision and unsigned 16-bit gear/channel.
DBC_MEASUREMENTS = {
    "FAST": {
        "rpm": 65535, "tps_pct": 100.0, "lambda_avg": 65.535,
        "vehicle_speed_kph": 6553.5, "status_bits_hex": "0x0380",
        "knock_detected": 1, "brake_pedal_active": 1, "clutch_pedal_active": 1,
        "rev_limit_rpm": 65535, "gear": 65535, "user_channel_1": 6553.5,
        "battery_v": 655.35,
    },
    "SLOW": {
        "lambda_a": 1.002, "lambda_b": 0.998, "lambda_target": -32.768,
        "fuel_inj_pulse_width_ms": 655.35,
        "fuel_inj_duty_pct": 6553.5, "intake_air_temp_c": 6553.5,
        "coolant_temp_c": 99.9,
    },
    "SENSORS": {
        "aero_pressure_1_pa": -32768, "aero_pressure_2_pa": 32767,
        "aero_ambient_temp_c": -327.68, "aero_ambient_pressure_hpa": 6553.5,
        "aero_node_state": 2, "aero_sensor_flags": 255,
        "aero_fault_flags": 65535, "aero_sequence": 255,
        "acceleration_x_g": -32.768, "acceleration_y_g": 32.767,
        "acceleration_z_g": 1.001, "yaw_rate_dps": -327.68,
        "pitch_rate_dps": 327.67, "roll_rate_dps": -0.01,
        "imu_node_state": 3, "imu_sensor_flags": 255,
        "imu_fault_flags": 65535, "imu_sequence": 255,
        "gps_latitude_deg": -214.7483648, "gps_longitude_deg": 214.7483647,
        "gps_ground_speed_kph": 655.35, "gps_course_deg": 359.99,
    },
    "EVENT": {
        "alert_flags_hex": "0x0007", "status_bits_hex": "0x0380",
        "knock_detected": 1, "brake_pedal_active": 1, "clutch_pedal_active": 1,
        "rpm": 4000, "aero_node_state": 2, "aero_sensor_flags": 3,
        "aero_fault_flags": 4, "aero_sequence": 5, "imu_node_state": 3,
        "imu_sensor_flags": 6, "imu_fault_flags": 7, "imu_sequence": 8,
    },
}


class ParserTests(unittest.TestCase):
    def test_all_40_dbc_measurements_are_parsed(self):
        parser = TelemetryCsvParser()
        self.assertIsNone(parser.parse_line(csv_line(CSV_HEADER)))
        covered = set()
        for packet_type, expected in DBC_MEASUREMENTS.items():
            with self.subTest(packet_type=packet_type):
                parsed = parser.parse_line(packet_line(packet_type, expected))
                self.assertIsNotNone(parsed)
                self.assertEqual(parsed.packet_type, packet_type)
                for field, value in expected.items():
                    self.assertEqual(parsed.payload[field], value)
                    covered.add(field)
                self.assertEqual(parsed.payload["schema_version"], 3)
                self.assertEqual(parsed.payload["received_mask"], 65535)
                self.assertEqual(parsed.payload["fresh_mask"], 65535)
                self.assertLessEqual(parsed.payload.keys(), set(PACKET_FIELDS[packet_type]))
                self.assertTrue(all(value is not None for value in parsed.payload.values()))
        self.assertEqual(set(CSV_HEADER[16:]), covered - {"status_bits_hex", "alert_flags_hex"})
        self.assertEqual(len(CSV_HEADER[16:]), 40)

    def test_missing_sources_need_no_other_packet_to_parse(self):
        parser = TelemetryCsvParser()
        # Only 0x520 has ever arrived; no sensor/status/GPS packet is necessary.
        for sequence in range(100):
            parsed = parser.parse_line(packet_line("FAST", {
                "seq": sequence, "rpm": 5200, "tps_pct": 34.2, "lambda_avg": 1.002,
                "received_mask_hex": "0x0001", "fresh_mask_hex": "0x0000",
            }))
            self.assertEqual(parsed.payload["seq"], sequence)
            self.assertEqual(parsed.payload["rpm"], 5200)  # Retain stale value.
            self.assertEqual(parsed.payload["received_mask"], 1)
            self.assertEqual(parsed.payload["fresh_mask"], 0)
            self.assertNotIn("vehicle_speed_kph", parsed.payload)
            self.assertNotIn("status_bits", parsed.payload)
            self.assertNotIn("gear", parsed.payload)

        empty = parser.parse_line(packet_line("SENSORS", {
            "received_mask_hex": "0x0000", "fresh_mask_hex": "0x0000",
        }))
        self.assertNotIn("gps_latitude_deg", empty.payload)
        self.assertNotIn("aero_pressure_1_pa", empty.payload)

    def test_wrong_versions_and_removed_packet_types_are_rejected(self):
        parser = TelemetryCsvParser()
        for version in (1, 2, 4, "", "invalid"):
            self.assertIsNone(parser.parse_line(packet_line("FAST", {"schema_version": version})))
        self.assertIsNone(parser.parse_line(packet_line("POWERTRAIN")))
        for width in (26, 42, 44, 75, 96):
            self.assertIsNone(parser.parse_line(csv_line(["rx_packet"] + [""] * (width - 1))))
        self.assertEqual(parser.parse_line(packet_line("FAST", {"rpm": "0009"})).payload["rpm"], 9)

    def test_wrong_header_requires_a_current_header_before_more_rows(self):
        invalid_headers = [
            CSV_HEADER[:-1], CSV_HEADER + ["unknown_field"],
            CSV_HEADER[:-1] + ["unknown_field"], CSV_HEADER[:-1] + ["rpm"],
        ]
        for header in invalid_headers:
            with self.subTest(header=header):
                parser = TelemetryCsvParser()
                parser.parse_line(csv_line(header))
                self.assertIsNone(parser.parse_line(packet_line("FAST", {"rpm": 1234})))
                parser.parse_line(csv_line(CSV_HEADER))
                self.assertEqual(parser.parse_line(packet_line("FAST", {"rpm": 1234})).payload["rpm"], 1234)

    def test_explicit_header_order_and_changes_are_authoritative(self):
        parser = TelemetryCsvParser()
        header = [CSV_HEADER[0]] + list(reversed(CSV_HEADER[1:]))
        parser.parse_line(csv_line(header))
        parsed = parser.parse_line(packet_line("SENSORS", {
            "gps_longitude_deg": -80.1234567, "gps_latitude_deg": 43.7654321,
        }, header))
        self.assertEqual(parsed.payload["gps_longitude_deg"], -80.1234567)
        parser.parse_line(csv_line(CSV_HEADER))
        parsed = parser.parse_line(packet_line("FAST", {"gear": 6}))
        self.assertEqual(parsed.payload["gear"], 6)

    def test_corrupt_rows_are_ignored_without_losing_next_packet(self):
        parser = TelemetryCsvParser()
        parser.parse_line(csv_line(CSV_HEADER))
        valid = packet_line("FAST", {"rpm": 3000})
        for invalid in ("", "[LoRaTask] Started", "rx_packet,broken", valid + ",extra",
                        valid.rsplit(",", 1)[0], 'rx_packet,"unterminated',
                        packet_line("UNKNOWN")):
            self.assertIsNone(parser.parse_line(invalid))
        parsed = parser.parse_line(valid.encode())
        self.assertEqual(parsed.payload["rpm"], 3000)

    def test_nonfinite_and_invalid_numeric_values_are_omitted(self):
        parsed = TelemetryCsvParser().parse_line(packet_line("FAST", {
            "rpm": "invalid", "lambda_avg": "nan", "tps_pct": "Infinity",
            "battery_v": "-inf", "status_bits_hex": "invalid",
        }))
        for field in ("rpm", "lambda_avg", "tps_pct", "battery_v", "status_bits"):
            self.assertNotIn(field, parsed.payload)
        self.assertTrue(all(value is not None for value in parsed.payload.values()))


class SchemaAndLoggingTests(unittest.TestCase):
    def test_sample_has_every_packet_type(self):
        software = Path(__file__).resolve().parents[1]
        packets = parse_lines((software / "samples/sample_rx.csv").read_text().splitlines())
        self.assertEqual([p.packet_type for p in packets], ["FAST", "SLOW", "SENSORS", "EVENT"])
        self.assertEqual(packets[2].payload["gps_longitude_deg"], -80.2234567)

    def test_host_column_order_matches_embedded_header(self):
        firmware = Path(__file__).resolve().parents[2] / "Embedded/src/Utils/TelemetryCsv.cpp"
        text = firmware.read_text(encoding="utf-8")
        columns = text.split("#define CSV_COLUMNS(X)", 1)[1].split("enum Column", 1)[0]
        self.assertEqual(CSV_HEADER, re.findall(r"X\((\w+)\)", columns))
        self.assertEqual(len(CSV_HEADER), 56)
        self.assertEqual(set(PACKET_FIELDS), {"FAST", "SLOW", "EVENT", "SENSORS"})
        exported = {field for fields in PACKET_FIELDS.values() for field in fields}
        self.assertLessEqual(exported, set(CSV_HEADER) | {"alert_flags", "status_bits", "received_mask", "fresh_mask"})

    def test_logger_records_valid_header_order_changes(self):
        output = io.StringIO()
        writer = TelemetryCsvWriter(output)
        self.assertEqual(output.getvalue(), "")
        writer.write_line("[LoRaTask] Started")
        reordered_header = [CSV_HEADER[0]] + list(reversed(CSV_HEADER[1:]))
        writer.write_line(csv_line(reordered_header))
        writer.write_line(packet_line("FAST", {"gear": 3}, reordered_header))
        writer.write_line(csv_line(CSV_HEADER))
        writer.write_line(packet_line("SENSORS", {"gps_latitude_deg": -12.1234567}))
        rows = list(csv.reader(io.StringIO(output.getvalue())))
        self.assertEqual(rows[0], reordered_header)
        self.assertEqual(rows[2], CSV_HEADER)
        parsed = parse_lines(output.getvalue().splitlines())
        self.assertEqual([p.packet_type for p in parsed], ["FAST", "SENSORS"])
        self.assertEqual(parsed[1].payload["gps_latitude_deg"], -12.1234567)

    def test_logger_late_attach_and_partial_rows(self):
        class FlushCounter(io.StringIO):
            flush_count = 0

            def flush(self):
                self.flush_count += 1
                super().flush()

        output = FlushCounter()
        writer = TelemetryCsvWriter(output)
        writer.write_line("rx_packet,partial")
        self.assertEqual(output.getvalue(), "")
        writer.write_line(packet_line("FAST", {"rpm": 1234}))
        self.assertEqual(output.flush_count, 2)  # Header and accepted packet are flushed.
        writer.write_line(csv_line(CSV_HEADER))  # Do not duplicate same boot header.
        writer.write_line("rx_packet,partial")
        self.assertEqual(output.flush_count, 2)
        rows = list(csv.reader(io.StringIO(output.getvalue())))
        self.assertEqual(rows[0], CSV_HEADER)
        self.assertEqual(len(rows), 2)
        self.assertEqual(len(rows[1]), len(CSV_HEADER))

    def test_logger_rejects_old_versions_and_wrong_headers(self):
        output = io.StringIO()
        writer = TelemetryCsvWriter(output)
        writer.write_line(packet_line("FAST", {"schema_version": 2}))
        writer.write_line(packet_line("POWERTRAIN"))
        writer.write_line(csv_line(CSV_HEADER[:-1] + ["unknown_field"]))
        writer.write_line(packet_line("FAST"))
        self.assertEqual(output.getvalue(), "")
        writer.write_line(csv_line(CSV_HEADER))
        writer.write_line(packet_line("FAST", {"rpm": 1234}))
        self.assertEqual(len(output.getvalue().splitlines()), 2)

    def test_csv_cli_logs_sample_from_serial_and_handles_idle_reads(self):
        sample = Path(__file__).resolve().parents[1] / "samples/sample_rx.csv"
        sample_lines = sample.read_bytes().splitlines(keepends=True)
        incoming = [b"", b"[LoRaTask] Started\n", sample_lines[0], b"rx_packet,partial\n"]
        for line in sample_lines[1:]:
            incoming.extend([b"", line])
        incoming.append(packet_line("FAST", {"schema_version": 2}).encode())

        class FakeSerial:
            def __init__(self):
                self.lines = iter(incoming)
                self.closed = False

            def __enter__(self):
                return self

            def __exit__(self, *args):
                self.closed = True

            def readline(self):
                try:
                    return next(self.lines)
                except StopIteration:
                    raise KeyboardInterrupt

        serial = FakeSerial()
        output = io.StringIO()
        with tempfile.TemporaryDirectory() as folder:
            destination = Path(folder) / "session.csv"
            with patch("grc_lora_telemetry.csv_logger.serial.Serial", return_value=serial) as factory, \
                 patch("sys.argv", ["grc-lora-csv", "COM_TEST", str(destination), "--print-rows"]), \
                 redirect_stdout(output):
                cli()
            factory.assert_called_once_with("COM_TEST", 115200, timeout=1)
            self.assertTrue(serial.closed)
            with destination.open(newline="") as file, sample.open(newline="") as expected:
                self.assertEqual(list(csv.reader(file)), list(csv.reader(expected)))
            self.assertEqual(output.getvalue().count("rx_packet,"), 4)


if __name__ == "__main__":
    unittest.main()
