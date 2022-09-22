#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

//
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM

#include "camera_pins.h"

#define BOT_TOKEN "5778940753:AAGRUal7OadBhvMYhYG7KxHtRUPebj38iq0"
#define CHAT_ID "1803294788"
#define FLASH_LED_PIN 4

#define DUMMY_SERVO1_PIN 12     //We need to create 2 dummy servos.
#define DUMMY_SERVO2_PIN 13     //So that ESP32Servo library does not interfere with pwm channel and timer used by esp32 camera.

#define PAN_PIN 14
#define SERVO_PIN 15

Servo dummyServo1;
Servo dummyServo2;
Servo panServo;
Servo myServo;


WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
// Иницијализација на самиот бот кој што ќе го користиме

int val = 90;
String message;
const unsigned long BOT_MTBS = 1000;
unsigned long bot_lasttime; // last time messages' scan has been done

bool flashState = LOW;
bool rotateFlag = false;
camera_fb_t *fb = NULL;
bool isMoreDataAvailable();
byte *getNextBuffer();
int getNextBufferLen();

bool dataAvailable = false;

//  Serial.println("handleNewMessages");
//  Serial.println(String(numNewMessages));

void handleNewMessages(int numNewMessages)
{
  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = String(bot.messages[i].chat_id);
    // Се зема ID од корисникот кој ја пратил командата 
    // со цел да може да се врати одговор на точното место
    
    String text = bot.messages[i].text;
    // Пораката која се добила од корисникот

    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    if(text == "/reset"){
      myServo.write(90); 
      continue;
    }
  
    if (text == "/rotate"){
      rotateFlag = true;
      bot.sendMessage(chat_id, "Vlegovte vo rezim na rotacija, vnesete agol", "");
      continue;
    }
    
    if (text == "/stop"){
      rotateFlag = false;
      bot.sendMessage(chat_id, "Izlegovte od rezim na rotacija", "");
      continue;
    }
    
    if (text == "/flash"){
      flashState = !flashState;
      digitalWrite(FLASH_LED_PIN, flashState);
      continue;
    }
//        Serial.println("Camera capture failed");

    if (text == "/photo")
    {
      fb = NULL; // Се чисти file buffer
      fb = esp_camera_fb_get();
      // Пробуваме да сликаме со камерата
      if (!fb)
      {
        bot.sendMessage(chat_id, "Neuspesna slika", "");
        return;
      }
      dataAvailable = true;
      bot.sendPhotoByBinary(chat_id, "image/jpeg", fb->len,
                            isMoreDataAvailable, nullptr,
                            getNextBuffer, getNextBufferLen);
      //sendPhotoByBinary е предефинирана фукнција од самата
      // UniversalTelegramBot библиотека
      esp_camera_fb_return(fb);
      continue;
    }

    if (text == "/flashphoto")
    {
      flashState = !flashState;
      digitalWrite(FLASH_LED_PIN, flashState);
      delay(2000); // го пуштаме блицот и чекаме 2 секунди
      // за камерата да може да се прилагоди
      fb = NULL; // Се чисти file buffer
      fb = esp_camera_fb_get();
      // Пробуваме да сликаме со камерата
      if (!fb)
      {
        bot.sendMessage(chat_id, "Neuspesna slika", "");
        return;
      }
      dataAvailable = true;
      bot.sendPhotoByBinary(chat_id, "image/jpeg", fb->len,
                            isMoreDataAvailable, nullptr,
                            getNextBuffer, getNextBufferLen);
      //sendPhotoByBinary е предефинирана фукнција од самата
      // UniversalTelegramBot библиотека
      esp_camera_fb_return(fb);
      flashState = !flashState;
      digitalWrite(FLASH_LED_PIN, flashState);
      continue;
    }

    if (text == "/info")
    {
      String welcome = "Dobredojde na ESP32Cam Telegram botot.\n\n";
      welcome += "/photo : ke se isprati slika\n";
      welcome += "/flashphoto : ke se isprati slika so blic\n";
      welcome += "/flash : vkluci ili iskluci blic\n";
      welcome += "/rotate : vlezi vo rezim na rotacija i potoa vnesi broj\n";
      welcome += "pr: 45 ili -45\n";
      welcome += "/stop : za da se izlezi od rezim na rotacija\n";
      welcome += "/reset : za vrakanje vo prvobitna polozba\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
      continue;
    }

    // Најпрво мора да сме во режим на ротација
    if (text.length()!=0 and rotateFlag){
      if(text == "/reset"){val = 90; myServo.write(val); }
      if(text.length()==3 && text.startsWith("-") && isDigit(text[1]) && isDigit(text[2])){
        text.remove(0, 1); //Тука се бриши знакот - од напред и остануваат само внесените цифри
        val -= text.toInt();
        if (val <=0){val=0;}
        // Серво моторот неможе да се заврти на пр. -30 степени,
        // па затоа имаме ограничување на 0
        myServo.write(val);
      }
      else if(text.length()==2 && text.startsWith("-") && isDigit(text[1])){
        text.remove(0, 1);
        val -= text.toInt();
        if (val <=0){val=0;}
        Serial.println(val);
        myServo.write(val);
      }
      else if(text.length()==2 && isDigit(text[0]) && isDigit(text[1])){
        val += text.toInt();
        if (val >=180){val=180;}
        // Истото ограничување се прави од другата страна до 180 степени
        myServo.write(val);
      }
      else if(text.length()==1 && isDigit(text[0])){
        val += text.toInt();
        if (val >=180){val=180;}
        Serial.println(val);
        myServo.write(val);
      }
    }
  }
}

bool isMoreDataAvailable()
{
  if (dataAvailable)
  {
    dataAvailable = false;
    return true;
  }
  else
  {
    return false;
  }
}

byte *getNextBuffer()
{
  if (fb)
  {
    return fb->buf;
  }
  else
  {
    return nullptr;
  }
}

int getNextBufferLen()
{
  if (fb)
  {
    return fb->len;
  }
  else
  {
    return 0;
  }
}
const char* ssid = "PrilepEbagoFan";
const char* password = "andrejce666";
int test;
void startCameraServer();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, flashState); 
    // Го сетираме пинот за блиц на камерата како излезен
    // и default состојба му е исклучено
    
 dummyServo1.attach(DUMMY_SERVO1_PIN);
  dummyServo2.attach(DUMMY_SERVO2_PIN);  
  panServo.attach(PAN_PIN);
  myServo.attach(SERVO_PIN);
  
  val = 90;
  myServo.write(val);


  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  WiFi.begin(ssid, password);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  
  
  Serial.println("' to connect");
//  bot.sendMessage(CHAT_ID, WiFi.localIP(), "");

   Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);

  // Make the bot wait for a new message for up to 60seconds
  bot.longPoll = 60;
}

void loop() {
  if (millis() - bot_lasttime > BOT_MTBS)
  // BOT_MTBS - Mean Time Between Scan и е еднакво на 1000ms
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      // handleNewMessages ни е функција каде што се интерпретираат
      // добиените пораки од корисникот
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    bot_lasttime = millis();
  }
}
