import tkinter as tk
from tkinter import ttk, messagebox
import sqlite3
import serial
import serial.tools.list_ports
import threading
import queue
from datetime import datetime

# SQLite database file 
DB_NAME = "rfid_tags.db"
BAUD_RATE = 9600


class RFIDDatabase:
    def __init__(self, db_name):
        self.conn = sqlite3.connect(db_name) # if the file does not exist, Python creates it
        self.create_table()                  # create the table if it does not already exist

    def create_table(self): # creates the database table
        cursor = self.conn.cursor() # cursor is used to run SQL commands

        # creates a table named tags only if it does not already exist
        cursor.execute("""  
            CREATE TABLE IF NOT EXISTS tags (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                uid TEXT UNIQUE NOT NULL,
                scan_count INTEGER NOT NULL DEFAULT 1,
                first_seen TEXT NOT NULL,
                last_seen TEXT NOT NULL
            )
        """)

        # creates unique ID for every tag automatically
        # stores RFID UID -> unique
        # stores how many times the tag was scanned
        # stores first scan time
        # stores latest scan time    

        self.conn.commit() # save changes

    # this function receives RFID UID from Arduino
    def add_or_update_tag(self, uid):
        cursor = self.conn.cursor()
        now = datetime.now().strftime("%Y-%m-%d %H:%M:%S") # gets current date and time

        # check if UID already exsitsts
        cursor.execute("SELECT id, scan_count FROM tags WHERE uid = ?", (uid,))
        row = cursor.fetchone() # gets one result

        if row:
            tag_id, count = row
            new_count = count + 1

            cursor.execute("""
                UPDATE tags
                SET scan_count = ?, last_seen = ?
                WHERE uid = ?
            """, (new_count, now, uid))

            # updates existing tag:
            # scan_count increases
            # last_seen becomes current time

            self.conn.commit()
            return tag_id, new_count, False # false = not new tag

        else:
            cursor.execute("""
                INSERT INTO tags (uid, scan_count, first_seen, last_seen)
                VALUES (?, ?, ?, ?)
            """, (uid, 1, now, now))

            # for a new tag:
            # scan_count = 1
            # first_seen = now
            # last_seen = now

            self.conn.commit()              # saves new row
            tag_id = cursor.lastrowid       # gets the automatically created unique ID
            return tag_id, 1, True

    def get_all_tags(self):
        cursor = self.conn.cursor()

        cursor.execute("""
            SELECT id, uid, scan_count, first_seen, last_seen
            FROM tags
            ORDER BY id ASC
        """) # select all recods and orders by ID

        return cursor.fetchall()

    def close(self):
        self.conn.close()


