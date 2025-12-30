import undetected_chromedriver as uc
from selenium.webdriver.common.by import By
import threading
import time
from mqttManager import mqttManager
import re
import os
import random

class Bot:
    def __init__(self, username, password):
        self.username = username
        self.password = password
        self.running = True

        self.PAGES = {
            "login": "https://eclass.ukdw.ac.id/e-class/id/",
            "main": "https://eclass.ukdw.ac.id/e-class/id/kelas/index",
            "presensi": "https://eclass.ukdw.ac.id/e-class/id/kelas/presensi/"
        }

        self.driver = uc.Chrome()
        self.driver.get(self.PAGES["login"])

        self.thread = threading.Thread(target=self.run, daemon=True)
        
        self.mqqtManage = mqttManager("broker.emqx.io",f"botPresensi/{self.username}")
        
        

    def start(self):
        self.thread.start()

    def stop(self):
        self.running = False
        self.driver.quit()

    def getSplittedClassName(self, text):
        classId = re.search(r"\[(.*?)\]", text)
        className = re.search(r"\]\s*(.*?)\s*\(\d+\s*SKS\)", text)
        return {"id":classId.group(1),"name":className.group(1)}

    def checkLogin(self):
        self.driver.get(self.PAGES["login"])
        self.driver.find_element(By.NAME, "id").send_keys(self.username)
        self.driver.find_element(By.NAME, "password").send_keys(self.password)
        self.driver.find_element(By.CSS_SELECTOR, "input[type='submit']").click()
        time.sleep(1)
        return self.driver.current_url != self.PAGES["login"]

    def run(self):
        print(f"Bot started: {self.username}")

        while self.running:
            try:
                self.loop()
                time.sleep(1)
            except Exception as e:
                print("Bot error:", e)
                break

    def loop(self):
        # Always go to main page
        if self.driver.current_url != self.PAGES["main"]:
            self.driver.get(self.PAGES["main"])

        time.sleep(1)  # allow page to load

        # STEP 1: COLLECT URLs (NO NAVIGATION HERE)
        daftarMatkul = {}
        boxes = self.driver.find_elements(By.CLASS_NAME, "kelas_box")
        
        for box in boxes:
            h2 = box.find_element(By.TAG_NAME, "h2")
            matkul = self.getSplittedClassName(h2.text)
            if matkul["id"]:
                daftarMatkul.update({matkul["id"]:matkul["name"]})
                
        # STEP 2: NAVIGATE USING STORED STRINGS
        for matkul in daftarMatkul:
            self.driver.get(self.PAGES["presensi"]+matkul)
            # second_td = self.driver.find_element(By.CSS_SELECTOR, "table tr:nth-child(2) td:nth-child(2)")
            # print(second_td.text)
            print(f"checking: [{matkul}]{daftarMatkul[matkul]}")
            matkulSplit = daftarMatkul[matkul].split(" ")
            msg= f"{''.join([i[0] for i in matkulSplit[:-2]])} ({matkulSplit[-1]})"
            if (random.randint(0,2) == 1):
                msg += "!"
            self.mqqtManage.sendMsg(msg)
            
            time.sleep(1)
        
