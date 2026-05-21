import tkinter as tk
from tkinter import ttk, messagebox
import sqlite3
import serial
import serial.tools.list_ports
import threading
import queue
from datetime import datetime

# SQLite database file name
DB_NAME = "rfid_tags.db"

# Serial communication speed
BAUD_RATE = 9600


# Database class
class RFIDDatabase:

    # Constructor
    def __init__(self, db_name):

        # Connect to SQLite database
        # If file does not exist, Python creates it
        self.conn = sqlite3.connect(db_name)

        # Create table if it does not exist
        self.create_table()

    # Function to create database table
    def create_table(self):

        # Cursor is used to execute SQL commands
        cursor = self.conn.cursor()

        # Create table only if it does not already exist
        cursor.execute("""  
            CREATE TABLE IF NOT EXISTS tags (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                uid TEXT UNIQUE NOT NULL,
                scan_count INTEGER NOT NULL DEFAULT 1,
                first_seen TEXT NOT NULL,
                last_seen TEXT NOT NULL
            )
        """)

        # id          -> automatic unique number
        # uid         -> RFID card UID
        # scan_count  -> number of scans
        # first_seen  -> first scan time
        # last_seen   -> latest scan time

        # Save changes
        self.conn.commit()

    # Function to add or update RFID tag
    def add_or_update_tag(self, uid):

        cursor = self.conn.cursor()

        # Get current date and time
        now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        # Check if UID already exists
        cursor.execute(
            "SELECT id, scan_count FROM tags WHERE uid = ?",
            (uid,)
        )

        # Get one result row
        row = cursor.fetchone()

        # If tag already exists
        if row:

            # Read ID and current scan count
            tag_id, count = row

            # Increase scan count
            new_count = count + 1

            # Update database values
            cursor.execute("""
                UPDATE tags
                SET scan_count = ?, last_seen = ?
                WHERE uid = ?
            """, (new_count, now, uid))

            # Save changes
            self.conn.commit()

            # Return tag info
            return tag_id, new_count, False

        # If tag does not exist
        else:

            # Insert new tag into database
            cursor.execute("""
                INSERT INTO tags (uid, scan_count, first_seen, last_seen)
                VALUES (?, ?, ?, ?)
            """, (uid, 1, now, now))

            # scan_count = 1
            # first_seen = current time
            # last_seen = current time

            # Save changes
            self.conn.commit()

            # Get automatically created ID
            tag_id = cursor.lastrowid

            # Return tag info
            return tag_id, 1, True

    # Function to get all tags
    def get_all_tags(self):

        cursor = self.conn.cursor()

        # Select all records ordered by ID
        cursor.execute("""
            SELECT id, uid, scan_count, first_seen, last_seen
            FROM tags
            ORDER BY id ASC
        """)

        # Return all rows
        return cursor.fetchall()

    # Function to close database
    def close(self):
        self.conn.close()


# Main GUI application
class RFIDApp:

    # Constructor
    def __init__(self, root):

        # Main window
        self.root = root

        # Window title
        self.root.title("RFID Security System Database")

        # Window size
        self.root.geometry("900x550")

        # Create database object
        self.db = RFIDDatabase(DB_NAME)

        # Serial variables
        self.serial_port = None
        self.running = False
        self.serial_thread = None

        # Queue for serial data
        self.data_queue = queue.Queue()

        # Build GUI
        self.create_widgets()

        # Load COM ports
        self.refresh_ports()

        # Load database table
        self.refresh_table()

        # Start checking serial data every 100 ms
        self.root.after(100, self.process_serial_data)

    # Function to create GUI widgets
    def create_widgets(self):

        # ---------------- TOP FRAME ----------------
        top_frame = ttk.LabelFrame(
            self.root,
            text="Serial Connection"
        )

        top_frame.pack(fill="x", padx=10, pady=10)

        # Port label
        ttk.Label(top_frame, text="Port:").pack(
            side="left",
            padx=5
        )

        # COM port dropdown menu
        self.port_box = ttk.Combobox(
            top_frame,
            width=20,
            state="readonly"
        )

        self.port_box.pack(side="left", padx=5)

        # Refresh ports button
        self.refresh_button = ttk.Button(
            top_frame,
            text="Refresh Ports",
            command=self.refresh_ports
        )

        self.refresh_button.pack(side="left", padx=5)

        # Connect button
        self.connect_button = ttk.Button(
            top_frame,
            text="Connect",
            command=self.connect_serial
        )

        self.connect_button.pack(side="left", padx=5)

        # Disconnect button
        self.disconnect_button = ttk.Button(
            top_frame,
            text="Disconnect",
            command=self.disconnect_serial,
            state="disabled"
        )

        self.disconnect_button.pack(side="left", padx=5)

        # Connection status text
        self.status_label = ttk.Label(
            top_frame,
            text="Status: Disconnected"
        )

        self.status_label.pack(side="left", padx=20)

        # ---------------- TABLE FRAME ----------------
        table_frame = ttk.LabelFrame(
            self.root,
            text="RFID Tag Database"
        )

        table_frame.pack(
            fill="both",
            expand=True,
            padx=10,
            pady=10
        )

        # Table column names
        columns = (
            "id",
            "uid",
            "scan_count",
            "first_seen",
            "last_seen"
        )

        # Create table widget
        self.tree = ttk.Treeview(
            table_frame,
            columns=columns,
            show="headings",
            height=12
        )

        # Set table headings
        self.tree.heading("id", text="Unique ID")
        self.tree.heading("uid", text="RFID UID")
        self.tree.heading("scan_count", text="Scan Count")
        self.tree.heading("first_seen", text="First Seen")
        self.tree.heading("last_seen", text="Last Seen")

        # Set column sizes
        self.tree.column("id", width=80, anchor="center")
        self.tree.column("uid", width=180, anchor="center")
        self.tree.column("scan_count", width=100, anchor="center")
        self.tree.column("first_seen", width=180, anchor="center")
        self.tree.column("last_seen", width=180, anchor="center")

        # Vertical scrollbar
        scrollbar = ttk.Scrollbar(
            table_frame,
            orient="vertical",
            command=self.tree.yview
        )

        # Connect scrollbar to table
        self.tree.configure(
            yscrollcommand=scrollbar.set
        )

        # Place table
        self.tree.pack(
            side="left",
            fill="both",
            expand=True
        )

        # Place scrollbar
        scrollbar.pack(
            side="right",
            fill="y"
        )

        # ---------------- BUTTON FRAME ----------------
        button_frame = ttk.Frame(self.root)

        button_frame.pack(
            fill="x",
            padx=10,
            pady=5
        )

        # Refresh table button
        self.refresh_table_button = ttk.Button(
            button_frame,
            text="Refresh Table",
            command=self.refresh_table
        )

        self.refresh_table_button.pack(
            side="left",
            padx=5
        )

        # ---------------- LOG FRAME ----------------
        log_frame = ttk.LabelFrame(
            self.root,
            text="Arduino Serial Log"
        )

        log_frame.pack(
            fill="both",
            expand=True,
            padx=10,
            pady=10
        )

        # Text box for serial messages
        self.log_text = tk.Text(
            log_frame,
            height=8
        )

        self.log_text.pack(
            fill="both",
            expand=True
        )
