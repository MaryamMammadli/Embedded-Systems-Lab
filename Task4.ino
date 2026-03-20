#app.py TASK 4
import sys, time
from collections import deque
import numpy
import serial
from PyQt6.QtCore import QTimer
from PyQt6.QtWidgets import QApplication, QWidget, QLabel, QPushButton, QHBoxLayout, QVBoxLayout
import pyqtgraph as pg


COM_PORT = "COM22"   # change if needed
BAUD = 9600
WINDOW_S = 5         # show last N seconds
MAX_POINTS = 400


class Main(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Joystick (Minimal + Start/Stop)")

        self.btn = QPushButton("Start")
        self.btn.clicked.connect(self.toggle)

        self.lbl_status = QLabel("● DISCONNECTED")
        self.lbl_status.setStyleSheet("font-weight:bold; color:#aa0000;")

        self.lbl = QLabel("Waiting data...")
        self.lbl.setStyleSheet("font-size:16px;")

        self.lbl_rate = QLabel("Sample rate: 0.0 Hz")

        top = QHBoxLayout()
        top.addWidget(self.btn)
        top.addWidget(self.lbl_status)
        top.addStretch(1)
        top.addWidget(self.lbl_rate)

        self.xy = pg.PlotWidget(title="Joystick (dx,dy)")
        self.xy.setLabel("bottom", "dx")
        self.xy.setLabel("left", "dy")
        self.xy.setXRange(-600, 600)
        self.xy.setYRange(-600, 600)
        self.xy.showGrid(x=True, y=True)
        self.xy.addLine(x=0); self.xy.addLine(y=0)
        self.dot = pg.ScatterPlotItem([0], [0], size=14)
        self.xy.addItem(self.dot)

        self.g = pg.PlotWidget(title="LR & UD (live)")
        self.g.setLabel("bottom", "Time (s)")
        self.g.setLabel("left", "ADC")
        self.g.setYRange(0, 1023)
        self.g.showGrid(x=True, y=True)
        self.lr_curve = self.g.plot([], [])
        self.ud_curve = self.g.plot([], [])

        lay = QVBoxLayout()
        lay.addLayout(top)
        lay.addWidget(self.lbl)
        lay.addWidget(self.xy, 1)
        lay.addWidget(self.g, 1)
        self.setLayout(lay)

        self.running = False
        self.ser = None
        self.rx = b""

        # data for plots = stores graph data
        self.t0 = None
        self.t = deque(maxlen=MAX_POINTS)
        self.lr = deque(maxlen=MAX_POINTS)
        self.ud = deque(maxlen=MAX_POINTS)

        # sample rate
        self.sample_count = 0 #total
        self.last_rate_time = 0.0 #last update
        self.last_rate_count = 0 #samples at last update

        # timer always alive but only reads when running, runs every 20ms
        self.timer = QTimer()
        self.timer.setInterval(20)
        self.timer.timeout.connect(self.tick) #read real data update gui
        self.timer.start()

    def start(self):
        self.ser = serial.Serial(COM_PORT, BAUD, timeout=0)
        self.ser.setDTR(False)
        time.sleep(0.2)
        self.ser.reset_input_buffer()

        # reset buffers/timing
        self.rx = b""
        self.t.clear(); self.lr.clear(); self.ud.clear()
        self.t0 = time.time()

        self.sample_count = 0
        self.last_rate_time = self.t0
        self.last_rate_count = 0
        self.lbl_rate.setText("Sample rate: 0.0 Hz")

        self.running = True
        self.btn.setText("Stop")
        self.lbl_status.setText("● CONNECTED")
        self.lbl_status.setStyleSheet("font-weight:bold; color:#00aa00;")

    def stop(self):
        self.running = False
        self.btn.setText("Start")
        self.lbl_status.setText("● DISCONNECTED")
        self.lbl_status.setStyleSheet("font-weight:bold; color:#aa0000;")
        if self.ser:
            try:
                self.ser.close()
            except:
                pass
        self.ser = None

    def toggle(self):
        try:
            if not self.running:
                self.start()
            else:
                self.stop()
        except Exception as e:
            self.stop()
            self.lbl.setText(f"ERROR: {e}")

    def handle_line(self, s: str):
        parts = s.split(",")
        if len(parts) != 5:
            return
        try:
            lr = int(parts[0]); ud = int(parts[1])
            dx = int(parts[2]); dy = int(parts[3])
            direction = parts[4].strip()
        except:
            return

        # label + dot
        self.lbl.setText(f"{direction}   LR:{lr}  UD:{ud}   dx:{dx} dy:{dy}")
        dx = max(-600, min(600, dx))
        dy = max(-600, min(600, dy))
        self.dot.setData([dx], [dy])

        # plot
        now = time.time()
        tt = now - self.t0
        self.t.append(tt); self.lr.append(lr); self.ud.append(ud)
        t_list = list(self.t)
        self.lr_curve.setData(t_list, list(self.lr))
        self.ud_curve.setData(t_list, list(self.ud))
        if tt > WINDOW_S:
            self.g.setXRange(tt - WINDOW_S, tt)

        # sample rate update every 0.5 s
        self.sample_count += 1
        if (now - self.last_rate_time) >= 0.5:
            dt = now - self.last_rate_time
            dn = self.sample_count - self.last_rate_count
            hz = dn / dt if dt > 0 else 0.0
            self.lbl_rate.setText(f"Sample rate: {hz:.1f} Hz")
            self.last_rate_time = now
            self.last_rate_count = self.sample_count

    def tick(self):
        if not self.running or not self.ser:
            return
        try:
            chunk = self.ser.read(self.ser.in_waiting or 1)
            if chunk:
                self.rx += chunk

            while b"\n" in self.rx:
                raw, self.rx = self.rx.split(b"\n", 1)
                self.handle_line(raw.decode(errors="ignore").strip())
        except Exception as e:
            self.lbl.setText(f"Serial read error: {e}")
            self.stop()

    def closeEvent(self, e):
        self.stop()
        e.accept()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    w = Main()
    w.resize(900, 600)
    w.show()
    sys.exit(app.exec())
