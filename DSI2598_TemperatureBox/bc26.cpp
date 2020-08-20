#include "bc26.h"

static String         bc26_buff;
static char           bc26_host[40], bc26_user[40], bc26_key[50];
static int            bc26_port;
static SoftwareSerial bc26(8, 9);
subscribe_t           sub_arr[LIMIT_OF_SUBS];

static bool _BC26SendCmdReply(const char *cmd, const char *reply, unsigned long timeout);

static bool _BC26SendCmdReply(const char *cmd, const char *reply, unsigned long timeout)
{
    bc26_buff = "";
    DEBUG_PRINT(cmd);
    while (bc26.available()) {
        bc26.read();
    }
    bc26.println(cmd);
    unsigned long timer = millis();
    while (millis() - timer < timeout) {
        if (bc26.available()) {
            bc26_buff += bc26.readStringUntil('\n');
        }
        if (bc26_buff.indexOf(reply) != -1) {
            DEBUG_PRINT(bc26_buff);
            return true;
        }
    }
    if (bc26_buff == "") {
        // BC26 module no response
        Serial.println(F("BC26 module no response ! Please Power off and power on again !!"));
        while (true)
            ;
    } else {
        DEBUG_PRINT(bc26_buff);
    }
    return false;
}

bool BC26Init(long baudrate, const char *apn, int band)
{
    char buff[100];
    bool result;
    // set random seed
    randomSeed(analogRead(A0));
    // init nbiot SoftwareSerial
    bc26.begin(baudrate);
    do {
        result = true;
        // wait bc26 module reboot
        Serial.println(F("Wait BC26 module reboot..."));
        //  reset bc26 module
        result &= _BC26SendCmdReply("AT+QRST=1", "+CPIN: READY", 5000);
        // echo mode off
        result &= _BC26SendCmdReply("ATE0", "OK", 2000);
        // set apn
        sprintf(buff, "AT+QCGDEFCONT=\"IP\",\"%s\"", apn);
        result &= _BC26SendCmdReply(buff, "OK", 2000);
        // set band
        sprintf(buff, "AT+QBAND=1,%d", band);
        result &= _BC26SendCmdReply(buff, "OK", 2000);
        // close EDRX
        result &= _BC26SendCmdReply("AT+CEDRXS=0", "OK", 2000);
        // close SCLK
        result &= _BC26SendCmdReply("AT+QSCLK=0", "OK", 2000);
    } while (!result);

    if (!_BC26SendCmdReply("AT+CGATT?", "+CGATT: 1", 2000)) {
        Serial.println(F("Connect to 4G BS....."));
        if (_BC26SendCmdReply("AT", "+IP:", 60000)) {
            Serial.println(F("Network is ok !!"));
            return true;
        } else {
            Serial.println(F("Network is not ok !!"));
            return false;
        }
    }
    Serial.println(F("Network is ok !!"));
    return true;
}

bool BC26ConnectMQTTServer(const char *host, const char *user, const char *key, int port)
{
    long random_id = random(65535);
    strcpy(bc26_host, host);
    strcpy(bc26_user, user);
    strcpy(bc26_key, key);
    bc26_port = port;

    char buff[255];

    while (!_BC26SendCmdReply("AT+QMTCONN?", "+QMTCONN: 0,3", 2000)) {
        while (!_BC26SendCmdReply("AT+QMTOPEN?", "+QMTOPEN: 0,", 2000)) {
            sprintf(buff, "AT+QMTOPEN=0,\"%s\",%d", host, port);
            if (_BC26SendCmdReply(buff, "+QMTOPEN: 0,0", 20000)) {
                Serial.println(F("Opened MQTT Channel Successfully"));
            } else {
                Serial.println(F("Failed to open MQTT Channel"));
            }
        }
        sprintf(buff, "AT+QMTCONN=0,\"Arduino_BC26_%ld\",\"%s\",\"%s\"", random_id, user, key);
        if (_BC26SendCmdReply(buff, "+QMTCONN: 0,0,0", 20000)) {
        } else {
            Serial.println(F("Failed to Connect MQTT Server"));
        }
    }
    Serial.println(F("Connect MQTT Server Successfully"));
    return true;
}

bool BC26MQTTPublish(const char *topic, char *msg, int qos)
{
    char buff[200];
    long msgID = 0;
    if (qos > 0) {
        msgID = random(1, 65535);
    }
    sprintf(buff, "AT+QMTPUB=0,%ld,%d,0,\"%s\",\"%s\"", msgID, qos, topic, msg);
    //DEBUG_PRINT(buff);
    while (!_BC26SendCmdReply(buff, "+QMTPUB: 0,0,0", 10000)) {
        BC26ConnectMQTTServer(bc26_host, bc26_user, bc26_key, bc26_port);
    }
    Serial.print(F("Publish :("));
    Serial.print(msg);
    Serial.println(F(") Successfully"));
    return true;
}

bool BC26MQTTSubscribe(const char *topic, int qos, void (*callback)(char *msg))
{
    static int sub_sum = 0;
    char       buff[200];
    sprintf(buff, "AT+QMTSUB=0,1,\"%s\",%d", topic, qos);
    while (!_BC26SendCmdReply(buff, "+QMTSUB: 0,1,0,0", 10000)) {
        BC26ConnectMQTTServer(bc26_host, bc26_user, bc26_key, bc26_port);
    }
    sub_arr[sub_sum].topic      = topic;
    sub_arr[sub_sum++].callback = callback;
    if (sub_sum > LIMIT_OF_SUBS) {
        Serial.println(F("Subscription limit exceeded !"));
        return false;
    }
    Serial.print(F("Subscribe Topic("));
    Serial.print(topic);
    Serial.println(F(") Successfully"));
    return true;
}

int BC26CSQ(void)
{
    String rssi;
    int    s_idx;

    if (_BC26SendCmdReply("AT+CSQ", "+CSQ: ", 2000)) {
        s_idx = bc26_buff.indexOf("+CSQ: ");
        s_idx += 6;
        while (bc26_buff[s_idx] != ',') {
            rssi += bc26_buff[s_idx++];
        }
        return rssi.toInt();
    }
    return -1;
}

void BC26ProcSubs(void)
{
    // +QMTRECV: 0,0,"<topic>","<message>"
    char *head, *tail, *msg;
    bc26_buff = "";

    if (bc26.available()) {
        bc26_buff += bc26.readStringUntil('\n');
        head = strstr(bc26_buff.c_str(), "+QMTRECV: 0,0,");
        if (head) {
            for (int ii = 0; ii < LIMIT_OF_SUBS; ii++) {
                msg = strstr(head, sub_arr[ii].topic);
                if (msg) {
                    DEBUG_PRINT("receive:");
                    DEBUG_PRINT(bc26_buff);
                    DEBUG_PRINT(msg);
                    msg += (strlen(sub_arr[ii].topic) + 3);
                    DEBUG_PRINT(msg);
                    tail = strstr(msg, "\"");
                    DEBUG_PRINT(tail);
                    *tail = '\0';
                    DEBUG_PRINT(msg);
                    sub_arr[ii].callback(msg);
                    return;
                }
            }
        }
    }
}
