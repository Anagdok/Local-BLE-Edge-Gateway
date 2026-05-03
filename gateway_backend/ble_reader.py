import asyncio
import sqlite3
from bleak import BleakClient, BleakScanner

DB_PATH = '/home/rock/ble_gateway/sensors.db'

# Zmienna przechowująca stan
current_data = {"temp": 0.0, "hum": 0.0, "light": 0, "noise": 0}

KLIMAT_NAME = "ESP32_Salon_Klimat"
KLIMAT_CHAR = "beb5483e-36e1-4688-b7f5-ea07361b26a8"

AUDIO_NAME = "ESP32_Salon_Audio"
NOISE_CHAR = "22222222-36e1-4688-b7f5-ea07361b26a8"
LED_CHAR = "33333333-344c-4be3-ab3f-189f80dd7518"

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

def get_pending_command():
    try:
        conn = sqlite3.connect(DB_PATH)
        c = conn.cursor()
        c.execute("SELECT id, action FROM commands ORDER BY id ASC LIMIT 1")
        cmd = c.fetchone()
        if cmd:
            c.execute("DELETE FROM commands WHERE id = ?", (cmd[0],))
            conn.commit()
        conn.close()
        return cmd[1] if cmd else None
    except Exception as e:
        print(f"Blad bazy komend: {e}", flush=True)
        return None

async def db_writer_task():
    while True:
        try:
            conn = sqlite3.connect(DB_PATH)
            c = conn.cursor()
            c.execute("INSERT INTO data (temperature, humidity, light, noise) VALUES (?, ?, ?, ?)", 
                      (current_data["temp"], current_data["hum"], current_data["light"], current_data["noise"]))
            conn.commit()
            conn.close()
        except Exception as e:
            print(f"Blad zapisu do bazy: {e}", flush=True)
        await asyncio.sleep(5)

async def handle_klimat(device):
    async with BleakClient(device) as client:
        print("Polaczono z Klimatem!", flush=True)
        while True:
            try:
                val = await client.read_gatt_char(KLIMAT_CHAR)
                parts = val.decode('utf-8').split(',')
                if len(parts) == 3:
                    current_data["temp"] = float(parts[0])
                    current_data["hum"] = float(parts[1])
                    current_data["light"] = int(parts[2])
            except Exception as e:
                print(f"Blad odczytu Klimatu: {e}", flush=True)
            await asyncio.sleep(4)

async def handle_audio(device):
    async with BleakClient(device) as client:
        print("Polaczono z Audio/LED!", flush=True)
        while True:
            try:
                # Odczyt hałasu
                val = await client.read_gatt_char(NOISE_CHAR)
                current_data["noise"] = int(val.decode('utf-8'))

                # Sprawdzenie i wysłanie komendy LED
                cmd = get_pending_command()
                if cmd:
                    print(f"Wysylam komende do ESP: {cmd}", flush=True)
                    await client.write_gatt_char(LED_CHAR, bytearray(cmd, "utf-8"))
                    print("Zakonczono wysylanie BLE!", flush=True)
            except Exception as e:
                print(f"Blad lacznosci z Audio: {e}", flush=True)
            await asyncio.sleep(1)

async def main():
    print("Szukanie urzadzen ESP32...", flush=True)
    scanner = BleakScanner()
    devices = await scanner.discover(timeout=5.0)
    
    klimat_dev = next((d for d in devices if d.name == KLIMAT_NAME), None)
    audio_dev = next((d for d in devices if d.name == AUDIO_NAME), None)

    tasks = [asyncio.create_task(db_writer_task())]
    
    if klimat_dev:
        tasks.append(asyncio.create_task(handle_klimat(klimat_dev)))
    else:
        print("UWAGA: Nie znaleziono ESP32_Salon_Klimat", flush=True)

    if audio_dev:
        tasks.append(asyncio.create_task(handle_audio(audio_dev)))
    else:
        print("UWAGA: Nie znaleziono ESP32_Salon_Audio", flush=True)

    await asyncio.gather(*tasks)

if __name__ == "__main__":
    setup_db()
    asyncio.run(main())
