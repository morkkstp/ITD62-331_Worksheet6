import paho.mqtt.client as mqtt
import json
import requests

host = "192.168.1.112"
port = 1883

def on_connect(client, userdata, flags, rc):
    print("MQTT Connected.")
    client.subscribe("dht11value")

def on_message(client, userdata, msg):
    payload = msg.payload.decode("utf-8", "strict")
    data = json.loads(payload)
    print(data)

    # Send the data to the JSON server
    response = requests.post(f"http://{host}:3000/dht11value", json=data)
    if response.status_code == 201:
        print("Send Data to the JSON-Server Success.")
    else:
        print(f"Failed to Send Data to the JSON-Server. Status code: {response.status_code}")


client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect(host, port)
client.loop_forever()
