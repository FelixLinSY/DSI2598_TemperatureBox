#include "DHT.h"
#include <Adafruit_NeoPixel.h>
#include "bc26.h"
#ifdef __AVR__
  #include <avr/power.h>
#endif

#define NBIOT_APN               "internet.iot"
#define AIO_SERVER              "io.adafruit.com"
#define AIO_USERNAME            "YOUR_AIO_NAME"
#define AIO_KEY                 "YOUR_AIO_KEY"

const char *topic_red           = AIO_USERNAME"/feeds/temp-box.red";
const char *topic_green         = AIO_USERNAME"/feeds/temp-box.green";
const char *topic_blue          = AIO_USERNAME"/feeds/temp-box.blue";
const char *topic_humidity      = AIO_USERNAME"/feeds/temp-box.humidity";
const char *topic_temperature   = AIO_USERNAME"/feeds/temp-box.temperature";


#define     UPLOAD_INTERVAL     15000       // unit: ms
#define     NEOPIXEL_PIN        10
#define     NEOPIXEL_NUM        8
#define     DHT_PIN             3           // Digital pin connected to the DHT sensor
#define     DHT_TYPE            DHT22       // choose one of DHT11/DHT22/DHT21

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NEOPIXEL_NUM, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

DHT             dht(DHT_PIN, DHT_TYPE);
int             update_led = 1, red, green, blue;
unsigned long   timer;

void setup() {
    // This is for Trinket 5V 16MHz, you can remove these three lines if you are not using a Trinket
    #if defined (__AVR_ATtiny85__)
        if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
    #endif
    // End of trinket special code
    Serial.begin(115200);
    
    // DHT init
    dht.begin();

    // NeoPixel init
    strip.begin();
    strip.setBrightness(50);
    strip.show(); // Initialize all pixels to 'off'
    
    // bc26 init
    BC26Init(BAUDRATE_9600, NBIOT_APN, BAND_8);
    // connect to server
    BC26ConnectMQTTServer(AIO_SERVER, AIO_USERNAME, AIO_KEY, MQTT_PORT_DEFAULT);
    // Subscribe topic
    BC26MQTTSubscribe(topic_red, MQTT_QOS0, red_callback);
    BC26MQTTSubscribe(topic_green, MQTT_QOS0, green_callback);
    BC26MQTTSubscribe(topic_blue, MQTT_QOS0, blue_callback);
    
}

void red_callback(char *msg)
{
    Serial.print(F("Red: "));
    Serial.println(msg);
    update_led = 1;
    red = atoi(msg);
}

void green_callback(char *msg)
{
    Serial.print(F("green: "));
    Serial.println(msg);
    update_led = 1;
    green = atoi(msg);
}

void blue_callback(char *msg)
{
    Serial.print(F("blue: "));
    Serial.println(msg);
    update_led = 1;
    blue = atoi(msg);
}

void loop() {
    float           humidity, temperature;
    char            buff[20];

    BC26ProcSubs();
    
    if (update_led) {
        update_led = 0;
        colorWipe(strip.Color(red, green, blue), 50);    // Black/off
    }
    
    if (millis() >= timer) {
        getDHT(&humidity, &temperature);
        timer = millis() + UPLOAD_INTERVAL;
        sprintf(buff, "%s", String(temperature).c_str());
        BC26MQTTPublish(topic_temperature, buff, MQTT_QOS0);
        sprintf(buff, "%s", String(humidity).c_str());
        BC26MQTTPublish(topic_humidity, buff, MQTT_QOS0);
    }

    delay(20);
}

void getDHT(float *humi, float *temp) {
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    float h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float t = dht.readTemperature();
    // Read temperature as Fahrenheit (isFahrenheit = true)
    float f = dht.readTemperature(true);
    // Check if any reads failed and exit early (to try again).
    if (isnan(h) || isnan(t) || isnan(f)) {
        Serial.println(F("Failed to read from DHT sensor!"));
        return;
    }

    *humi = h;
    *temp = t;

    // Compute heat index in Fahrenheit (the default)
    float hif = dht.computeHeatIndex(f, h);
    // Compute heat index in Celsius (isFahreheit = false)
    float hic = dht.computeHeatIndex(t, h, false);

    Serial.print(F("Humidity: "));
    Serial.print(h);
    Serial.print(F("%  Temperature: "));
    Serial.print(t);
    Serial.print(F("°C "));
    Serial.print(f);
    Serial.print(F("°F  Heat index: "));
    Serial.print(hic);
    Serial.print(F("°C "));
    Serial.print(hif);
    Serial.println(F("°F"));
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
    for(uint16_t i=0; i<strip.numPixels(); i++) {
        strip.setPixelColor(i, c);
        strip.show();
        delay(wait);
    }
}

void rainbow(uint8_t wait) {
    uint16_t i, j;

    for(j=0; j<256; j++) {
        for(i=0; i<strip.numPixels(); i++) {
            strip.setPixelColor(i, Wheel((i+j) & 255));
        }
        strip.show();
        delay(wait);
    }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
    uint16_t i, j;

    for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
        for(i=0; i< strip.numPixels(); i++) {
            strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
        }
        strip.show();
        delay(wait);
    }
}

//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait) {
    for (int j=0; j<10; j++) {  //do 10 cycles of chasing
        for (int q=0; q < 3; q++) {
            for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
                strip.setPixelColor(i+q, c);    //turn every third pixel on
            }
            strip.show();

            delay(wait);

            for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
                strip.setPixelColor(i+q, 0);        //turn every third pixel off
            }
        }
    }
}

//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {
    for (int j=0; j < 256; j++) {     // cycle all 256 colors in the wheel
        for (int q=0; q < 3; q++) {
            for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
                strip.setPixelColor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
            }
            strip.show();

            delay(wait);

            for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
                strip.setPixelColor(i+q, 0);        //turn every third pixel off
            }
        }
    }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
    WheelPos = 255 - WheelPos;
    if(WheelPos < 85) {
        return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    }
    if(WheelPos < 170) {
        WheelPos -= 85;
        return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
    WheelPos -= 170;
    return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
