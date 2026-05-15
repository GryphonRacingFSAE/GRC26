"""Telemetry field definitions for the GRC LoRa telemetry receiver.

This schema is matched to the uploaded CAN task. The CAN task currently emits
three telemetry packet types:

- FAST: main driver/engine data from CAN messages 0x520, 0x530, 0x537 and 0x522
- SLOW: slower diagnostic/temperature data from CAN messages 0x530, 0x534 and 0x537
- EVENT: change-triggered alert/status data

The RX board prints one CSV row per LoRa packet. FAST/SLOW/EVENT rows share a
common superset header; unused values are blank.
"""

from __future__ import annotations

CSV_HEADER = [
    "event",
    "rx_count",
    "rx_ms",
    "rssi_dbm",
    "snr_db",
    "radio_len",
    "packet_type",
    "seq",
    "tx_ms",
    "alert_flags_hex",
    "status_bits_hex",
    "rpm",
    "tps_pct",
    "map_kpa",
    "lambda_avg",
    "lambda_error",
    "fuel_pressure_kpa",
    "coolant_temp_c",
    "battery_v",
    "vehicle_speed_kph",
    "intake_air_temp_c",
    "ecu_error_count",
    "ecu_lost_sync_count",
    "ecu_temp_c",
    "egt_delta_c",
    "coolant_pressure_kpa",
]

STRING_FIELDS = {
    "event",
    "packet_type",
    "alert_flags_hex",
    "status_bits_hex",
}

INT_FIELDS = {
    "rx_count",
    "rx_ms",
    "radio_len",
    "seq",
    "tx_ms",
    "rpm",
    "ecu_error_count",
    "ecu_lost_sync_count",
    "ecu_temp_c",
    "egt_delta_c",
}

# Every other known numeric field is a float. This keeps Foxglove plots smooth
# for scaled CAN values such as x10, x100 and x1000 fields.
FLOAT_FIELDS = set(CSV_HEADER) - STRING_FIELDS - INT_FIELDS

FAST_FIELDS = [
    "rx_count",
    "rx_ms",
    "rssi_dbm",
    "snr_db",
    "radio_len",
    "packet_type",
    "seq",
    "tx_ms",
    "status_bits_hex",
    "status_bits",
    "rpm",
    "tps_pct",
    "map_kpa",
    "lambda_avg",
    "lambda_error",
    "fuel_pressure_kpa",
    "coolant_temp_c",
    "battery_v",
    "vehicle_speed_kph",
]

SLOW_FIELDS = [
    "rx_count",
    "rx_ms",
    "rssi_dbm",
    "snr_db",
    "radio_len",
    "packet_type",
    "seq",
    "tx_ms",
    "intake_air_temp_c",
    "ecu_error_count",
    "ecu_lost_sync_count",
    "ecu_temp_c",
    "egt_delta_c",
    "coolant_pressure_kpa",
]

EVENT_FIELDS = [
    "rx_count",
    "rx_ms",
    "rssi_dbm",
    "snr_db",
    "radio_len",
    "packet_type",
    "seq",
    "tx_ms",
    "alert_flags_hex",
    "alert_flags",
    "status_bits_hex",
    "status_bits",
    "rpm",
    "fuel_pressure_kpa",
    "coolant_temp_c",
    "battery_v",
    "ecu_error_count",
    "ecu_lost_sync_count",
]

ALL_NUMERIC_ALIASES = {"alert_flags", "status_bits"}

TOPICS = {
    "FAST": ("/telemetry/fast", FAST_FIELDS),
    "SLOW": ("/telemetry/slow", SLOW_FIELDS),
    "EVENT": ("/telemetry/event", EVENT_FIELDS),
}


def json_schema_for_fields(name: str, fields: list[str]) -> dict:
    """Create a Foxglove-compatible JSON schema for a telemetry channel."""
    props: dict[str, dict] = {}

    for field in fields:
        if field in STRING_FIELDS:
            props[field] = {"type": "string"}
        else:
            # JSON has one numeric type. Python ints/floats are both valid.
            props[field] = {"type": "number"}

    return {
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "title": name,
        "type": "object",
        "additionalProperties": False,
        "properties": props,
    }
