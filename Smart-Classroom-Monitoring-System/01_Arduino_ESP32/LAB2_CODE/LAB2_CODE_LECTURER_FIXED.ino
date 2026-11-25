

#include <WiFi.h>

#include <WiFiClientSecure.h>

#include <PubSubClient.h>

#include <SPI.h>

#include <MFRC522.h>

#include <Wire.h>

#include <LiquidCrystal_I2C.h>

#include <time.h>



// =====================================================

// SMART CLASSROOM - NODE-RED DATABASE VERSION

// One ESP32 = One physical LAB / DOOR

//

// LAB_ID is different for each ESP32.

// Student information and class access come from

// Node-RED / SQLite through MQTT.

// ESP32 keeps only temporary attendance state and

// uses NTP for real entry/exit timestamps.

// =====================================================



// ---------------- LAB ID ----------------

// CHANGE THIS ONLY PER ESP32

const char* LAB_ID = "LAB 2";





// ---------------- WIFI / MQTT ----------------

// Fill in your own credentials.

// Do NOT share your MQTT password publicly.

const char* WIFI_SSID = "KEON";

const char* WIFI_PASSWORD = "123456789";



const char* MQTT_SERVER = "33c2c07549be4b20a69af8763e32741a.s1.eu.hivemq.cloud";

const int MQTT_PORT = 8883;

const char* MQTT_USERNAME = "esp32_fyp";

const char* MQTT_PASSWORD = "Ancik@2004";



// Existing attendance/event topic

const char* MQTT_STATUS_TOPIC = "fyp/classroom/status";



// New request/response topics for Student Database

const char* MQTT_REQUEST_TOPIC  = "fyp/classroom/rfid/request";

const char* MQTT_RESPONSE_TOPIC = "fyp/classroom/rfid/response";



// ---------------- MALAYSIA TIME ----------------

const long GMT_OFFSET_SEC = 8 * 3600;

const int DAYLIGHT_OFFSET_SEC = 0;



// ---------------- PINS ----------------

#define SS_PIN      5

#define RST_PIN     4

#define RELAY_PIN   32

#define GREEN_LED   25

#define RED_LED     33

#define BUTTON_PIN  27



// ---------------- OBJECTS ----------------

MFRC522 rfid(SS_PIN, RST_PIN);

LiquidCrystal_I2C lcd(0x27, 20, 4);



WiFiClientSecure espClient;

PubSubClient mqttClient(espClient);



// =====================================================

// TEMPORARY ACTIVE ATTENDANCE RECORD

// No student database is stored in ESP32.

// =====================================================



const int MAX_ACTIVE_STUDENTS = 30;



struct AttendanceRecord {

  bool active;

  String uid;

  String student;

  String semester;

  String subject;

  String className;

  String entryDate;

  String entryTime;

  time_t entryEpoch;

};



AttendanceRecord records[MAX_ACTIVE_STUDENTS];



// =====================================================

// NODE-RED RESPONSE VARIABLES

// =====================================================



bool responseReceived = false;

String responseRequestId = "";



String responseUid = "";

String responseStudent = "";

String responseSemester = "";

String responseStatus = "";

String responseRole = "";

String responseAccess = "";

String responseReason = "";

String responseClassState = "";

String responseSubject = "";

String responseClassName = "";

String responseClassStart = "";

String responseClassEnd = "";



String pendingRequestId = "";



// =====================================================

// FUNCTION DECLARATIONS

// =====================================================



void connectWiFi();

void setupTime();

void connectMQTT();

void mqttCallback(char* topic, byte* payload, unsigned int length);



void handleRFID();

void handleExitButton();



bool requestStudentFromNodeRED(String uid);

void clearResponse();

void processRFIDResponse();

void lecturerFullAccess();



int findActiveRecord(String uid);

int findFreeRecord();



void clockIn(String uid);

void reEntry(int index);

void clockOut(int index);



void showClassNotActive();

void showClassEndedNoRecord();

void showAccessDenied(String reason);

void showCommunicationError();

void resetRecord(int index);



void publishAttendanceEvent(

  String type,

  String uid,

  String student,

  String semester,

  String subject,

  String className,

  String attendance,

  String entry,

  String exit,

  String duration

);



