from flask import Flask, request, jsonify, render_template, url_for, redirect
from mysql.connector import pooling
from datetime import datetime
import threading
import requests as req

app = Flask(__name__)

# ── Database connection pool ──────────────────────────────────────────────────
db_pool = pooling.MySQLConnectionPool(
    pool_name="smartpool",
    pool_size=10,
    host="localhost",
    user="root",
    port=3306,
    password="B@nele2002",
    database="SmartIrrigationDB",
    auth_plugin="mysql_native_password"
)

# ── In-memory cache (latest value per sensor) ─────────────────────────────────
sensor_data = {}

# ── Irrigation state ──────────────────────────────────────────────────────────
irrigation_state = {
    "irrigating": False,
    "reason":     "Waiting for sensor data",
    "updated_at": None,
}

# ── Active session ID ─────────────────────────────────────────────────────────
current_session_id = None

# ── Role → Flask route name ───────────────────────────────────────────────────
ROLE_ROUTES = {
    "2": "Soil",
    "3": "TempHum",
    "4": "waterlevel",
    "5": "Motion",
    "6": "Rainfall",
}


# ═══════════════════════════════════════════════════════════════════════════════
#  IRRIGATION DECISION ENGINE
#  Called every time any sensor posts new data.
#  Returns (should_irrigate: bool, reason: str)
# ═══════════════════════════════════════════════════════════════════════════════

def evaluate_irrigation():
    soil      = sensor_data.get("soil",      {}).get("distance")
    water_pct = sensor_data.get("waterlevel",{}).get("distance")
    rain_stat = sensor_data.get("rainfall",  {}).get("status")
    temp      = sensor_data.get("temp",      {}).get("distance")
    motion    = sensor_data.get("pir_motion",{}).get("distance")

    # ── 1. Tank too low ───────────────────────────────────────────────────────
    if water_pct is not None and float(water_pct) < 20:
        return False, "Tank below 20% — refill required"

    # ── 2. Currently raining ──────────────────────────────────────────────────
    if rain_stat == "Raining":
        return False, "It is raining — natural irrigation active"

    # ── 3. Soil already moist ─────────────────────────────────────────────────
    if soil is not None and float(soil) > 60:
        return False, f"Soil moisture is {float(soil):.1f}% — no irrigation needed"

    # ── 4. Soil too dry → irrigation needed ──────────────────────────────────
    if soil is not None and float(soil) < 40:

        # ── 4a. Motion safety check ───────────────────────────────────────────
        if motion == "Detected":
            return False, "Motion detected in field — irrigation paused for safety"

        # ── 4b. High temperature urgency ─────────────────────────────────────
        if temp is not None and float(temp) > 35:
            return True, f"Soil dry ({float(soil):.1f}%) + high temp ({float(temp):.1f}°C) — irrigating at full rate"

        # ── 4c. Normal irrigation ─────────────────────────────────────────────
        return True, f"Soil moisture low ({float(soil):.1f}%) — irrigating"

    # ── 5. Soil in safe zone (40–60%) — monitor only ─────────────────────────
    if soil is not None:
        return False, f"Soil moisture is {float(soil):.1f}% — within safe range, monitoring"

    # ── 6. Not enough data yet ────────────────────────────────────────────────
    return False, "Waiting for soil moisture reading"


# ═══════════════════════════════════════════════════════════════════════════════
#  COMMAND SENDER  –  tells the DHT22 ESP32 to turn the green LED on or off
#  Runs in a background thread so it never blocks Flask from responding.
# ═══════════════════════════════════════════════════════════════════════════════

def send_irrigation_command(irrigate: bool):
    try:
        payload = {"irrigate": irrigate}
        response = req.post(DHT22_COMMAND_URL, json=payload, timeout=3)
        print(f"[IRRIGATE CMD] sent irrigate={irrigate} → HTTP {response.status_code}")
    except Exception as e:
        print(f"[IRRIGATE CMD ERROR] Could not reach DHT22 ESP32: {e}")


def trigger_irrigation_decision():
    """Evaluate, update state, and send command to ESP32 if state changed."""
    global irrigation_state

    should_irrigate, reason = evaluate_irrigation()
    now = datetime.now().strftime("%H:%M:%S")

    # Only send a command to the ESP32 when state actually changes (avoids spam)
    state_changed = should_irrigate != irrigation_state["irrigating"]

    irrigation_state["irrigating"]  = should_irrigate
    irrigation_state["reason"]      = reason
    irrigation_state["updated_at"]  = now

    print(f"[DECISION] irrigate={should_irrigate} | reason={reason}")

    if state_changed:
        t = threading.Thread(target=send_irrigation_command, args=(should_irrigate,), daemon=True)
        t.start()


