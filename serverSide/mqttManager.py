import paho.mqtt.client as mqtt
import time
class mqttManager:
    def __init__(self,broker,topic):
        self.BROKER = broker
        self.TOPIC = topic
        self.client =  mqtt.Client()
        self.client.connect(self.BROKER,1883,60)
    
    def sendMsg(self,msg):
        self.client.publish(self.TOPIC,msg)
        time.sleep(1)
    