import sys
import serial
import serial.tools.list_ports
import time, math, random
import pyqtgraph as pg
from PyQt6.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QWidget, QPushButton, QComboBox, QHBoxLayout
from PyQt6.QtCore import QTimer

class DynoApp(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Dyno Logger MVP")
        self.resize(800, 600)

        # Main Layout
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)

        # Top Control Bar (COM Port Selection)
        top_bar = QHBoxLayout()
        self.port_selector = QComboBox()
        self.refresh_ports()
        self.connect_btn = QPushButton("Connect")
        self.connect_btn.clicked.connect(self.toggle_connection)
        
        top_bar.addWidget(self.port_selector)
        top_bar.addWidget(self.connect_btn)
        layout.addLayout(top_bar)

        # Plot Widget
        self.plot_widget = pg.PlotWidget(title="Live Dyno Data")
        self.plot_widget.setLabel('left', 'Torque / HP')
        self.plot_widget.setLabel('bottom', 'Time (samples)')
        self.plot_widget.addLegend()
        
        self.torque_line = self.plot_widget.plot(pen='r', name="Torque")
        self.hp_line = self.plot_widget.plot(pen='b', name="HP")
        
        # Marker Lines
        self.marker_lines = []
        self.marker_events = []  
        self.last_rpm = None
        self.last_torque = None
        self.last_hp = None

        self.marker_btn = QPushButton("Marker")
        self.marker_btn.clicked.connect(self.drop_marker)
        
        layout.addWidget(self.plot_widget)

        # Bottom Control Bar (Start/Stop Run)
        bottom_bar = QHBoxLayout()
        self.start_btn = QPushButton("Start Run")
        self.stop_btn = QPushButton("Stop & Save Run")
        self.start_btn.clicked.connect(self.start_run)
        self.stop_btn.clicked.connect(self.stop_run)
        
        bottom_bar.addWidget(self.start_btn)
        bottom_bar.addWidget(self.stop_btn)
        bottom_bar.addWidget(self.marker_btn)
        layout.addLayout(bottom_bar)

        # Data Buffers
        self.is_running = False
        self.torque_data = []
        self.hp_data = []
        
        # Demo 
        self.demo_t0 = time.time()
        self.demo_rpm = 2000
        
        # Serial & Timer
        self.serial_port = None
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_plot)
        self.timer.start(30) 

    def refresh_ports(self):
        self.port_selector.clear()
        ports = serial.tools.list_ports.comports()
        for port in ports:
            self.port_selector.addItem(port.device)

    def toggle_connection(self):
        if self.serial_port is None:
            port = self.port_selector.currentText()
            try:
                self.serial_port = serial.Serial(port, 115200, timeout=0.02)
                self.connect_btn.setText("Disconnect")
                self.timer.start(30) # ~33Hz GUI update
            except Exception as e:
                print(f"Failed to connect: {e}")
        else:
            self.timer.stop()
            self.serial_port.close()
            self.serial_port = None
            self.connect_btn.setText("Connect")

    def start_run(self):
        self.is_running = True
        self.torque_data = []
        self.hp_data = []

    def stop_run(self):
        self.is_running = False
        # Here you would trigger the CSV save function
        print(f"Run stopped. Gathered {len(self.torque_data)} samples.")
        
    def drop_marker(self):
        # Place marker at newest sample on the plot
        if len(self.torque_data) == 0:
            return

        x = len(self.torque_data) - 1  # "live timestamp" = most recent index

        line = pg.InfiniteLine(
            pos=x, angle=90, movable=False,
            pen=pg.mkPen('y', width=2)
        )
        self.plot_widget.addItem(line)
        self.marker_lines.append(line)

        # Save marker info for export/logging
        self.marker_events.append({
            "sample_index": x,
            "rpm": self.last_rpm,
            "torque": self.last_torque,
            "hp": self.last_hp,
        })

    def update_plot(self):
        if RunDemo:
            self.demo_rpm = min(self.demo_rpm + 25, 12000)  # ramp RPM
            rpm = self.demo_rpm
            torque = 40 + 25*math.sin((rpm-2000)/2500) + random.uniform(-1.0, 1.0)
            hp = torque * rpm / 5252.0

            if self.is_running:
                self.torque_data.append(torque)
                self.hp_data.append(hp)
                self.torque_line.setData(self.torque_data)
                self.hp_line.setData(self.hp_data)
                self.last_rpm = rpm
                self.last_torque = torque
                self.last_hp = hp
            return
        
        if self.serial_port and self.serial_port.in_waiting > 0:
            try:
                # Read line from ESP32
                line = self.serial_port.readline().decode('utf-8').strip()
                if line:
                    rpm, torque, hp = map(float, line.split(','))
                    
                    if self.is_running:
                        self.torque_data.append(torque)
                        self.hp_data.append(hp)
                        
                        # Update the graph arrays
                        self.torque_line.setData(self.torque_data)
                        self.hp_line.setData(self.hp_data)
            except Exception as e:
                pass # Ignore malformed serial strings during live plot

if __name__ == "__main__":
    app = QApplication(sys.argv)
    RunDemo = True
    window = DynoApp()
    window.show()
    sys.exit(app.exec())