# ── DB helpers ────────────────────────────────────────────────────────────────
def get_db():
    return db_pool.get_connection()

def create_session():
    db = cursor = None
    try:
        db = get_db()
        cursor = db.cursor()
        cursor.execute("INSERT INTO Sessions () VALUES ()")
        db.commit()
        sid = cursor.lastrowid
        print(f"[SESSION] New session created: SessionID={sid}")
        return sid
    except Exception as e:
        print(f"[SESSION ERROR] {e}")
        return None
    finally:
        if cursor: cursor.close()
        if db:     db.close()

with app.app_context():
    current_session_id = create_session()


# ═══════════════════════════════════════════════════════════════════════════════
#  PAGE ROUTES
# ═══════════════════════════════════════════════════════════════════════════════

@app.route('/')
def home():
    return render_template("index.html")

@app.route('/farmerlogin', methods=["POST", "GET"])
def farmerlogin():
    if request.method == "POST":
        username = request.form["username"]
        password = request.form["password"]
        role     = request.form["role"]
        db = get_db(); cursor = db.cursor()
        cursor.execute(
            "SELECT * FROM Users WHERE Username=%s AND Passwords=%s AND UserRole=%s",
            (username, password, role))
        farmer = cursor.fetchone(); cursor.close(); db.close()
        if farmer:
            route = ROLE_ROUTES.get(role)
            if route: return redirect(url_for(route))
            return render_template("farmerlogin.html", error="Unknown role")
        return render_template("farmerlogin.html", error="Invalid credentials")
    return render_template("farmerlogin.html")

@app.route('/adminlogin', methods=["POST", "GET"])
def adminlogin():
    if request.method == "POST":
        username = request.form["username"]
        password = request.form["password"]
        db = get_db(); cursor = db.cursor()
        cursor.execute(
            "SELECT * FROM Users WHERE Username=%s AND Passwords=%s AND UserRole=%s",
            (username, password, 1))
        admin = cursor.fetchone(); cursor.close(); db.close()
        if admin: return redirect(url_for("admindashboard"))
        return render_template("adminlogin.html", error="Invalid credentials")
    return render_template("adminlogin.html")

@app.route('/admindashboard')
def admindashboard():
    return render_template("admindashboard.html")

@app.route('/waterlevel')
def waterlevel():
    return render_template("waterlevel.html")

@app.route('/TempHum')
def TempHum():
    return render_template("TempHum.html")

@app.route('/Soil')
def Soil():
    return render_template("Soil.html")

@app.route('/Rainfall')
def Rainfall():
    return render_template("Rainfall.html")

@app.route('/Motion')
def Motion():
    return render_template("Motion.html")


# ═══════════════════════════════════════════════════════════════════════════════
#  SENSOR DATA RECEIVER  –  POST /data
# ═══════════════════════════════════════════════════════════════════════════════

