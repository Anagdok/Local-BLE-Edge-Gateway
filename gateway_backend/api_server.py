from flask import Flask, jsonify, request
from flask_cors import CORS
import sqlite3
import os

app = Flask(__name__)
CORS(app)

# TWARDA ŚCIEŻKA - zapobiega tworzeniu "duchów" baz danych
DB_PATH = '/home/rock/ble_gateway/sensors.db'

def setup_db():
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute('''CREATE TABLE IF NOT EXISTS data
                 (timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, 
                  temperature REAL, humidity REAL, light INTEGER, noise INTEGER)''')
    c.execute('''CREATE TABLE IF NOT EXISTS commands
                 (id INTEGER PRIMARY KEY AUTOINCREMENT, action TEXT)''')
    conn.commit()
    conn.close()

@app.route('/api/data', methods=['GET'])
def get_data():
    try:
        conn = sqlite3.connect(DB_PATH)
        c = conn.cursor()
        c.execute("SELECT timestamp, temperature, humidity, light, noise FROM data ORDER BY timestamp DESC LIMIT 1")
        row = c.fetchone()
        conn.close()

        if row:
            return jsonify({
                "status": "success",
                "timestamp": row[0],
                "temperature": row[1] if row[1] is not None else 0.0,
                "humidity": row[2] if row[2] is not None else 0.0,
                "light": row[3] if row[3] is not None else 0,
                "noise": row[4] if row[4] is not None else 0
            })
        return jsonify({"status": "error", "message": "Brak danych"}), 404
    except Exception as e:
        print(f"Blad API: {e}", flush=True)
        return jsonify({"status": "error", "message": str(e)}), 500

@app.route('/api/history', methods=['GET'])
def get_history():
    try:
        conn = sqlite3.connect(DB_PATH)
        c = conn.cursor()
        c.execute("SELECT timestamp, temperature, humidity, light, noise FROM data ORDER BY timestamp DESC LIMIT 10")
        rows = c.fetchall()
        conn.close()

        if rows:
            rows.reverse()
            return jsonify({
                "status": "success",
                "timestamps": [r[0].split(" ")[1] for r in rows],
                "temperature": [r[1] if r[1] is not None else 0 for r in rows],
                "humidity": [r[2] if r[2] is not None else 0 for r in rows],
                "light": [r[3] if r[3] is not None else 0 for r in rows],
                "noise": [r[4] if r[4] is not None else 0 for r in rows]
            })
        return jsonify({"status": "error"}), 404
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 500

@app.route('/api/led', methods=['POST'])
def control_led():
    try:
        data = request.json
        action = data.get('state')
        
        if action in ["ON", "OFF"]:
            conn = sqlite3.connect(DB_PATH)
            c = conn.cursor()
            c.execute("INSERT INTO commands (action) VALUES (?)", (action,))
            conn.commit()
            conn.close()
            print(f"Zapisano komende LED do bazy: {action}", flush=True)
            return jsonify({"status": "success", "action_queued": action})
        
        return jsonify({"status": "error"}), 400
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 500

if __name__ == '__main__':
    setup_db()
    app.run(host='0.0.0.0', port=9001)
