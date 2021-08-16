#include <dummy.h>

#include <SPI.h>
#include <LoRa.h>
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
#define byte int8_t
//OLED pins
#define OLED_RESET  4
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define     LINK_CHECK_DELAY 6000000

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);
void sendMessage(String);
void IRAM_ATTR onTimer();

String LoRaDataRx;
String LoRaDataTx;

byte state = 0;
byte msgCount = 0;            // count of outgoing messages
const byte localAddress = 0xFF;     // address of this device
const byte destination = 0xBB;      // destination to send to
int packetSize;

volatile int link_timer;
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void setup() {
    Serial.begin(115200);

    pinMode(RED, OUTPUT);
    pinMode(YELL, OUTPUT);
    pinMode(GREEN, OUTPUT);

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
    LoRa.setPins(SS, RST, DIO0);
    if (!LoRa.begin(BAND)) {
        Serial.println("Starting LoRa failed!");
        while (1);
    }
    LoRa.setTxPower(20);
    LoRa.enableCrc();
    Serial.println("LoRa Initializing OK!");

    // https://techtutorialsx.com/2017/10/07/esp32-arduino-timer-interrupts/
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, LINK_CHECK_DELAY, true);
    timerAlarmEnable(timer);
}

void loop() {

    if (link_timer)
    {
        digitalWrite(GREEN, LOW);
        digitalWrite(RED, LOW);
        digitalWrite(YELL, HIGH);
        link_timer = 0;
    }
    packetSize = LoRa.parsePacket();
    if (packetSize)
    {
        while (LoRa.available()) {
            LoRaDataRx = LoRa.readString();

            if (LoRaDataRx[0] == localAddress)
            {
                timerWrite(timer, 0);// reset counter
                if (LoRaDataRx.endsWith("CH") == true)
                {
                    digitalWrite(YELL, LOW);
                    digitalWrite(GREEN, LOW);
                    digitalWrite(RED, HIGH);
                    LoRaDataTx = "CH";
                    sendMessage(LoRaDataTx);
                    //Serial.print("check\n");
                }
                else if (LoRaDataRx.endsWith("GREEN") == true)
                {
                    // press_but serv->cl->serv->cli(green light up)
                    if (state == 0)
                    {
                        state = 1;
                        LoRaDataTx = "GREEN";
                        sendMessage(LoRaDataTx);
                    }
                    else if (state == 1)// нажали кнопку
                    {
                        state = 2;
                        digitalWrite(RED, LOW);
                        digitalWrite(YELL, LOW);
                        digitalWrite(GREEN, HIGH);
                        //Serial.print("green1\n");
                    }
                    // during press(green)   serv->cli->serv
                    else if (state == 2)
                    {
                        LoRaDataTx = "GREEN";
                        sendMessage(LoRaDataTx);
                        //Serial.print("green2\n");
                    }

                }
                else if (LoRaDataRx.endsWith("RED") == true)
                {
                    // unpress_but serv->cl->serv->cli(red light up)
                    if (state == 2 )//|| state == 0)
                    {
                        state = 3; // отпустили кнопку
                        LoRaDataTx = "RED";
                        sendMessage(LoRaDataTx);
                    }
                    else if (state == 3)// подтвердили что кнопка отпущена
                    {
                        state = 0;
                        digitalWrite(GREEN, LOW);
                        digitalWrite(YELL, LOW);
                        digitalWrite(RED, HIGH);
                    }
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
    LoRa.print(outgoing);                 // add payload
    LoRa.endPacket();                     // finish packet and send it
}

void IRAM_ATTR onTimer()
{
    portENTER_CRITICAL_ISR(&timerMux);
    link_timer = 1;
    state = 0;
    portEXIT_CRITICAL_ISR(&timerMux);
}
