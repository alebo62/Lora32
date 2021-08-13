#include <LoRa.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 23
#define DIO0 26
#define BAND 866E6

#define RED     12
#define YELL    13
#define GREEN   14
#define BUTTON  34

#define OLED_RESET  4
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define     LINK_CHECK_DELAY 5000000
#define WAIT_ANSW_LINK_CHECK 1000000

enum {WAIT_TIM = 0, 
      WAIT_CH_ANSW = 1,
      WAIT_GREEN_TIM = 2,
      WAIT_GREEN_ANSW = 3,
      WAIT_RED_ANSW = 4
      };

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);
void sendMessage(String outgoing);
void IRAM_ATTR onTimer();
void IRAM_ATTR onTimerConn();
void onReceive(int packetSize);

String LoRaDataTx;              // outgoing message
String LoRaDataRx;

byte msgCount = 0;            // count of outgoing messages
const byte localAddress = 0xBB;     // address of this device
const byte destination = 0xFF;      // destination to send to
long lastSendTime = 0;        // last send time
//int  interval = 2000;          // interval between sends

byte state = 0;
byte lora_tx = 0;
byte wait_check;
int packetSize = 0;

volatile int link_timer;
volatile int conn_timer;

hw_timer_t * timerCheckLink = NULL;
hw_timer_t * timerConn = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE timerMux1 = portMUX_INITIALIZER_UNLOCKED;
void setup() {
  Serial.begin(115200);

  pinMode(RED, OUTPUT);
  pinMode(YELL, OUTPUT);
  pinMode(GREEN, OUTPUT);

  pinMode(BUTTON, INPUT);

  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(20);
  digitalWrite(OLED_RST, HIGH);

//  Wire.begin(OLED_SDA, OLED_SCL);
//  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false)) {
//    Serial.println(F("SSD1306 allocation failed"));
//    for (;;);
//  }
  digitalWrite(YELL, HIGH);
  digitalWrite(RED, LOW);
  digitalWrite(GREEN, LOW);
  SPI.begin(SCK, MISO, MOSI, SS);
  while (!Serial);

  //Serial.println("LoRa Duplex");

  // override the default CS, reset, and IRQ pins (optional)
  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(BAND)) {             // initialize ratio at 915 MHz
    Serial.println("LoRa init failed. Check your connections.");
    while (true);                       // if failed, do nothing
  }
  LoRa.setTxPower(20);
  LoRa.enableCrc();
  Serial.println("LoRa init succeeded.");

  // https://techtutorialsx.com/2017/10/07/esp32-arduino-timer-interrupts/
  // https://github.com/espressif/arduino-esp32/blob/af35773d65a8f328dba8090dff66ba43abe0e7be/cores/esp32/esp32-hal-timer.c#L216-L265
  timerCheckLink = timerBegin(0, 80, true);
  timerAttachInterrupt(timerCheckLink, &onTimer, true);
  timerAlarmWrite(timerCheckLink, LINK_CHECK_DELAY, true);
  timerAlarmEnable(timerCheckLink);

//  timerConn = timerBegin(1, 80, true);
//  timerAttachInterrupt(timerConn, &onTimerConn, true);
//  timerAlarmWrite(timerConn, 2000000, true);
  //timerAlarmEnable(timerConn);

}

