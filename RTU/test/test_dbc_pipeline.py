"""Compile real RTU/RRU code and check every DBC signal through CAN -> LoRa -> CSV.

Uses only Python's standard library and a host C++ compiler. Run from any directory:
python RTU/test/test_dbc_pipeline.py [--compiler g++]
"""

import argparse
import csv
from decimal import Decimal
import io
from pathlib import Path
import random
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
RTU = ROOT / "RTU"
RRU = ROOT / "RRU/Embedded"
sys.path.insert(0, str(ROOT / "RRU/Software/src"))
from grc_lora_telemetry.parser import parse_lines

# Names are the only manual mapping; bits, signedness and scales come from the DBC.
COLUMNS = {
    "EngineSpeed": "rpm", "ThrottlePos": "tps_pct", "Lambda_Average": "lambda_avg",
    "Lambda_A": "lambda_a", "Lambda_B": "lambda_b",
    "FuelInjPulsewidth": "fuel_inj_pulse_width_ms", "FuelInjDuty": "fuel_inj_duty_pct",
    "VehicleSpeed": "vehicle_speed_kph", "Knock_detected": "knock_detected",
    "Brake_pedal_active": "brake_pedal_active", "Clutch_pedal_active": "clutch_pedal_active",
    "RevLimit_RPM": "rev_limit_rpm", "Lambda_Target": "lambda_target",
    "ECU_BatteryVoltage": "battery_v", "IntakeAirTemp": "intake_air_temp_c",
    "CoolantTemp": "coolant_temp_c", "GearPosn": "gear", "User_Channel_1": "user_channel_1",
    "AeroProbe_Pressure_1": "aero_pressure_1_pa", "AeroProbe_Pressure_2": "aero_pressure_2_pa",
    "AeroProbe_AmbientTemp": "aero_ambient_temp_c",
    "AeroProbe_AmbientPressure": "aero_ambient_pressure_hpa",
    "AeroProbe_NodeState": "aero_node_state", "AeroProbe_SensorFlags": "aero_sensor_flags",
    "AeroProbe_FaultFlags": "aero_fault_flags", "AeroProbe_Sequence": "aero_sequence",
    "Accel_Longitudinal": "acceleration_x_g", "Accel_Lateral": "acceleration_y_g",
    "Accel_Vertical": "acceleration_z_g", "YawRate": "yaw_rate_dps",
    "PitchRate": "pitch_rate_dps", "RollRate": "roll_rate_dps",
    "ImuGps_NodeState": "imu_node_state", "ImuGps_SensorFlags": "imu_sensor_flags",
    "ImuGps_FaultFlags": "imu_fault_flags", "ImuGps_Sequence": "imu_sequence",
    "GPS_Latitude": "gps_latitude_deg", "GPS_Longitude": "gps_longitude_deg",
    "GPS_GroundSpeed": "gps_ground_speed_kph", "GPS_Course": "gps_course_deg",
}

METADATA_COLUMNS = {
    "event", "rx_count", "rx_ms", "rssi_dbm", "snr_db", "radio_len", "packet_type",
    "seq", "tx_ms", "schema_version", "received_mask_hex", "fresh_mask_hex",
    "alert_flags_hex", "status_bits_hex", "error_code", "error_text",
}

SENDER = r'''
#include "TelemetrySender.h"
#include <iostream>
#include <iomanip>
int main() {
    uint32_t now, count;
    if (!(std::cin >> now >> count)) return 1;
    EcuTelemetryState state = {};
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t id, timestamp;
        std::cin >> id >> timestamp;
        uint8_t bytes[8];
        for (size_t j = 0; j < 8; ++j) {
            unsigned value;
            std::cin >> value;
            bytes[j] = static_cast<uint8_t>(value);
        }
        if (!decodeEcuCanFrame(state, id, bytes, 8, timestamp)) return 2;
    }
    for (uint8_t type : {1, 2, 3, 5}) {
        TelemetryPacket packet = {};
        packet.type = type;
        switch (type) {
        case 1: populateFastPacket(packet.data.fast, state, now, type); break;
        case 2: populateSlowPacket(packet.data.slow, state, now, type); break;
        case 3: populateEventPacket(packet.data.event, state, now, type, 7); break;
        case 5: populateSensorsPacket(packet.data.sensors, state, now, type); break;
        }
        uint8_t wire[96];
        size_t length = 0;
        if (!buildTelemetryRadioPayload(packet, wire, sizeof(wire), &length)) return 3;
        for (size_t i = 0; i < length; ++i)
            std::cout << std::hex << std::setfill('0') << std::setw(2) << unsigned(wire[i]) << ' ';
        std::cout << '\n';
    }
}
'''

RECEIVER = r'''
#include "TelemetryCsv.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
int main() {
    std::cout << telemetryCsvHeader() << '\n';
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream input(line);
        std::vector<uint8_t> bytes;
        unsigned byte;
        while (input >> std::hex >> byte) bytes.push_back(static_cast<uint8_t>(byte));
        TelemetryReceivedPacket packet = {};
        if (decodeTelemetryRadioPayload(bytes.data(), bytes.size(), packet) != TelemetryDecodeResult::Ok)
            return 1;
        TelemetryRxMetadata metadata = {1, 100, -80, 7, bytes.size()};
        char csv[TELEMETRY_CSV_BUFFER_SIZE];
        if (!formatTelemetryCsv(csv, sizeof(csv), packet, metadata)) return 2;
        std::cout << csv << '\n';
    }
}
'''