String getUID();

String getDate();

String getTime();

String formatDuration(unsigned long seconds);



String jsonGetString(String json, String key);

bool jsonGetBool(String json, String key, bool defaultValue);



// =====================================================

// SETUP

// =====================================================



void setup() {

  Serial.begin(115200);



  SPI.begin();

  rfid.PCD_Init();



  Wire.begin(21, 22);

  lcd.init();

  lcd.backlight();



  pinMode(RELAY_PIN, OUTPUT);

  pinMode(GREEN_LED, OUTPUT);

  pinMode(RED_LED, OUTPUT);

  pinMode(BUTTON_PIN, INPUT_PULLUP);



  digitalWrite(RELAY_PIN, HIGH);

  digitalWrite(GREEN_LED, LOW);

  digitalWrite(RED_LED, HIGH);



  for (int i = 0; i < MAX_ACTIVE_STUDENTS; i++) {

    resetRecord(i);

  }



  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("SMART CLASSROOM");

  lcd.setCursor(0, 1);

  lcd.print(LAB_ID);

  delay(1500);



  connectWiFi();

  setupTime();



  espClient.setInsecure();



  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  mqttClient.setCallback(mqttCallback);

  mqttClient.setBufferSize(2048);



  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("SYSTEM READY");

  lcd.setCursor(0, 1);

  lcd.print(LAB_ID);

  lcd.setCursor(0, 2);

  lcd.print("SCAN RFID");



  Serial.println();

  Serial.println("==============================");

  Serial.println("SMART CLASSROOM");

  Serial.print("LAB: ");

  Serial.println(LAB_ID);

  Serial.println("NODE-RED DATABASE MODE");

  Serial.println("==============================");

}



// =====================================================

// LOOP

// =====================================================



void loop() {



  if (WiFi.status() != WL_CONNECTED) {

    connectWiFi();

  }



  if (!mqttClient.connected()) {

    connectMQTT();

  }



  mqttClient.loop();



  if (digitalRead(BUTTON_PIN) == LOW) {

    handleExitButton();

    delay(500);

  }



  handleRFID();

}



// =====================================================

// MQTT CALLBACK

// =====================================================



void mqttCallback(char* topic, byte* payload, unsigned int length) {



  String incomingTopic = String(topic);



  if (incomingTopic != MQTT_RESPONSE_TOPIC) {

    return;

  }



  String message = "";



  for (unsigned int i = 0; i < length; i++) {

    message += (char)payload[i];

  }



  Serial.println();

  Serial.println("MQTT RESPONSE:");

  Serial.println(message);



  String type = jsonGetString(message, "type");



  if (type != "RFID_RESPONSE") {

    return;

  }



  String requestId = jsonGetString(message, "requestId");



  if (requestId != pendingRequestId) {

    Serial.println("Response ignored: request ID mismatch.");

    return;

  }



  responseRequestId = requestId;

responseUid = jsonGetString(message, "uid");
responseStudent = jsonGetString(message, "student");
responseSemester = jsonGetString(message, "semester");
responseStatus = jsonGetString(message, "status");
responseRole = jsonGetString(message, "role");
responseAccess = jsonGetString(message, "access");
responseReason = jsonGetString(message, "reason");
responseClassState = jsonGetString(message, "classState");
responseSubject = jsonGetString(message, "subject");
responseClassName = jsonGetString(message, "className");
responseClassStart = jsonGetString(message, "start");
responseClassEnd = jsonGetString(message, "end");

Serial.println("===== RECEIVED DATA =====");
Serial.println("UID: " + responseUid);
Serial.println("NAME: " + responseStudent);
Serial.println("STATUS: " + responseStatus);
Serial.println("ROLE: " + responseRole);
Serial.println("ACCESS: " + responseAccess);
Serial.println("CLASS STATE: " + responseClassState);
Serial.println("=========================");

responseReceived = true;

}



// =====================================================

// RFID

// =====================================================



