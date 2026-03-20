#app.py task5
import sys  #start gui            
import time                    
import csv       #save alert data 
from datetime import datetime   #generate real timestamps for logging

import serial                   # PySerial: enables UART/Serial communication with Arduino
import serial.tools.list_ports  

from PyQt6.QtCore import QTimer # QTimer calls a function repeatedly without freezing the GUI
from PyQt6.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QPushButton
)

import pyqtgraph as pg          

BAUD = 9600                     
THRESHOLD = 600              
SAMPLES_ON_SCREEN = 200         
UPDATE_MS = 50  #20 updates per second                
PORT = "COM21"                  

class SoundGUI(QWidget):
    def __init__(self):
        super().__init__()  # Initialize QWidget parent class
#window
        self.setWindowTitle("Sound Level Monitor (UART)")  
        self.resize(900, 550)                              

        self.ser = None        
        self.running = False    
#main layout 
        main = QVBoxLayout(self)      
        self.status = QLabel("Status: stopped") 
        main.addWidget(self.status)    
#button
        btn_row = QHBoxLayout() 
        main.addLayout(btn_row)

        self.start_btn = QPushButton("Start")  
        self.stop_btn = QPushButton("Stop") 
        self.stop_btn.setEnabled(False)   

        btn_row.addWidget(self.start_btn)    
        btn_row.addWidget(self.stop_btn)  

#plot
        self.plot = pg.PlotWidget()  
        main.addWidget(self.plot) 

        self.plot.setTitle("Live Sound Level")  
        self.plot.setLabel("left", "Sound (0–1023)") 
        self.plot.setLabel("bottom", "Sample #")    
        self.plot.showGrid(x=True, y=True) 

#data for plot
        self.x = []       
        self.y = []       
        self.sample_idx = 0   
        self.curve = self.plot.plot(self.x, self.y) 

#treshold line
        self.threshold_line = pg.InfiniteLine(pos=THRESHOLD, angle=0)  
        self.plot.addItem(self.threshold_line)           

#csv file
        self.csv_file = open("sound_threshold_log.csv", "a", newline="", encoding="utf-8")
        self.csv_writer = csv.writer(self.csv_file)

        # If file is empty write column headers once
        if self.csv_file.tell() == 0:
            self.csv_writer.writerow(["timestamp", "sound_value"])

#timer
        self.timer = QTimer()                      # QTimer triggers events repeatedly
        self.timer.timeout.connect(self.read_serial)  # Every timeout, call read_serial()

        # when Start is clicked, run start_monitoring()
        self.start_btn.clicked.connect(self.start_monitoring)
        # when Stop is clicked, run stop_monitoring()
        self.stop_btn.clicked.connect(self.stop_monitoring)

    def start_monitoring(self):
        """Connect to Arduino serial port and start reading live data."""
        if self.running:
            return  # Prevent starting again if already running

        # Choose COM port: manual PORT if set, otherwise auto-detect
        port = PORT or auto_detect_port()

        if port is None:
            # If Arduino not found, show message and do not start
            self.status.setText("Status: Arduino not found. Set PORT='COMx' in code.")
            return

        # Try to open serial port with BAUD rate
        try:
            self.ser = serial.Serial(port, BAUD, timeout=0.1)  # timeout makes readline non-blocking-ish
        except Exception as e:
            # If COM port cannot open (busy/wrong port), show the error
            self.status.setText(f"Status: cannot open {port}: {e}")
            return

        # Reset plot each time we start (optional)
        self.x.clear()
        self.y.clear()
        self.sample_idx = 0
        self.curve.setData(self.x, self.y)         # Update graph with empty data

        self.running = True                        # Now we are in running mode
        self.timer.start(UPDATE_MS)                # Start timer; read_serial called every UPDATE_MS ms

        # Update buttons to prevent wrong use
        self.start_btn.setEnabled(False)           # Start disabled while running
        self.stop_btn.setEnabled(True)             # Stop enabled while running

        # Show connection info on status label
        self.status.setText(f"Status: running (connected {port} @ {BAUD})")

    def stop_monitoring(self):
        """Stop reading, stop timer, and close serial port to free COM port."""
        if not self.running:
            return  # Nothing to stop if not running

        self.timer.stop()                          # Stop calling read_serial()
        self.running = False

        # Close serial port so Windows releases it
        try:
            if self.ser is not None and self.ser.is_open:
                self.ser.close()
        except:
            pass

        self.ser = None                             # Clear serial object

        # Update buttons
        self.start_btn.setEnabled(True)              # Allow start again
        self.stop_btn.setEnabled(False)

        self.status.setText("Status: stopped")       # Update status label


    def read_serial(self):
        """
        Reads incoming serial lines from Arduino.
        Expected Arduino format: SOUND:<value>
        Updates status label, logs alerts, and updates the plot.
        """
        if not self.running or self.ser is None:
            return  # Safety check

        try:
            # While there is data waiting in serial buffer, read it
            while self.ser.in_waiting:

                # Read one line from serial and decode bytes -> string
                line = self.ser.readline().decode(errors="ignore").strip()

                # Ignore lines not in expected format
                if not line.startswith("SOUND:"):
                    continue

                # Extract number after "SOUND:"
                try:
                    value = int(line.split(":", 1)[1])
                except ValueError:
                    continue  # Skip if conversion fails

                if value > THRESHOLD:
                    # ALERT condition: update status text and log it to CSV
                    self.status.setText(f"Status: ALERT | Sound={value} (>{THRESHOLD})")

                    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")  # Current timestamp
                    self.csv_writer.writerow([ts, value])              # Save to CSV
                    self.csv_file.flush()                               # Ensure data is written immediately
                else:
                    # Normal condition
                    self.status.setText(f"Status: running | Sound={value} (threshold {THRESHOLD})")

                self.sample_idx += 1                 # Increase sample counter
                self.x.append(self.sample_idx)       # Add x value
                self.y.append(value)                 # Add y value

                # Keep only last SAMPLES_ON_SCREEN points (sliding window)
                if len(self.x) > SAMPLES_ON_SCREEN:
                    self.x = self.x[-SAMPLES_ON_SCREEN:]
                    self.y = self.y[-SAMPLES_ON_SCREEN:]

                self.curve.setData(self.x, self.y)   # Redraw curve with new data

        except Exception as e:
            # If serial disconnects or another error happens
            self.status.setText(f"Status: serial read error: {e}")
            self.stop_monitoring()                   # Stop safely


    def closeEvent(self, event):
        
        self.stop_monitoring()  # Stop timer and serial first

        try:
            self.csv_file.close()  # Close CSV file
        except:
            pass

        event.accept()  # Allow window to close normally


if __name__ == "__main__":
    app = QApplication(sys.argv)   # Create Qt application object (must exist for GUI)
    w = SoundGUI()                 # Create main window from SoundGUI class
    w.show()                       # Show the window on screen
    sys.exit(app.exec())           # Start Qt event loop (keeps GUI running)