void loop() {

  // -------------------- LINK_TIMER --------------------------
  if (link_timer)
  {
    link_timer = 0;
    Serial.println(wait_check);
    // ------Проверка связи , когда кнопка не нажата-----
    if (wait_check == WAIT_TIM)// сработал таймер - шлем запрос
    {
      wait_check = WAIT_CH_ANSW;
      LoRaDataTx = "CH";
      sendMessage(LoRaDataTx);
      timerWrite(timerCheckLink, LINK_CHECK_DELAY - WAIT_ANSW_LINK_CHECK);//  1sec
    }
    else if (wait_check == WAIT_CH_ANSW) // небыло ответа на 'CH'- сработал таймер
    {
      wait_check = WAIT_TIM;
      digitalWrite(GREEN, LOW);
      digitalWrite(RED, LOW);
      digitalWrite(YELL, HIGH);
      timerWrite(timerCheckLink, WAIT_ANSW_LINK_CHECK);//
    }
    // ----- Во время работы ------------
    else if (wait_check == WAIT_GREEN_TIM) // во время нажатия - шлем зел. цвет(проверка связи)
    { // проверка на каждое срабатывание таймера
      wait_check = WAIT_GREEN_ANSW;
      LoRaDataTx = "GREEN";
      sendMessage(LoRaDataTx);
      timerWrite(timerCheckLink, LINK_CHECK_DELAY - WAIT_ANSW_LINK_CHECK);//  1sec на ответ
       Serial.print("send\n");
    }
    else if (wait_check == WAIT_GREEN_ANSW) // небыло ответа на 'GREEN'- сработал таймер
    {
      if (msgCount < 3)
      {
        msgCount++;// 3 попытки
        LoRaDataTx = "GREEN";
        sendMessage(LoRaDataTx);
        timerWrite(timerCheckLink, WAIT_ANSW_LINK_CHECK);//  1sec на ответ
      }
      else // после 3-х попыток сброс
      {
        timerWrite(timerCheckLink, 0);
        wait_check = 0;
        state = 0;
        digitalWrite(GREEN, LOW);
        digitalWrite(RED, LOW);
        digitalWrite(YELL, HIGH);
      }
    }
    else if (wait_check == WAIT_RED_ANSW) // небыло ответа на 'GREEN'- сработал таймер
    {
      if (msgCount < 3)
      {
        msgCount++;// 3 попытки
        LoRaDataTx = "RED";
        sendMessage(LoRaDataTx);
        timerWrite(timerCheckLink, WAIT_ANSW_LINK_CHECK);//  1sec на ответ
      }
      else // после 3-х попыток сброс
      {
        timerWrite(timerCheckLink, 0);
        wait_check = WAIT_TIM;
        state = 0;
        digitalWrite(GREEN, LOW);
        digitalWrite(RED, LOW);
        digitalWrite(YELL, HIGH);
      }
    }
  }

  // -------------------- TX ----------------------------
  if (digitalRead(BUTTON) == LOW && !lora_tx)
  {
    lora_tx = 1;
    state = 1; // кнопка нажата
    //timerWrite(timerConn, 0);
    LoRaDataTx = "GREEN";
    sendMessage(LoRaDataTx);
    digitalWrite(YELL, LOW);
    digitalWrite(GREEN, LOW);
    digitalWrite(RED, LOW);
  }
  if (lora_tx && digitalRead(BUTTON) == HIGH)
  {
    lora_tx = 0;
    state = 1; // кнопка была отжата
    LoRaDataTx = "RED";
    sendMessage(LoRaDataTx);
    wait_check = WAIT_RED_ANSW;
    digitalWrite(YELL, LOW);
    digitalWrite(GREEN, LOW);
    digitalWrite(RED, LOW);
  }
  // ------------------- RX --------------------
  packetSize = LoRa.parsePacket();
  if (packetSize)
  {
    while (LoRa.available())
      LoRaDataRx = LoRa.readString();
    if (LoRaDataRx[0] == localAddress)
    {
      if (LoRaDataRx.endsWith("CH") == true)// приняли ответ на СН
      {
        digitalWrite(YELL, LOW);
        digitalWrite(GREEN, LOW);
        digitalWrite(RED, HIGH);
        wait_check = WAIT_TIM;
        timerWrite(timerCheckLink, 0);// reset timer
      }
      else if (LoRaDataRx.endsWith("GREEN") == true)
      {
        if (wait_check == WAIT_GREEN_ANSW)// ответ в  течении нажатия кнопки
        {
          timerWrite(timerCheckLink, 0);
          wait_check = WAIT_GREEN_TIM;
          msgCount = 0; // кнопка нажата
          digitalWrite(GREEN, HIGH);
          digitalWrite(RED, LOW);
          digitalWrite(YELL, LOW);
        }
        else // ответ при нажатии кнопки
        {
          if (state == 1) // кнопка была нажата
          {
            LoRaDataTx = "GREEN";
            sendMessage(LoRaDataTx);
            digitalWrite(GREEN, HIGH);
            digitalWrite(RED, LOW);
            digitalWrite(YELL, LOW);
            wait_check = 2;
            state = 0;
            timerWrite(timerCheckLink, 0);
          }
        }
      }
      else if (LoRaDataRx.endsWith("RED") == true)
      {
        if (state == 1)// кнопка была отжата
        {
          wait_check = WAIT_TIM;
          state = 0;
          msgCount = 0;
          LoRaDataTx = "RED";
          sendMessage(LoRaDataTx);
          digitalWrite(GREEN, LOW);
          digitalWrite(RED, HIGH);
          digitalWrite(YELL, LOW);
        }
      }
    }
  }
}