void handleRFID() {



  if (!rfid.PICC_IsNewCardPresent()) {

    return;

  }



  if (!rfid.PICC_ReadCardSerial()) {

    return;

  }



  String uid = getUID();



  Serial.println();

  Serial.println("==============================");

  Serial.print("RFID UID: ");

  Serial.println(uid);



  rfid.PICC_HaltA();

  rfid.PCD_StopCrypto1();



  if (!requestStudentFromNodeRED(uid)) {

    showCommunicationError();

    delay(1200);

    return;

  }



  processRFIDResponse();



  delay(500);

}



// =====================================================

// REQUEST NODE-RED / SQLITE

// =====================================================



bool requestStudentFromNodeRED(String uid) {



  if (!mqttClient.connected()) {

    return false;

  }



  clearResponse();



  pendingRequestId =

    String(LAB_ID) + "_" +

    String((uint32_t)millis()) + "_" +

    uid;



  String payload = "{";

  payload += "\"type\":\"RFID_REQUEST\",";

  payload += "\"requestId\":\"" + pendingRequestId + "\",";

  payload += "\"lab\":\"" + String(LAB_ID) + "\",";

  payload += "\"uid\":\"" + uid + "\"";

  payload += "}";



  Serial.print("MQTT REQUEST: ");

  Serial.println(payload);



  bool published =

    mqttClient.publish(MQTT_REQUEST_TOPIC, payload.c_str());



  if (!published) {

    Serial.println("MQTT request publish FAILED");

    return false;

  }



  unsigned long startWait = millis();



  while (!responseReceived &&

         millis() - startWait < 5000) {



    mqttClient.loop();

    delay(10);

  }



  if (!responseReceived) {

    Serial.println("NODE-RED RESPONSE TIMEOUT");

    pendingRequestId = "";

    return false;

  }



  return true;

}



// =====================================================

// PROCESS NODE-RED RESPONSE

// =====================================================




void processRFIDResponse() {

  Serial.println("---- NODE-RED DECISION ----");

  Serial.println("Student/Lecturer: " + responseStudent);
  Serial.println("UID: " + responseUid);
  Serial.println("Status: " + responseStatus);
  Serial.println("Role: " + responseRole);
  Serial.println("Access: " + responseAccess);
  Serial.println("Class State: " + responseClassState);
  Serial.println("Subject: " + responseSubject);

  // LECTURER FULL ACCESS
  // A lecturer with GRANTED access bypasses the student timetable.
  // Node-RED remains responsible for validating the lecturer UID.
  if (
    responseRole.equalsIgnoreCase("LECTURER") &&
    responseAccess.equalsIgnoreCase("GRANTED")
  ) {
    lecturerFullAccess();
    return;
  }

  // STUDENT ACCESS - ORIGINAL LOGIC
  if (responseAccess != "GRANTED") {
    showAccessDenied(responseReason);
    return;
  }

  if (responseClassState == "ONGOING") {

    int activeIndex = findActiveRecord(responseUid);

    if (activeIndex >= 0) {
      reEntry(activeIndex);
    } else {
      clockIn(responseUid);
    }

    return;
  }

  if (responseClassState == "ENDED") {

    int activeIndex = findActiveRecord(responseUid);

    if (activeIndex >= 0) {
      clockOut(activeIndex);
    } else {
      showClassEndedNoRecord();
    }

    return;
  }

  showClassNotActive();
}



// =====================================================

// CLOCK IN

// =====================================================

// =====================================================
// LECTURER FULL ACCESS
// =====================================================

void lecturerFullAccess() {
  Serial.println("LECTURER FULL ACCESS GRANTED");
  Serial.print("Lecturer: ");
  Serial.println(responseStudent);
  Serial.print("LAB: ");
  Serial.println(LAB_ID);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(RED_LED, LOW);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LECTURER ACCESS");
  lcd.setCursor(0, 1);
  lcd.print("FULL ACCESS GRANTED");
  lcd.setCursor(0, 2);
  lcd.print(responseStudent.substring(0, 20));
  lcd.setCursor(0, 3);
  lcd.print(LAB_ID);

  delay(3000);

  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SYSTEM READY");
  lcd.setCursor(0, 1);
  lcd.print(LAB_ID);
  lcd.setCursor(0, 2);
  lcd.print("SCAN RFID");
}


