"""Current GRC26 DBC signal and receiver CSV definitions.

The 56 columns match Embedded/src/Utils/TelemetryCsv.cpp: 16 transport metadata
columns followed by the DBC's 40 signals. Missing measurements remain optional.
"""

from __future__ import annotations

SCHEMA_VERSION = 3

CSV_HEADER = """
event rx_count rx_ms rssi_dbm snr_db radio_len packet_type seq tx_ms schema_version
received_mask_hex fresh_mask_hex alert_flags_hex status_bits_hex error_code error_text
rpm tps_pct lambda_avg lambda_a lambda_b fuel_inj_pulse_width_ms fuel_inj_duty_pct
vehicle_speed_kph knock_detected brake_pedal_active clutch_pedal_active rev_limit_rpm
lambda_target battery_v intake_air_temp_c coolant_temp_c gear user_channel_1
aero_pressure_1_pa aero_pressure_2_pa aero_ambient_temp_c aero_ambient_pressure_hpa
aero_node_state aero_sensor_flags aero_fault_flags aero_sequence acceleration_x_g
acceleration_y_g acceleration_z_g yaw_rate_dps pitch_rate_dps roll_rate_dps
imu_node_state imu_sensor_flags imu_fault_flags imu_sequence gps_latitude_deg
gps_longitude_deg gps_ground_speed_kph gps_course_deg
""".split()

STRING_FIELDS = {
    "event", "packet_type", "alert_flags_hex", "status_bits_hex",
    "received_mask_hex", "fresh_mask_hex", "error_text",
}
STATUS_FIELDS = ["knock_detected", "brake_pedal_active", "clutch_pedal_active"]
INT_FIELDS = set("""
rx_count rx_ms radio_len seq tx_ms error_code schema_version rpm gear rev_limit_rpm
knock_detected brake_pedal_active clutch_pedal_active aero_pressure_1_pa
 aero_pressure_2_pa aero_node_state aero_sensor_flags aero_fault_flags aero_sequence
imu_node_state imu_sensor_flags imu_fault_flags imu_sequence
""".split())
FLOAT_FIELDS = set(CSV_HEADER) - STRING_FIELDS - INT_FIELDS

COMMON_FIELDS = """
rx_count rx_ms rssi_dbm snr_db radio_len packet_type seq tx_ms schema_version
received_mask_hex fresh_mask_hex received_mask fresh_mask
""".split()
STATUS_PAYLOAD_FIELDS = ["status_bits_hex", "status_bits"] + STATUS_FIELDS
NODE_FIELDS = """
aero_node_state aero_sensor_flags aero_fault_flags aero_sequence
imu_node_state imu_sensor_flags imu_fault_flags imu_sequence
""".split()

FAST_FIELDS = COMMON_FIELDS + STATUS_PAYLOAD_FIELDS + """
rpm tps_pct lambda_avg vehicle_speed_kph battery_v gear rev_limit_rpm user_channel_1
""".split()
SLOW_FIELDS = COMMON_FIELDS + """
lambda_a lambda_b lambda_target fuel_inj_pulse_width_ms fuel_inj_duty_pct
intake_air_temp_c coolant_temp_c
""".split()
EVENT_FIELDS = COMMON_FIELDS + STATUS_PAYLOAD_FIELDS + NODE_FIELDS + [
    "alert_flags_hex", "alert_flags", "rpm",
]
SENSORS_FIELDS = COMMON_FIELDS + NODE_FIELDS + """
aero_pressure_1_pa aero_pressure_2_pa aero_ambient_temp_c aero_ambient_pressure_hpa
acceleration_x_g acceleration_y_g acceleration_z_g yaw_rate_dps pitch_rate_dps
roll_rate_dps gps_latitude_deg gps_longitude_deg gps_ground_speed_kph gps_course_deg
""".split()

PACKET_FIELDS = {
    "FAST": FAST_FIELDS,
    "SLOW": SLOW_FIELDS,
    "EVENT": EVENT_FIELDS,
    "SENSORS": SENSORS_FIELDS,
}


def is_current_header(header: list[str]) -> bool:
    """Accept exactly the current columns, optionally in a different order."""
    return len(header) == len(CSV_HEADER) and header[0] == "event" and set(header) == set(CSV_HEADER)