def read_signals():
    messages = {}
    for line in (RTU / "GRC26.dbc").read_text().splitlines():
        message = re.match(r"BO_ (\d+) \w+: 8 \w+", line)
        if message:
            current = int(message[1])
            messages[current] = []
        if line.lstrip().startswith("SG_ "):
            match = re.match(r"\s*SG_ (\w+) : (\d+)\|(\d+)@1([+-]) \(([^,]+),([^\)]+)\)", line)
            assert match, f"Unsupported DBC signal: {line}"
            name, start, size, sign, scale, offset = match.groups()
            messages[current].append((name, int(start), int(size), sign == "-", Decimal(scale), Decimal(offset)))
    assert len(messages) == 16
    assert {s[0] for signals in messages.values() for s in signals} == set(COLUMNS)
    return messages


def build(compiler, build_dir, name, source, project, sources):
    cpp = build_dir / (name + ".cpp")
    executable = build_dir / (name + ".exe")
    cpp.write_text(source)
    subprocess.run([compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-O1",
                    "-DTELEMETRY_MOCK_DATA=0", "-I", str(project / "include/Utils"), str(cpp),
                    *[str(project / path) for path in sources], "-o", str(executable)], check=True)
    return executable


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="g++")
    args = parser.parse_args()
    messages = read_signals()
    build_dir = RTU / ".pio/dbc-pipeline-tests"
    build_dir.mkdir(parents=True, exist_ok=True)
    sender = build(args.compiler, build_dir, "sender", SENDER, RTU,
                   ["src/Utils/EcuTelemetry.cpp", "src/Utils/TelemetrySender.cpp"])
    receiver = build(args.compiler, build_dir, "receiver", RECEIVER, RRU,
                     ["src/Utils/TelemetryReceiver.cpp", "src/Utils/TelemetryCsv.cpp"])
    rng = random.Random(260922)
    random_frames = {can_id: rng.getrandbits(64) for can_id in messages}
    cases = [("no sources", {}, 100), ("random", random_frames, 100),
             ("freshness boundary", random_frames, 2100), ("stale", random_frames, 2101),
             ("zero", dict.fromkeys(messages, 0), 100),
             ("ones", dict.fromkeys(messages, (1 << 64) - 1), 100)]
    # Signed minima / maxima expose truncation, endian errors and abs(INT_MIN).
    for high in [False, True]:
        frames = {}
        for can_id, signals in messages.items():
            bits = 0
            for _, start, size, signed, _, _ in signals:
                value = ((1 << (size - int(signed))) - 1) if high else ((1 << (size - 1)) if signed else 0)
                bits |= value << start
            frames[can_id] = bits
        cases.append(("maxima" if high else "minima", frames, 100))
    for can_id in messages:
        cases.append((f"only {can_id:#x}", {can_id: random_frames[can_id]}, 100))
    # Each absent source must blank its own fields while all others continue.
    for can_id in messages:
        cases.append((f"missing {can_id:#x}", {key: value for key, value in random_frames.items() if key != can_id}, 100))

    checks = 0
    for label, frames, now in cases:
        lines = [f"{now} {len(frames)}"]
        for can_id, bits in frames.items():
            lines.append(f"{can_id} 100 " + " ".join(str(value) for value in bits.to_bytes(8, "little")))
        wire = subprocess.run([str(sender)], input="\n".join(lines), text=True, capture_output=True, check=True).stdout
        output = subprocess.run([str(receiver)], input=wire, text=True, capture_output=True, check=True).stdout
        rows = list(csv.DictReader(io.StringIO(output)))
        host_packets = parse_lines(output.splitlines())
        assert len(rows) == 4, label
        assert len(host_packets) == 4, label
        assert all(None not in row for row in rows), label
        received = sum(1 << index for index, can_id in enumerate(messages) if can_id in frames)
        for row in rows:
            # The DBC is the complete measurement inventory, alongside transport metadata.
            assert set(row) == set(COLUMNS.values()) | METADATA_COLUMNS, label
            assert len(row) == 56, label
            assert int(row["schema_version"]) == 3, label
            assert int(row["received_mask_hex"], 16) == received, label
            assert int(row["fresh_mask_hex"], 16) == (received if now <= 2100 else 0), label
        for can_id, signals in messages.items():
            for name, start, size, signed, scale, offset in signals:
                values = [Decimal(row[COLUMNS[name]]) for row in rows if row[COLUMNS[name]] != ""]
                host_values = [Decimal(str(packet.payload[COLUMNS[name]])) for packet in host_packets
                               if COLUMNS[name] in packet.payload]
                assert host_values == values, (label, name, "host parser dropped/changed values", host_values, values)
                if can_id not in frames:
                    assert values == [], (label, name, values)
                else:
                    raw = (frames[can_id] >> start) & ((1 << size) - 1)
                    if signed and raw & (1 << (size - 1)):
                        raw -= 1 << size
                    expected = raw * scale + offset
                    assert values and all(value == expected for value in values), (label, name, expected, values)
                checks += 1
    print(f"PASS: {len(cases)} CAN-to-LoRa-to-CSV-to-host scenarios; {checks} DBC signal checks ({len(COLUMNS)} signals)")


if __name__ == "__main__":
    main()