void clockIn(String uid) {



  int index = findFreeRecord();



  if (index < 0) {

    showAccessDenied("MAX ACTIVE STUDENTS");

    return;

  }



  records[index].active = true;

  records[index].uid = uid;

  records[index].student = responseStudent;

  records[index].semester = responseSemester;

  records[index].subject = responseSubject;

  records[index].className = responseClassName;

  records[index].entryDate = getDate();

  records[index].entryTime = getTime();

  records[index].entryEpoch = time(nullptr);



  Serial.println("CLOCK-IN");

  Serial.print("Student: ");

  Serial.println(records[index].student);

  Serial.print("Entry: ");

  Serial.println(records[index].entryTime);



  digitalWrite(GREEN_LED, HIGH);

  digitalWrite(RED_LED, LOW);

  digitalWrite(RELAY_PIN, LOW);



  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("WELCOME");

  lcd.setCursor(0, 1);

  lcd.print(records[index].student);

  lcd.setCursor(0, 2);

  lcd.print("ENTRY: ");

  lcd.print(records[index].entryTime);

  lcd.setCursor(0, 3);

  lcd.print("PRESENT");



  publishAttendanceEvent(

    "ENTRY",

    records[index].uid,

    records[index].student,

    records[index].semester,

    records[index].subject,

    records[index].className,

    "PRESENT",

    records[index].entryTime,

    "-",

    "ACTIVE"

  );



  delay(3000);



  digitalWrite(RELAY_PIN, HIGH);

  digitalWrite(GREEN_LED, LOW);

  digitalWrite(RED_LED, HIGH);



  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("CLASS ACTIVE");

  lcd.setCursor(0, 1);

  lcd.print("PRESENT");

  lcd.setCursor(0, 2);

  lcd.print("PRESS EXIT TO LEAVE");

}



// =====================================================

// RE-ENTRY

// =====================================================



void reEntry(int index) {



  digitalWrite(GREEN_LED, HIGH);

  digitalWrite(RED_LED, LOW);

  digitalWrite(RELAY_PIN, LOW);



  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("RE-ENTRY");

  lcd.setCursor(0, 1);

  lcd.print(records[index].student);

  lcd.setCursor(0, 2);

  lcd.print("ATTENDANCE ACTIVE");



  publishAttendanceEvent(

    "RE_ENTRY",

    records[index].uid,

    records[index].student,

    records[index].semester,

    records[index].subject,

    records[index].className,

    "PRESENT",

    records[index].entryTime,

    "-",

    "ACTIVE"

  );



  delay(3000);



  digitalWrite(RELAY_PIN, HIGH);

  digitalWrite(GREEN_LED, LOW);

  digitalWrite(RED_LED, HIGH);



  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("CLASS ACTIVE");

  lcd.setCursor(0, 1);

  lcd.print("ATTENDANCE OK");

}



// =====================================================

// CLOCK OUT

// =====================================================



void clockOut(int index) {



  String exitTime = getTime();

  time_t exitEpoch = time(nullptr);



  unsigned long durationSeconds = 0;



  if (exitEpoch >= records[index].entryEpoch) {

    durationSeconds =

      (unsigned long)(exitEpoch - records[index].entryEpoch);

  }



  String duration = formatDuration(durationSeconds);



  Serial.println("OFFICIAL CLOCK-OUT");

  Serial.print("Student: ");

  Serial.println(records[index].student);

  Serial.print("Entry: ");

  Serial.println(records[index].entryTime);

  Serial.print("Exit: ");

  Serial.println(exitTime);

  Serial.print("Duration: ");

  Serial.println(duration);



  digitalWrite(RELAY_PIN, LOW);

  digitalWrite(GREEN_LED, HIGH);

  digitalWrite(RED_LED, LOW);



  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("CLOCK OUT");

  lcd.setCursor(0, 1);

  lcd.print(records[index].student);

  lcd.setCursor(0, 2);

  lcd.print("EXIT: ");

  lcd.print(exitTime);

  lcd.setCursor(0, 3);

  lcd.print("D:");

  lcd.print(duration);



  publishAttendanceEvent(

    "EXIT",

    records[index].uid,

    records[index].student,

    records[index].semester,

    records[index].subject,

    records[index].className,

    "COMPLETED",

    records[index].entryTime,

    exitTime,

    duration

  );



  resetRecord(index);



  delay(2500);

  digitalWrite(RELAY_PIN, HIGH);

  digitalWrite(GREEN_LED, LOW);

  digitalWrite(RED_LED, HIGH);



  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("CLASS ENDED");

  lcd.setCursor(0, 1);

  lcd.print("SCAN NEXT STUDENT");

}



