from flask import Flask, Response
from picamera2 import Picamera2
import time
import io
from PIL import Image
import numpy as np
import paho.mqtt.client as mqtt
import time
 
app = Flask(__name__)
picam2 = Picamera2()
picam2.configure(picam2.create_video_configuration(main={"size": (640, 480)}))
picam2.start()
time.sleep(2)  # Give time to warm up

interval = 1200
last_sent_time = 0

mqtt_broker = "192.168.1.125"
mqtt_topic = "cam/picture"
mqtt_topic2 = "door/alarm"
client = mqtt.Client("camPi")
client.connect(mqtt_broker)
client.loop_start()

print("mqtt doen")

def take_and_publish_picture():
    frame = picam2.capture_array()
    pil_image = Image.fromarray(frame).convert('RGB')
    byte_io = io.BytesIO()
    pil_image.save(byte_io, 'JPEG')
    byte_io.seek(0)
    raw_image = byte_io.read()
    client.publish(mqtt_topic, raw_image)
 
def on_message(client, userdata, msg):
    if msg.topic == mqtt_topic2:
        take_and_publish_picture()
 
def setup_mqtt():
    client.on_message = on_message
    client.connect(mqtt_broker)
    client.subscribe(mqtt_topic2)
    client.loop_start()
 
setup_mqtt()
    

def generate_frames():
    global last_sent_time
    while True:
        current_time = time.time()
        # Capture an image from the camera
        frame = picam2.capture_array()
                 
        # Convert the image array to a PIL Image
        pil_image = Image.fromarray(frame)
 
        # Convert the image to RGB to ensure it's compatible with JPEG
        pil_image = pil_image.convert('RGB')
        
        # Save the image into a byte buffer
        byte_io = io.BytesIO()
        pil_image.save(byte_io, 'JPEG')
        byte_io.seek(0)
        
        raw_image = byte_io.read()
        
        if current_time - last_sent_time >= interval:
            client.publish(mqtt_topic, raw_image)
            last_sent_time = current_time
            
            
        byte_io.seek(0)
        # Yield the frame in the correct format for MJPEG streaming
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + byte_io.read() + b'\r\n')
        

 
@app.route('/')
def index():
    return '''
    <html>
        <head><title>Raspberry Pi Camera</title></head>
        <body>
            <h1>Live Camera Feed</h1>
            <img src="/video_feed">
        </body>
    </html>
    '''
 
@app.route('/video_feed')
def video_feed():
    return Response(generate_frames(),
                    mimetype='multipart/x-mixed-replace; boundary=frame')
 
if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)