void sendMessage(String outgoing)
{
  LoRa.beginPacket();                   // start packet
  LoRa.write(destination);              // add destination address
  LoRa.write(localAddress);             // add sender address
  //LoRa.write(msgCount);                 // add message ID
  //LoRa.write(outgoing.length());        // add payload length
  LoRa.print(outgoing);                 // add payload
  LoRa.endPacket();                     // finish packet and send it
  //msgCount++;                           // increment message ID
}

void IRAM_ATTR onTimer()
{
  portENTER_CRITICAL_ISR(&timerMux);
  link_timer = 1;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void IRAM_ATTR onTimerConn()
{
  portENTER_CRITICAL_ISR(&timerMux1);
  conn_timer = 1;
  timerAlarmDisable(timerConn);
  portEXIT_CRITICAL_ISR(&timerMux1);
  
}

//void onReceive(int packetSize)
//{
//  if (packetSize == 0) return;
//
//
//  int recipient = LoRa.read();
//  byte sender = LoRa.read();
//  byte incomingMsgId = LoRa.read();
//  byte incomingLength = LoRa.read();
//
//  String incoming = "";
//
//  while (LoRa.available()) {
//    incoming += (char)LoRa.read();
//  }
//
//  //  if (incomingLength != incoming.length()) {
//  //    Serial.println("error: message length does not match length");
//  //    return;
//  //  }
//
//
//  if (recipient != localAddress && recipient != 0xFF) {
//    Serial.println("This message is not for me.");
//    return;
//  }
//
//  if (incoming.endsWith("CH") == true)
//  {
//    digitalWrite(YELL, LOW);
//    digitalWrite(GREEN, LOW);
//    digitalWrite(RED, HIGH);
//    timerWrite(timerCheckLink, 500000);// 900ms
//    interruptCounter = 0;
//    //sendMessage("CH");
//  }

//  int snr = LoRa.packetSnr();
//  int rssi = LoRa.packetRssi();
//  display.clearDisplay();
//  display.setCursor(0,0);
//  display.println("LORA SENDER");
//  display.setCursor(0,20);
//  display.setTextSize(1);
//  display.print("LoRa packet sent.");
//  display.setCursor(0,30);
//  display.print("Counter:");
//  display.setCursor(50,30);
//  display.print(incomingMsgId);
//  display.setCursor(0,40);
//  display.print("RSSI:");
//  display.setCursor(30,40);
//  display.print(rssi);
//  display.setCursor(0,50);
//  display.print("SNR");
//  display.setCursor(30,50);
//  display.print(snr);
//  display.display();

//  Serial.println("Received from: 0x" + String(sender, HEX));
//  Serial.println("Sent to: 0x" + String(recipient, HEX));
//  Serial.println("Message ID: " + String(incomingMsgId));
//  Serial.println("Message length: " + String(incomingLength));
//  Serial.println("Message: " + incoming);
//  Serial.println("RSSI: " + String(LoRa.packetRssi()));
//  Serial.println("Snr: " + String(LoRa.packetSnr()));
//  Serial.println();
//}




// parse for a packet, and call onReceive with the result:
//  onReceive(LoRa.parsePacket());


//    while (LoRa.available()) {
//      LoRaData = LoRa.readString();
//      if(LoRaData.endsWith("GREEN") == true)
//      {
//        digitalWrite(RED, LOW);
//        digitalWrite(YELL, LOW);
//        digitalWrite(GREEN, HIGH);
//      }
//      if(LoRaData.endsWith("RED") == true)
//      {
//        digitalWrite(GREEN, LOW);
//        digitalWrite(YELL, LOW);
//        digitalWrite(RED, HIGH);
//      }

//  if (millis() - lastSendTime > interval) {
//    String message = "HeLoRa World!";   // send a message
//    sendMessage(message);
//    Serial.println("Sending " + message);
//    lastSendTime = millis();            // timestamp the message
//    interval = 1000;    // 2-3 seconds
//  }