// =====================================================

// EXIT BUTTON

// =====================================================



void handleExitButton() {



  Serial.println();

  Serial.println("EXIT BUTTON");

  Serial.println("Temporary exit - attendance unchanged");



  digitalWrite(RELAY_PIN, LOW);

  digitalWrite(GREEN_LED, HIGH);

  digitalWrite(RED_LED, LOW);



  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("EXIT DOOR OPEN");

  lcd.setCursor(0, 1);

  lcd.print("TEMPORARY EXIT");

  lcd.setCursor(0, 2);

  lcd.print("NO ATTENDANCE");

  lcd.setCursor(0, 3);

  lcd.print("CHANGE");



  publishAttendanceEvent(

    "EXIT_BUTTON",

    "-",

    "DOOR EXIT",

    "-",

    "-",

    "-",

    "CLASS ACTIVE",

    "-",

    "-",

    "ACTIVE"

  );



  delay(3000);



  digitalWrite(RELAY_PIN, HIGH);

  digitalWrite(GREEN_LED, LOW);

  digitalWrite(RED_LED, HIGH);



  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("CLASS ACTIVE");

  lcd.setCursor(0, 1);

  lcd.print("SCAN RFID TO ENTER");

}



// =====================================================

// DISPLAY STATES

// =====================================================



void showClassNotActive() {



  digitalWrite(RELAY_PIN, HIGH);

  digitalWrite(GREEN_LED, LOW);

  digitalWrite(RED_LED, HIGH);



  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("CLASS NOT ACTIVE");



  if (responseClassStart != "" &&

      responseClassEnd != "") {

    lcd.setCursor(0, 1);

    lcd.print(responseClassStart);

    lcd.print(" - ");

    lcd.print(responseClassEnd);

  }



  publishAttendanceEvent(

    "ACCESS_DENIED",

    responseUid,

    responseStudent,

    responseSemester,

    responseSubject,

    responseClassName,

    "ABSENT",

    "-",

    "-",

    "00:00:00"

  );



  delay(1800);



  digitalWrite(RED_LED, LOW);



  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("SCAN RFID");

}



void showClassEndedNoRecord() {



  digitalWrite(RELAY_PIN, HIGH);

  digitalWrite(GREEN_LED, LOW);

  digitalWrite(RED_LED, HIGH);



  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("CLASS ENDED");

  lcd.setCursor(0, 1);

  lcd.print("NO ACTIVE RECORD");



  delay(1800);



  digitalWrite(RED_LED, LOW);



  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("CLASS ENDED");

}



void showAccessDenied(String reason) {



  digitalWrite(RELAY_PIN, HIGH);

  digitalWrite(GREEN_LED, LOW);

  digitalWrite(RED_LED, HIGH);



  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("ACCESS DENIED");



  lcd.setCursor(0, 1);

  if (reason == "") {

    lcd.print("NOT AUTHORIZED");

  } else {

    lcd.print(reason.substring(0, 20));

  }



  publishAttendanceEvent(

    "ACCESS_DENIED",

    responseUid,

    responseStudent,

    responseSemester,

    responseSubject,

    responseClassName,

    "ABSENT",

    "-",

    "-",

    "00:00:00"

  );



  delay(1800);



  digitalWrite(RED_LED, LOW);



  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("SCAN RFID");

}



void showCommunicationError() {



  digitalWrite(RELAY_PIN, HIGH);

  digitalWrite(GREEN_LED, LOW);

  digitalWrite(RED_LED, HIGH);



  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("COMMUNICATION");

  lcd.setCursor(0, 1);

  lcd.print("ERROR");

  lcd.setCursor(0, 2);

  lcd.print("NODE-RED TIMEOUT");



  delay(1800);



  digitalWrite(RED_LED, LOW);



  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("SCAN RFID");

}



