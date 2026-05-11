"""Telemetry field definitions for the new GRC LoRa telemetry receiver.

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
    "oil_pressure_kpa",
    "fuel_pressure_kpa",
    "coolant_temp_c",
    "battery_v",
    "vehicle_speed_kph",
    "gear",
    "oil_temp_c",
    "intake_air_temp_c",
    "fuel_inj_duty_pct",
    "fuel_trim_total_pct",
    "lambda_corr_a_pct",
    "lambda_corr_b_pct",
    "ignition_timing_deg",
    "ignition_cut_pct",
    "fuel_cut_pct",
    "ecu_error_count",
    "ecu_lost_sync_count",
    "ecu_temp_c",
    "egt_highest_c",
    "egt_delta_c",
    "knock_count",
    "knock_correction_deg",
    "boost_target_kpa",
    "boost_duty_pct",
    "coolant_pressure_kpa",
    "wastegate_pressure_kpa",
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
    "gear",
    "ecu_error_count",
    "ecu_lost_sync_count",
    "egt_highest_c",
    "egt_delta_c",
    "knock_count",
}

# Every other numeric field is a float. This makes Foxglove plotting smoother.
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
    "oil_pressure_kpa",
    "fuel_pressure_kpa",
    "coolant_temp_c",
    "battery_v",
    "vehicle_speed_kph",
    "gear",
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
    "oil_temp_c",
    "intake_air_temp_c",
    "fuel_inj_duty_pct",
    "fuel_trim_total_pct",
    "lambda_corr_a_pct",
    "lambda_corr_b_pct",
    "ignition_timing_deg",
    "ignition_cut_pct",
    "fuel_cut_pct",
    "ecu_error_count",
    "ecu_lost_sync_count",
    "ecu_temp_c",
    "egt_highest_c",
    "egt_delta_c",
    "knock_count",
    "knock_correction_deg",
    "boost_target_kpa",
    "boost_duty_pct",
    "coolant_pressure_kpa",
    "wastegate_pressure_kpa",
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
    "oil_pressure_kpa",
    "fuel_pressure_kpa",
    "coolant_temp_c",
    "battery_v",
    "lambda_error",
    "ecu_error_count",
    "ecu_lost_sync_count",
    "knock_count",
]

ALL_NUMERIC_ALIASES = {"alert_flags", "status_bits"}

TOPICS = {
    "FAST": ("/telemetry/fast", FAST_FIELDS),
    "SLOW": ("/telemetry/slow", SLOW_FIELDS),
    "EVENT": ("/telemetry/event", EVENT_FIELDS),
}


def json_schema_for_fields(name: str, fields: list[str]) -> dict:
    """Create a small JSON schema for a Foxglove JSON channel."""
    props: dict[str, dict] = {}

    for field in fields:
        if field in STRING_FIELDS:
            props[field] = {"type": ["string", "null"]}
        else:
            # JSON has one numeric type. Python ints/floats are both valid.
            props[field] = {"type": ["number", "null"]}

    return {
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "title": name,
        "type": "object",
        "additionalProperties": False,
        "properties": props,
    }
