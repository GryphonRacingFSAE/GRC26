# GRC LoRa Telemetry Viewer

Python receiver-side software for the new Gryphon Racing LoRa telemetry boards.

This replaces the old Remote Logging Module flow:

- old board printed raw CAN frames as `address|data|time`
- old Python loaded DBC files with `cantools`, decoded every CAN frame, pushed JSON into Foxglove, and wrote MCAP logs

The new LoRa RX board already receives compact telemetry packets and prints CSV rows. This project reads those rows directly, publishes them to Foxglove as JSON channels, and records MCAP logs.

## Expected serial input

The RX board should print a CSV header once, followed by packet rows:

```csv
event,rx_count,rx_ms,rssi_dbm,snr_db,radio_len,packet_type,seq,tx_ms,alert_flags_hex,status_bits_hex,rpm,tps_pct,map_kpa,lambda_avg,lambda_error,oil_pressure_kpa,fuel_pressure_kpa,coolant_temp_c,battery_v,vehicle_speed_kph,gear,oil_temp_c,intake_air_temp_c,fuel_inj_duty_pct,fuel_trim_total_pct,lambda_corr_a_pct,lambda_corr_b_pct,ignition_timing_deg,ignition_cut_pct,fuel_cut_pct,ecu_error_count,ecu_lost_sync_count,ecu_temp_c,egt_highest_c,egt_delta_c,knock_count,knock_correction_deg,boost_target_kpa,boost_duty_pct,coolant_pressure_kpa,wastegate_pressure_kpa
rx_packet,1,1000,-42.5,9.25,35,FAST,12,980,0x0000,0x0000,5200,34.2,98.4,1.002,-0.018,250.0,360.0,88.5,13.25,41.2,3,,,,,,,,,,,,,,,,,,
```

## Install

```bash
python -m venv .venv
.venv\Scripts\activate      # Windows
# source .venv/bin/activate  # macOS/Linux
pip install -e .
```

## Run Foxglove bridge

```bash
grc-lora-foxglove COM7 --baud 115200
```

Linux example:

```bash
grc-lora-foxglove /dev/ttyUSB0 --baud 115200
```

Then open Foxglove and connect to:

```text
ws://localhost:8765
```

The bridge creates these topics:

| Topic | Contents |
|---|---|
| `/telemetry/fast` | RPM, TPS, MAP, lambda, pressures, coolant, battery, speed, gear, status bits |
| `/telemetry/slow` | oil temp, IAT, fuel trims, ignition, ECU errors, EGT, knock, boost/coolant/wastegate pressure |
| `/telemetry/event` | alert/status changes and safety-critical snapshots |

MCAP logs are written to `./logs` unless disabled:

```bash
grc-lora-foxglove COM7 --no-mcap
```

## CSV-only logging

```bash
grc-lora-csv COM7 logs/session.csv --baud 115200 --print-rows
```

## Foxglove starter layout

Import `foxglove/grc_lora_starter_layout.json` into Foxglove as a starting point. If the layout import format changes in your Foxglove version, just create plots manually against the three topics above.

Recommended signals to plot first:

- `/telemetry/fast.rpm`
- `/telemetry/fast.lambda_error`
- `/telemetry/fast.oil_pressure_kpa`
- `/telemetry/fast.fuel_pressure_kpa`
- `/telemetry/fast.coolant_temp_c`
- `/telemetry/fast.battery_v`
- `/telemetry/slow.ecu_error_count`
- `/telemetry/slow.ecu_lost_sync_count`
- `/telemetry/event.alert_flags`

## Notes

- No DBC files are required because the TX/RX firmware already reduces the CAN bus into named telemetry fields.
- This is intentionally not forwarding the full CAN bus. That would fight the 915 MHz / 1 km range goal.
- If the firmware adds more CSV columns, the parser will ignore unknown fields unless you add them to `schema.py`.
