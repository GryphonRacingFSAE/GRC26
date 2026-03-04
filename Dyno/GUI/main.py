import sys
import os
import csv
import time
import math
import random
import serial
import serial.tools.list_ports
import pyqtgraph as pg
from PyQt6.QtWidgets import (QApplication, QMainWindow, QVBoxLayout, QWidget, 
                             QPushButton, QComboBox, QHBoxLayout, QLineEdit, QLabel, QMessageBox)
from PyQt6.QtCore import QTimer

# Set to false during norm ops
RunDemo = False

class DynoApp(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Dyno Logger")
        self.resize(10000, 7000)

        # Main Layout
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)

        # Top Control Bar (COM Port Selection & Filename)
        top_bar = QHBoxLayout()
        self.port_selector = QComboBox()
        self.refresh_ports()
        
        self.refresh_btn = QPushButton("Refresh Ports")
        self.refresh_btn.clicked.connect(self.refresh_ports)
        
        self.connect_btn = QPushButton("Connect")
        self.connect_btn.clicked.connect(self.toggle_connection)

        self.filename_input = QLineEdit("Enter File Name")
        
        top_bar.addWidget(QLabel("COM Port:"))
        top_bar.addWidget(self.port_selector)
        top_bar.addWidget(self.refresh_btn)
        top_bar.addWidget(self.connect_btn)
        top_bar.addSpacing(50)
        top_bar.addWidget(QLabel("Output Filename:"))
        top_bar.addWidget(self.filename_input)
        layout.addLayout(top_bar)

        # ---------------------------------------------------------
        # PLOT WIDGET SETUP (Dual Y-Axis & RPM X-Axis)
        # ---------------------------------------------------------
        self.plot_widget = pg.PlotWidget(title="Live Dyno Data")
        label_style = {'color': '#FFF', 'font-size': '14pt', 'font-weight': 'bold'}
        self.plot_widget.setLabel('bottom', 'RPM', **label_style)
        self.plot_widget.setLabel('left', 'Torque (ft-lbs)', **label_style)
        self.plot_widget.setLabel('right', 'Horsepower (HP)', **label_style)
        self.plot_widget.addLegend(offset=(10, 10))
        layout.addWidget(self.plot_widget)
        
        self.plot_widget.plotItem.layout.setContentsMargins(10, 10, 30, 10)
        self.plot_widget.showGrid(x=True, y=True, alpha=0.7)
        
        # Main ViewBox (Left Axis) - Torque
        self.main_view = self.plot_widget.plotItem.vb   
        self.torque_line = self.plot_widget.plot(pen=pg.mkPen('r', width=2), name="Torque")

        self.plot_widget.setXRange(0, 16000, padding = 0.0)
        self.plot_widget.setYRange(0, 75,    padding = 0.0)
        self.main_view.setMouseEnabled(x = False, y = False)  # Disable zooming/panning 
        
        # Secondary ViewBox (Right Axis) - HP
        self.hp_view = pg.ViewBox()
        self.plot_widget.plotItem.scene().addItem(self.hp_view)
        self.plot_widget.plotItem.getAxis('right').linkToView(self.hp_view)
        self.hp_view.setXLink(self.main_view)

        self.hp_view.setYRange(0, 150, padding = 0.0)
        self.hp_view.setMouseEnabled(x = False, y = False)  # Disable zoom

        # Add HP Curve to secondary view
        self.hp_line = pg.PlotCurveItem(pen=pg.mkPen('b', width=2), name="Horesepower")
        self.hp_view.addItem(self.hp_line)
        
        self.legend = self.plot_widget.addLegend(offset=(10, 10))
        self.legend.addItem(self.hp_line, "Horsepower")

        # Keep secondary view geometry synced with main view
        def update_views():
            self.hp_view.setGeometry(self.main_view.sceneBoundingRect())
            self.hp_view.linkedViewChanged(self.main_view, self.hp_view.XAxis)

        update_views()
        self.main_view.sigResized.connect(update_views)

        # ---------------------------------------------------------
        # Bottom Control Bar (Start/Stop Run & Marker)
        # ---------------------------------------------------------
        bottom_bar = QHBoxLayout()
        self.start_btn  = QPushButton("Start Run")
        self.stop_btn   = QPushButton("Stop & Save Run")
        self.marker_btn = QPushButton("Drop Marker")
        
        self.start_btn.clicked.connect(self.start_run)
        self.stop_btn.clicked.connect(self.stop_run)
        self.marker_btn.clicked.connect(self.drop_marker)
        
        bottom_bar.addWidget(self.start_btn)
        bottom_bar.addWidget(self.stop_btn)
        bottom_bar.addWidget(self.marker_btn)
        layout.addLayout(bottom_bar)

        # Data Buffers
        self.is_running = False
        self.rpm_data = []
        self.torque_data = []
        self.hp_data = []
        
        # Marker state
        self.marker_lines = []
        self.marker_events = []  
        self.last_rpm = None
        self.last_torque = None
        self.last_hp = None

        # Demo generator state
        self.demo_rpm = 2000
        
        # Serial & Timer
        self.serial_port = None
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_plot)
        self.timer.start(30) 


    def refresh_ports(self):
        current = self.port_selector.currentText()
        self.port_selector.clear()
        ports = serial.tools.list_ports.comports()
        for port in ports:
            self.port_selector.addItem(port.device)
            if port.device == current:
                self.port_selector.setCurrentText(current)


    def toggle_connection(self):
        if self.serial_port is None:
            port = self.port_selector.currentText()
            if not port and not RunDemo:
                return
            try:
                if not RunDemo:
                    self.serial_port = serial.Serial(port, 115200, timeout=0.02)
                self.connect_btn.setText("Disconnect")
            except Exception as e:
                QMessageBox.critical(self, "Error", f"Failed to connect to port {port}")
                
        else:
            self.serial_port.close()
            self.serial_port = None
            self.connect_btn.setText("Connect")


    def start_run(self):
        self.is_running = True
        
        # Clear data arrays
        self.rpm_data.clear()
        self.torque_data.clear()
        self.hp_data.clear()
        
        # Clear UI lines
        self.torque_line.setData([], [])
        self.hp_line.setData([], [])
        
        # Clear old markers from the plot
        for m in self.marker_lines:
            self.plot_widget.removeItem(m)
        self.marker_lines.clear()
        self.marker_events.clear()


    def stop_run(self):
        if not self.is_running:
            return
            
        self.is_running = False
        print(f"Run stopped. Gathered {len(self.torque_data)} samples.")
        self.save_data_to_csv()


    def save_data_to_csv(self):
        if not self.torque_data:
            return
            
        filename = self.filename_input.text().strip()
        if not filename:
            filename = "Unnamed_Run"
            
        os.makedirs("outputs", exist_ok=True)
        
        filepath = os.path.join("outputs", f"{filename}.csv")
        marker_filepath = os.path.join("outputs", f"{filename}_markers.csv")

        try:
            with open(filepath, mode='w', newline='') as f:
                writer = csv.writer(f)
                writer.writerow(["Torque", "HP", "RPM"])
                for t, h, r in zip(self.torque_data, self.hp_data, self.rpm_data):
                    writer.writerow([t, h, r])
            
            if self.marker_events:
                with open(marker_filepath, mode='w', newline='') as f:
                    writer = csv.writer(f)
                    writer.writerow(["Torque", "HP", "RPM", "Note"])
                    for m in self.marker_events:
                        writer.writerow([m["torque"], m["hp"], m["rpm"], "Marker Drop"])
                        
            QMessageBox.information(self, "Success", f"Saved {len(self.torque_data)} samples to {filepath}")
            
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to save CSV: {e}")


    def drop_marker(self):
        if self.last_rpm is None:
            return

        # Place vertical marker anchored to the most recent RPM seen
        x_pos = self.last_rpm 
        line = pg.InfiniteLine(
            pos=x_pos, angle=90, movable=False,
            pen=pg.mkPen('y', width=2, style=pg.QtCore.Qt.PenStyle.DashLine)
        )
        self.plot_widget.addItem(line)
        self.marker_lines.append(line)

        # Save marker info for export logging
        self.marker_events.append({
            "rpm": self.last_rpm,
            "torque": self.last_torque,
            "hp": self.last_hp,
        })


    def update_plot(self):
        if RunDemo and self.is_running:
            
            self.demo_rpm = min(self.demo_rpm + 25, 12000)
            rpm = self.demo_rpm
            
            torque = 40 + 25*math.sin((rpm-2000)/2500) + random.uniform(-1.0, 1.0)
            hp = torque * rpm / 5252.0

            self.last_rpm = rpm
            self.last_torque = torque
            self.last_hp = hp

            self.rpm_data.append(rpm)
            self.torque_data.append(torque)
            self.hp_data.append(hp)
            
            self.torque_line.setData(x=self.rpm_data, y=self.torque_data)
            self.hp_line.setData(x=self.rpm_data, y=self.hp_data)
            return

        if self.serial_port and self.serial_port.in_waiting > 0:
            new_data_arrived = False
            
            while self.serial_port.in_waiting > 0:
                try:
                    line = self.serial_port.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        torque, hp, rpm = map(float, line.split(','))
                        
                        self.last_torque = torque
                        self.last_hp = hp
                        self.last_rpm = rpm

                        if self.is_running:
                            self.rpm_data.append(rpm)
                            self.torque_data.append(torque)
                            self.hp_data.append(hp)
                            new_data_arrived = True
                            
                except Exception:
                    pass # Ignore mangled strings during transmission

            if self.is_running and new_data_arrived:
                self.torque_line.setData(x=self.rpm_data, y=self.torque_data)
                self.hp_line.setData(x=self.rpm_data, y=self.hp_data)


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = DynoApp()
    window.show()
    sys.exit(app.exec())