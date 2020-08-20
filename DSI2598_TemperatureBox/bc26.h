#ifndef BC26_H
#define BC26_H
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <string.h>

#define DEBUG_MODE true
#define debugSerial Serial
#if DEBUG_MODE
#define DEBUG_PRINT(x) debugSerial.println(x)
#else
#define DEBUG_PRINT(x)
#endif
#define ERROR(x) debugSerial.println(x)

#define LIMIT_OF_SUBS   5

#define BAUDRATE_9600   9600
#define BAUDRATE_19200  19200
#define BAUDRATE_38400  8400

#define MQTT_PORT_DEFAULT   1883
#define MQTT_PORT_TLS       8883
#define MQTT_PORT_WS        8080
#define MQTT_PORT_WS_TLS    8081

typedef enum
{
    MQTT_QOS0,
    MQTT_QOS1,
} MQTT_QOS_E;

typedef enum
{
    BAND_1  = 1,
    BAND_3  = 3,
    BAND_5  = 5,
    BAND_8  = 8,
    BAND_20 = 20,
} BC26_BAND;

typedef struct {
    const char *topic;
    void (*callback)(char *msg);
} subscribe_t;

bool BC26Init(long baudrate, const char *apn, int band);
bool BC26ConnectMQTTServer(const char *host, const char *user, const char *key, int port);
bool BC26MQTTPublish(const char *topic, char *msg, int qos);
bool BC26MQTTSubscribe(const char *topic, int qos, void (*callback)(char *msg));
int  BC26CSQ(void);
void BC26ProcSubs(void);

#endif /* BC26_H */