// =====================================================

// ACTIVE RECORD LOOKUP

// =====================================================



int findActiveRecord(String uid) {



  for (int i = 0; i < MAX_ACTIVE_STUDENTS; i++) {



    if (records[i].active &&

        records[i].uid.equalsIgnoreCase(uid)) {

      return i;

    }

  }



  return -1;

}



int findFreeRecord() {



  for (int i = 0; i < MAX_ACTIVE_STUDENTS; i++) {



    if (!records[i].active) {

      return i;

    }

  }



  return -1;

}



void resetRecord(int index) {



  records[index].active = false;

  records[index].uid = "";

  records[index].student = "";

  records[index].semester = "";

  records[index].subject = "";

  records[index].className = "";

  records[index].entryDate = "";

  records[index].entryTime = "";

  records[index].entryEpoch = 0;

}



// =====================================================

// MQTT ATTENDANCE EVENT

// =====================================================



void publishAttendanceEvent(

  String type,

  String uid,

  String student,

  String semester,

  String subject,

  String className,

  String attendance,

  String entry,

  String exit,

  String duration

) {



  if (!mqttClient.connected()) {

    Serial.println("MQTT disconnected - event not sent.");

    return;

  }



  String payload = "{";



  payload += "\"type\":\"ATTENDANCE_EVENT\",";

  payload += "\"event\":\"" + type + "\",";

  payload += "\"lab\":\"" + String(LAB_ID) + "\",";

  payload += "\"uid\":\"" + uid + "\",";

  payload += "\"student\":\"" + student + "\",";

  payload += "\"semester\":\"" + semester + "\",";

  payload += "\"subject\":\"" + subject + "\",";

  payload += "\"className\":\"" + className + "\",";

  payload += "\"attendance\":\"" + attendance + "\",";

  payload += "\"date\":\"" + getDate() + "\",";

  payload += "\"entry_time\":\"" + entry + "\",";

  payload += "\"exit_time\":\"" + exit + "\",";

  payload += "\"duration\":\"" + duration + "\"";



  payload += "}";



  mqttClient.publish(

    MQTT_STATUS_TOPIC,

    payload.c_str()

  );



  Serial.print("MQTT EVENT: ");

  Serial.println(payload);

}



// =====================================================

// WIFI

// =====================================================



void connectWiFi() {



  Serial.println("Connecting WiFi...");



  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("CONNECTING WIFI");



  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);



  int counter = 0;



  while (WiFi.status() != WL_CONNECTED) {



    delay(500);

    Serial.print(".");



    counter++;



    if (counter > 30) {



      Serial.println();

      Serial.println("WiFi connection failed");



      lcd.clear();

      lcd.setCursor(0, 0);

      lcd.print("WIFI FAILED");



      delay(1500);

      return;

    }

  }



  Serial.println();

  Serial.println("WiFi Connected");

  Serial.print("IP: ");

  Serial.println(WiFi.localIP());



  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("WIFI CONNECTED");



  delay(1000);

}



// =====================================================

// NTP

// =====================================================



void setupTime() {



  configTime(

    GMT_OFFSET_SEC,

    DAYLIGHT_OFFSET_SEC,

    "pool.ntp.org",

    "time.nist.gov"

  );



  struct tm timeinfo;

  int retry = 0;



  while (!getLocalTime(&timeinfo) &&

         retry < 20) {



    Serial.println("Waiting NTP...");

    delay(500);

    retry++;

  }



  if (getLocalTime(&timeinfo)) {



    Serial.print("Malaysia Time: ");

    Serial.println(getTime());



  } else {



    Serial.println("NTP failed");

  }

}



// =====================================================

// MQTT CONNECT

// =====================================================