@app.route('/data', methods=['POST'])
def receive_data():
    global current_session_id

    data = request.get_json()
    if not data or 'id' not in data:
        return jsonify({"status": "error", "message": "Invalid data"}), 400

    sensor_id = data['id']
    sensor_data[sensor_id] = data
    print(f"[RECEIVED] sensor_id={sensor_id} | data={data}")

    if current_session_id is None:
        current_session_id = create_session()
        if current_session_id is None:
            return jsonify({"status": "error", "message": "No session"}), 500

    sid = current_session_id
    db = cursor = None
    try:
        db = get_db(); cursor = db.cursor()

        if sensor_id == "soil":
            value = data.get("distance")
            if value is not None:
                cursor.execute(
                    "INSERT INTO SoilMoisture (SessionID, Moisture) VALUES (%s, %s)",
                    (sid, float(value)))
                db.commit()
                print(f"[DB] SoilMoisture saved: {value}")

        elif sensor_id == "rainfall":
            raw = data.get("value"); status = data.get("status")
            if raw is not None and status is not None:
                cursor.execute(
                    "INSERT INTO Rainfall (SessionID, RawValue, RainStatus) VALUES (%s, %s, %s)",
                    (sid, int(raw), str(status)))
                db.commit()
                print(f"[DB] Rainfall saved: raw={raw}, status={status}")

        elif sensor_id == "temp":
            value = data.get("distance")
            if value is not None:
                cursor.execute(
                    "INSERT INTO Temperature (SessionID, TempValue) VALUES (%s, %s)",
                    (sid, float(value)))
                db.commit()
                print(f"[DB] Temperature saved: {value}")

        elif sensor_id == "humidity":
            value = data.get("distance")
            if value is not None:
                cursor.execute(
                    "INSERT INTO Humidity (SessionID, HumValue) VALUES (%s, %s)",
                    (sid, float(value)))
                db.commit()
                print(f"[DB] Humidity saved: {value}")

        elif sensor_id == "waterlevel":
            value = data.get("distance")
            if value is not None:
                cursor.execute(
                    "INSERT INTO WaterLevel (SessionID, LevelPct) VALUES (%s, %s)",
                    (sid, float(value)))
                db.commit()
                print(f"[DB] WaterLevel saved: {value}")

        elif sensor_id in ("pir_motion", "motion"):
            value = data.get("distance") or data.get("status")
            if value is not None:
                cursor.execute(
                    "INSERT INTO Motion (SessionID, MotionStatus) VALUES (%s, %s)",
                    (sid, str(value)))
                db.commit()
                print(f"[DB] Motion saved: {value}")

        else:
            print(f"[WARN] Unknown sensor_id: {sensor_id}")

    except Exception as e:
        print(f"[DB ERROR] {e}")
        if db:
            try: db.rollback()
            except: pass
    finally:
        if cursor: cursor.close()
        if db:     db.close()

    # ── Re-evaluate irrigation decision after every sensor update ─────────────
    trigger_irrigation_decision()

    return jsonify({"status": "ok"}), 200


# ═══════════════════════════════════════════════════════════════════════════════
#  LIVE SENSOR READ  –  GET /get_sensor/<sensor_id>
# ═══════════════════════════════════════════════════════════════════════════════

@app.route('/get_sensor/<sensor_id>', methods=['GET'])
def get_sensor(sensor_id):
    if sensor_id in sensor_data:
        return jsonify(sensor_data[sensor_id])
    return jsonify({"id": sensor_id, "distance": None, "value": None, "status": None})


# ═══════════════════════════════════════════════════════════════════════════════
#  IRRIGATION STATUS  –  GET /irrigation_status
#  Dashboard polls this to show the current decision + reason.
# ═══════════════════════════════════════════════════════════════════════════════

@app.route('/irrigation_status', methods=['GET'])
def irrigation_status():
    return jsonify(irrigation_state)


# ═══════════════════════════════════════════════════════════════════════════════
#  HISTORY API  –  GET /history/<sensor_id>
# ═══════════════════════════════════════════════════════════════════════════════

@app.route('/history/<sensor_id>', methods=['GET'])
def history(sensor_id):
    TABLE_MAP = {
        "soil":       ("SoilMoisture", "RecordedAt", "Moisture"),
        "temp":       ("Temperature",  "RecordedAt", "TempValue"),
        "humidity":   ("Humidity",     "RecordedAt", "HumValue"),
        "waterlevel": ("WaterLevel",   "RecordedAt", "LevelPct"),
        "rainfall":   ("Rainfall",     "RecordedAt", "RawValue"),
        "pir_motion": ("Motion",       "RecordedAt", "MotionStatus"),
    }
    if sensor_id not in TABLE_MAP:
        return jsonify({"error": "Unknown sensor"}), 400

    table, ts_col, val_col = TABLE_MAP[sensor_id]
    db = cursor = None; rows = []
    try:
        db = get_db(); cursor = db.cursor(dictionary=True)
        cursor.execute(
            f"SELECT t.{ts_col} AS ts, t.{val_col} AS val, t.SessionID "
            f"FROM {table} t "
            f"INNER JOIN Sessions s ON t.SessionID = s.SessionID "
            f"ORDER BY t.{ts_col} DESC LIMIT 20")
        rows = cursor.fetchall()
        for r in rows:
            if isinstance(r.get("ts"), datetime):
                r["ts"] = r["ts"].strftime("%H:%M:%S")
    except Exception as e:
        print(f"[HISTORY ERROR] {e}")
    finally:
        if cursor: cursor.close()
        if db:     db.close()

    return jsonify(list(reversed(rows)))


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)