class RFIDApp:
    def __init__(self, root):
        self.root = root
        self.root.title("RFID Security System Database")
        self.root.geometry("900x550")

        self.db = RFIDDatabase(DB_NAME)

        self.serial_port = None
        self.running = False
        self.serial_thread = None
        self.data_queue = queue.Queue()

        self.create_widgets()
        self.refresh_ports()
        self.refresh_table()

        self.root.after(100, self.process_serial_data)

    def create_widgets(self):
        # top frame
        top_frame = ttk.LabelFrame(self.root, text="Serial Connection")
        top_frame.pack(fill="x", padx=10, pady=10)

        ttk.Label(top_frame, text="Port:").pack(side="left", padx=5)

        self.port_box = ttk.Combobox(top_frame, width=20, state="readonly")
        self.port_box.pack(side="left", padx=5)

        self.refresh_button = ttk.Button(
            top_frame,
            text="Refresh Ports",
            command=self.refresh_ports
        )
        self.refresh_button.pack(side="left", padx=5)

        self.connect_button = ttk.Button(
            top_frame,
            text="Connect",
            command=self.connect_serial
        )
        self.connect_button.pack(side="left", padx=5)

        self.disconnect_button = ttk.Button(
            top_frame,
            text="Disconnect",
            command=self.disconnect_serial,
            state="disabled"
        )
        self.disconnect_button.pack(side="left", padx=5)

        self.status_label = ttk.Label(top_frame, text="Status: Disconnected")
        self.status_label.pack(side="left", padx=20)

        # table frame
        table_frame = ttk.LabelFrame(self.root, text="RFID Tag Database")
        table_frame.pack(fill="both", expand=True, padx=10, pady=10)

        # defines table columns
        columns = ("id", "uid", "scan_count", "first_seen", "last_seen")

        # creates table widget
        self.tree = ttk.Treeview(
            table_frame,
            columns=columns,
            show="headings",
            height=12
        )

        # sets column titles
        self.tree.heading("id", text="Unique ID")
        self.tree.heading("uid", text="RFID UID")
        self.tree.heading("scan_count", text="Scan Count")
        self.tree.heading("first_seen", text="First Seen")
        self.tree.heading("last_seen", text="Last Seen")

        self.tree.column("id", width=80, anchor="center")
        self.tree.column("uid", width=180, anchor="center")
        self.tree.column("scan_count", width=100, anchor="center")
        self.tree.column("first_seen", width=180, anchor="center")
        self.tree.column("last_seen", width=180, anchor="center")

        scrollbar = ttk.Scrollbar(
            table_frame,
            orient="vertical",
            command=self.tree.yview
        )

        self.tree.configure(yscrollcommand=scrollbar.set)

        self.tree.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        # refresh table button frame
        button_frame = ttk.Frame(self.root)
        button_frame.pack(fill="x", padx=10, pady=5)

        self.refresh_table_button = ttk.Button(
            button_frame,
            text="Refresh Table",
            command=self.refresh_table
        )
        self.refresh_table_button.pack(side="left", padx=5)

        # log frame
        log_frame = ttk.LabelFrame(self.root, text="Arduino Serial Log")
        log_frame.pack(fill="both", expand=True, padx=10, pady=10)

        self.log_text = tk.Text(log_frame, height=8)
        self.log_text.pack(fill="both", expand=True)

    def refresh_ports(self):
        ports = serial.tools.list_ports.comports()
        port_names = [port.device for port in ports]

        self.port_box["values"] = port_names

        if port_names:
            self.port_box.current(0)

    def connect_serial(self):
        selected_port = self.port_box.get()

        if not selected_port:
            messagebox.showerror("Error", "Please select a COM port.")
            return

        try:
            self.serial_port = serial.Serial(selected_port, BAUD_RATE, timeout=1)
            self.running = True

            self.serial_thread = threading.Thread(
                target=self.read_serial,
                daemon=True
            )
            self.serial_thread.start()

            self.status_label.config(text=f"Status: Connected to {selected_port}")
            self.connect_button.config(state="disabled")
            self.disconnect_button.config(state="normal")

            self.add_log(f"Connected to {selected_port}")

        except Exception as e:
            messagebox.showerror("Connection Error", str(e))

    def disconnect_serial(self):
        self.running = False

        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()

        self.status_label.config(text="Status: Disconnected")
        self.connect_button.config(state="normal")
        self.disconnect_button.config(state="disabled")

        self.add_log("Disconnected")

    def read_serial(self):
        while self.running:
            try:
                if self.serial_port and self.serial_port.in_waiting > 0:
                    line = self.serial_port.readline().decode("utf-8", errors="ignore").strip()
                    # reads one line from Arduino

                    if line:
                        self.data_queue.put(line)

            except Exception as e:
                self.data_queue.put(f"ERROR,{e}")
                break

    def process_serial_data(self):
        while not self.data_queue.empty():
            line = self.data_queue.get()

            self.add_log(line)

            if line.startswith("TAG,"):
                uid = line.split(",", 1)[1].strip()

                if uid:
                    # saves or updates tag in database
                    tag_id, count, is_new = self.db.add_or_update_tag(uid)

                    if is_new:
                        self.add_log(f"New tag saved: ID={tag_id}, UID={uid}")
                    else:
                        self.add_log(f"Existing tag updated: ID={tag_id}, Count={count}")

                    self.refresh_table()

        self.root.after(100, self.process_serial_data)
        # the GUI continuously checks for new Serial data without freezing

    def refresh_table(self):
        # deletes old visible rows from the GUI table (only GUI rows, not database rows)
        for row in self.tree.get_children():
            self.tree.delete(row)

        # gets all records from SQLite database
        records = self.db.get_all_tags()

        # inserts each database row into GUI table
        for record in records:
            self.tree.insert("", "end", values=record)

    def add_log(self, message):
        self.log_text.insert("end", message + "\n")
        self.log_text.see("end")

    def on_close(self):
        self.disconnect_serial()
        self.db.close()
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    app = RFIDApp(root)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()

# id          -> automatic unique ID
# uid         -> RFID card UID
# scan_count  -> how many times card was scanned
# first_seen  -> first scan time
# last_seen   -> latest scan time