void connectMQTT() {



  if (WiFi.status() != WL_CONNECTED) {

    return;

  }



  Serial.println("Connecting HiveMQ...");



  String clientID = "ESP32_";

  clientID += String(LAB_ID);

  clientID += "_";

  clientID += String((uint32_t)ESP.getEfuseMac(), HEX);



  if (mqttClient.connect(

        clientID.c_str(),

        MQTT_USERNAME,

        MQTT_PASSWORD

      )) {



    Serial.println("HiveMQ Connected");



    mqttClient.subscribe(

      MQTT_RESPONSE_TOPIC

    );



    lcd.clear();

    lcd.setCursor(0, 0);

    lcd.print("MQTT CONNECTED");

    lcd.setCursor(0, 1);

    lcd.print(LAB_ID);



    delay(1000);



  } else {



    Serial.print("MQTT failed state=");

    Serial.println(mqttClient.state());



    delay(1000);

  }

}



// =====================================================

// JSON HELPERS

// Simple parser for controlled Node-RED JSON response.

// =====================================================



String jsonGetString(String json, String key) {
  // Accept JSON with or without spaces around the colon:
  // "role":"LECTURER" OR "role" : "LECTURER"
  String search = "\"" + key + "\"";
  int keyStart = json.indexOf(search);
  if (keyStart < 0) return "";

  int colon = json.indexOf(':', keyStart + search.length());
  if (colon < 0) return "";

  int valueStart = colon + 1;
  while (valueStart < json.length() &&
         (json[valueStart] == ' ' || json[valueStart] == '\t' ||
          json[valueStart] == '\r' || json[valueStart] == '\n')) {
    valueStart++;
  }

  if (valueStart >= json.length() || json[valueStart] != '\"') return "";
  valueStart++;

  String value = "";
  bool escaped = false;
  for (int i = valueStart; i < json.length(); i++) {
    char c = json[i];
    if (escaped) {
      if (c == 'n') value += '\n';
      else if (c == 'r') value += '\r';
      else if (c == 't') value += '\t';
      else value += c;
      escaped = false;
    } else if (c == '\\') {
      escaped = true;
    } else if (c == '\"') {
      value.trim();
      return value;
    } else {
      value += c;
    }
  }

  return "";
}

bool jsonGetBool(

  String json,

  String key,

  bool defaultValue

) {



  String search = "\"" + key + "\":";



  int start = json.indexOf(search);



  if (start < 0) {

    return defaultValue;

  }



  start += search.length();



  if (json.startsWith("true", start)) {

    return true;

  }



  if (json.startsWith("false", start)) {

    return false;

  }



  return defaultValue;

}



void clearResponse() {



  responseReceived = false;



  responseRequestId = "";



  responseUid = "";

  responseStudent = "";

  responseSemester = "";

  responseStatus = "";

  responseRole = "";

  responseAccess = "";

  responseReason = "";

  responseClassState = "";

  responseSubject = "";

  responseClassName = "";

  responseClassStart = "";

  responseClassEnd = "";

}



// =====================================================

// UID

// =====================================================



String getUID() {



  String uid = "";



  for (byte i = 0; i < rfid.uid.size; i++) {



    uid += String(

      rfid.uid.uidByte[i],

      HEX

    );

  }



  uid.toLowerCase();



  return uid;

}



// =====================================================

// DATE

// =====================================================



String getDate() {



  struct tm timeinfo;



  if (!getLocalTime(&timeinfo)) {

    return "0000-00-00";

  }



  char buffer[11];



  strftime(

    buffer,

    sizeof(buffer),

    "%Y-%m-%d",

    &timeinfo

  );



  return String(buffer);

}



// =====================================================

// TIME

// =====================================================



String getTime() {



  struct tm timeinfo;



  if (!getLocalTime(&timeinfo)) {

    return "00:00:00";

  }



  char buffer[9];



  strftime(

    buffer,

    sizeof(buffer),

    "%H:%M:%S",

    &timeinfo

  );



  return String(buffer);

}



// =====================================================

// DURATION

// =====================================================



String formatDuration(

  unsigned long seconds

) {



  unsigned long hours =

    seconds / 3600;



  unsigned long minutes =

    (seconds % 3600) / 60;



  unsigned long secs =

    seconds % 60;



  char buffer[12];



  snprintf(

    buffer,

    sizeof(buffer),

    "%02lu:%02lu:%02lu",

    hours,

    minutes,

    secs

  );



  return String(buffer);

}
