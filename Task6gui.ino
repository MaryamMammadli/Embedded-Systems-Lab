import csv
import os
import queue
import threading
import time
import tkinter as tk
from collections import defaultdict
from datetime import datetime
from tkinter import messagebox, ttk

# Import matplotlib for graphs
import matplotlib
matplotlib.use("TkAgg")

# Import matplotlib canvas for Tkinter
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

# Import graph figure
from matplotlib.figure import Figure

# Import serial communication library
import serial
import serial.tools.list_ports

# Folder to save player files
DATA_DIR = "players"

# Create folder if it does not exist
os.makedirs(DATA_DIR, exist_ok=True)

# File to store match victories
VICTORY_LOG = os.path.join(DATA_DIR, "_victories.csv")

# ======================================================================
# Serial communication manager
# ======================================================================
class SerialManager:

    def __init__(self):

        # Serial object
        self.ser = None

        # Queue for received serial data
        self.rx_queue = queue.Queue()

        # Reader thread state
        self._running = False

        # Thread object
        self._thread = None

    @staticmethod
    def list_ports():

        # Return available COM ports
        return [p.device for p in serial.tools.list_ports.comports()]

    def connect(self, port, baud=9600):

        # Open serial connection
        self.ser = serial.Serial(port, baud, timeout=0.1)

        # Wait for Arduino reset
        time.sleep(2.0)

        # Clear old serial data
        self.ser.reset_input_buffer()

        # Start reader thread
        self._running = True

        # Create background thread
        self._thread = threading.Thread(target=self._reader, daemon=True)

        # Start thread
        self._thread.start()

    def disconnect(self):

        # Stop reader thread
        self._running = False

        try:
            if self.ser:

                # Close serial port
                self.ser.close()

        except Exception:
            pass

        # Remove serial object
        self.ser = None

    def is_open(self):

        # Check if serial port is connected
        return self.ser is not None and self.ser.is_open

    def _reader(self):

        # Temporary byte buffer
        buf = b""

        while self._running and self.ser:

            try:

                # Read serial data
                chunk = self.ser.read(128)

                if chunk:

                    # Add data to buffer
                    buf += chunk

                    # Check for complete line
                    while b"\n" in buf:

                        # Split line
                        line, buf = buf.split(b"\n", 1)

                        try:

                            # Convert bytes to text
                            text = line.decode("utf-8", errors="ignore").strip()

                        except Exception:
                            text = ""

                        # Save text into queue
                        if text:
                            self.rx_queue.put(text)

            except Exception:
                break

    def poll(self):

        try:

            # Return next serial message
            return self.rx_queue.get_nowait()

        except queue.Empty:
            return None

# ======================================================================
# Player data manager
# ======================================================================
class PlayerData:

    @staticmethod
    def safe_name(name):

        # Keep only safe filename characters
        clean = "".join(c for c in name if c.isalnum() or c in ("-", "_", " ")).strip()

        # Return cleaned name
        return clean or "unnamed"

    @classmethod
    def path_for(cls, name):

        # Return CSV file path
        return os.path.join(DATA_DIR, f"{cls.safe_name(name)}.csv")

    @classmethod
    def record_round(cls, player, opponent, rt_ms, won, false_start,
                     session_id, round_num):

        # Get player file path
        path = cls.path_for(player)

        # Check if file is new
        is_new = not os.path.exists(path)

        # Open file for appending
        with open(path, "a", newline="") as f:

            # Create CSV writer
            w = csv.writer(f)

            # Write header for new file
            if is_new:
                w.writerow([
                    "timestamp", "session_id", "round", "opponent",
                    "reaction_time_ms", "won", "false_start"
                ])

            # Write round data
            w.writerow([
                datetime.now().isoformat(timespec="seconds"),
                session_id, round_num, opponent,
                rt_ms if rt_ms is not None else "",
                int(bool(won)), int(bool(false_start))
            ])

    @staticmethod
    def record_victory(winner, loser, session_id, final_score):

        # Check if victory file exists
        is_new = not os.path.exists(VICTORY_LOG)

        # Open victory file
        with open(VICTORY_LOG, "a", newline="") as f:

            # Create CSV writer
            w = csv.writer(f)

            # Write header if new file
            if is_new:
                w.writerow(["timestamp", "session_id", "winner", "loser", "final_score"])

            # Save victory data
            w.writerow([
                datetime.now().isoformat(timespec="seconds"),
                session_id, winner, loser, final_score
            ])

    @classmethod
    def load_player(cls, name):

        # Get player file path
        path = cls.path_for(name)

        # Return empty list if file does not exist
        if not os.path.exists(path):
            return []

        # Load CSV data
        with open(path, newline="") as f:
            return list(csv.DictReader(f))

    @staticmethod
    def list_players():

        # Check if player folder exists
        if not os.path.isdir(DATA_DIR):
            return []

        # Return all player CSV names
        return sorted(
            f[:-4] for f in os.listdir(DATA_DIR)
            if f.endswith(".csv") and not f.startswith("_")
        )

# ======================================================================
# Main application class
# ======================================================================
class ReactionGameApp:

    # Program states
    IDLE   = "idle"
    ACTIVE = "active"
    ENDED  = "ended"

    def __init__(self, root):

        # Save Tkinter root window
        self.root = root

        # Set window title
        self.root.title("Lab Task 6 — Two-Player Reaction Game (Host)")

        # Set window size
        self.root.geometry("950x760")

        # Create serial manager
        self.link = SerialManager()

        # Temporary player names
        self.pending_p1 = ""
        self.pending_p2 = ""

        # Armed state
        self.armed = False

        # Current player names
        self.p1_name = ""
        self.p2_name = ""

        # Current game state
        self.state = self.IDLE

        # Player scores
        self.p1_wins = 0
        self.p2_wins = 0

        # Current round number
        self.round_num = 0

        # Session ID
        self.session_id = ""

        # Build interface
        self._build_ui()

        # Start serial checking
        self._pump_serial()

    # Build all interface tabs
    def _build_ui(self):

        # Create notebook widget
        self.nb = ttk.Notebook(self.root)

        # Fill window
        self.nb.pack(fill="both", expand=True)

        # Create tabs
        self.game_tab = ttk.Frame(self.nb)
        self.stats_tab = ttk.Frame(self.nb)

        # Add tabs
        self.nb.add(self.game_tab, text="Game")
        self.nb.add(self.stats_tab, text="Statistics")

        # Build game tab
        self._build_game_tab()

        # Build statistics tab
        self._build_stats_tab()
