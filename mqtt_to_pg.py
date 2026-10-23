import json
import psycopg2
import paho.mqtt.client as mqtt
import subprocess
import time

def is_postgres_running():
    try:
        subprocess.check_output(["pg_isready"])
        return True
    except subprocess.CalledProcessError:
        return False

while not is_postgres_running():
    print("Waiting for PostgreSQL to start...")
    time.sleep(5)

# PostgreSQL connection
conn = psycopg2.connect(
    dbname="sensordata",
    user="postgres",
    password="postgres",
    host="localhost"
)
cur = conn.cursor()

# MQTT callback
def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
        cur.execute("""
            INSERT INTO sensor_data (sensor_id, event_time, temperature, humidity, co2)
            VALUES (%s, %s, %s, %s, %s)
        """, (
            data.get("sensorId"),
            data.get("eventTime"),
            data.get("temperature"),
            data.get("humidity"),
            data.get("co2")
        ))
        conn.commit()
        print("Inserted:", data)
    except Exception as e:
        print("Error:", e)

# MQTT Setup
client = mqtt.Client()
client.username_pw_set("berlin/loco", "loco")
client.connect("localhost", 1883, 60)
client.subscribe("sensors/esp32-01/data")
client.on_message = on_message

print("Listening for sensor data...")
client.loop_forever()

