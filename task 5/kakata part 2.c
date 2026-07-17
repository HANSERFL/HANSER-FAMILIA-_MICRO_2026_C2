#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// --- CONFIGURACIÓN DE PANTALLA ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- CONFIGURACIÓN RED Y MQTT ---
const char* ssid = "TU_SSID_WIFI";
const char* password = "TU_PASSWORD_WIFI";
const char* mqtt_server = "broker.hivemq.com"; // Broker público de prueba
const int mqtt_port = 1883;
const char* mqtt_topic = "kakata433/telemetry";

WiFiClient espClient;
PubSubClient mqttClient(espClient);
unsigned long lastMqttPublish = 0;
const long publishInterval = 100; // Publicar cada 100ms (10Hz)

// --- INSTANCIA IMU MPU6050 ---
Adafruit_MPU6050 mpu;

// --- CONFIGURACIÓN DE PINES ---
// Joysticks (Entradas Analógicas)
#define JOY1_X 34
#define JOY1_Y 35
#define JOY2_X 32
#define JOY2_Y 33

// Botones Gatillos (Pull-up interno)
#define GATILLO_L1 13
#define GATILLO_R1 12
#define GATILLO_L2 14
#define GATILLO_R2 27

// Botones de Frente
#define BTN_F1 26
#define BTN_F2 25
#define BTN_F3 39
#define BTN_F4 15 

// --- VARIABLES GLOBALES DE ESTADO ---
// Variables mapeadas (-100 a +100)
int j1x = 0, j1y = 0;
int j2x = 0, j2y = 0;
int gyroX_mapped = 0, gyroY_mapped = 0;

// Valores crudos de acelerómetro
float accX = 0, accY = 0, accZ = 0;

// Estados de botones (0 = Suelto, 1 = Presionado)
bool g_l1 = 0, g_r1 = 0, g_l2 = 0, g_r2 = 0;
bool b_f1 = 0, b_f2 = 0, b_f3 = 0, b_f4 = 0;

// Variables de Calibración
int j1x_offset = 1800, j1y_offset = 1800; // Centros nominales (ADC 12-bits del ESP32)
int j2x_offset = 1800, j2y_offset = 1800;
float gyroX_offset = 0, gyroY_offset = 0;
unsigned long calibTimer = 0;

// --- DECLARACIÓN DE FUNCIONES PROTOTIPO ---
void setup_wifi();
void reconnectMqtt();
void calibrateSensors();
void readInputs();
void checkCalibrationTrigger();
void updateDisplay();
void publishTelemetry();

// --- VOID SETUP ---
void setup() {
  Serial.begin(115200);

  // Inicializar Pantalla OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("Error: No se pudo asignar memoria para SSD1306"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,20);
  display.println("  KAKATA-433 INIT...  ");
  display.display();

  // Configuración de Pines Digitales (Lógica invertida con Pull-up interno)
  pinMode(GATILLO_L1, INPUT_PULLUP);
  pinMode(GATILLO_R1, INPUT_PULLUP);
  pinMode(GATILLO_L2, INPUT_PULLUP);
  pinMode(GATILLO_R2, INPUT_PULLUP);
  pinMode(BTN_F1, INPUT_PULLUP);
  pinMode(BTN_F2, INPUT_PULLUP);
  pinMode(BTN_F3, INPUT_PULLUP);
  pinMode(BTN_F4, INPUT_PULLUP);

  // Inicializar MPU6050
  if (!mpu.begin()) {
    Serial.println("No se encontro el chip MPU6050");
    display.setCursor(0,40);
    display.println("ERROR: MPU6050");
    display.display();
    while (1) { delay(10); }
  }
  
  // Configuraciones óptimas para el control
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // Conexión inalámbrica y MQTT
  setup_wifi();
  mqttClient.setServer(mqtt_server, mqtt_port);
  
  // Calibración inicial al encender
  calibrateSensors();
}

// --- VOID LOOP ---
void loop() {
  // Mantener la conexión MQTT activa
  if (!mqttClient.connected()) {
    reconnectMqtt();
  }
  mqttClient.loop();

  // Procesar lecturas
  readInputs();
  checkCalibrationTrigger();

  // Renderizar la pantalla local
  updateDisplay();

  // Envío periódico por red inalámbrica
  unsigned long now = millis();
  if (now - lastMqttPublish > publishInterval) {
    lastMqttPublish = now;
    publishTelemetry();
  }
  
  delay(10); // Respiro para el procesador
}

// --- FUNCIONES COMPLEMENTARIAS ---

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  int timeout = 0;

  // Intenta conectar durante 10 segundos
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
  }
  
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Conectado con IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nModo de red fallido. Iniciando en modo Offline.");
  }
}

void reconnectMqtt() {
  static unsigned long lastReconnectAttempt = 0;
  unsigned long now = millis();
  
  // Reintento asíncrono cada 5 segundos para evitar congelar el control si se cae el Wi-Fi
  if (now - lastReconnectAttempt > 5000) {
    lastReconnectAttempt = now;
    Serial.print("Conectando a MQTT...");
    String clientId = "Kakata433-Client-";
    clientId += String(random(0xffff), HEX);
    
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("¡Conectado exitosamente!");
    } else {
      Serial.print("Error, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Reintentando en breve.");
    }
  }
}

void calibrateSensors() {
  display.clearDisplay();
  display.setCursor(0, 15);
  display.println("CALIBRANDO CONTROL...");
  display.println("Deje los mandos quietos");
  display.display();
  
  long sumJ1X = 0, sumJ1Y = 0, sumJ2X = 0, sumJ2Y = 0;
  float sumGyroX = 0, sumGyroY = 0;
  const int samples = 50;

  for (int i = 0; i < samples; i++) {
    sumJ1X += analogRead(JOY1_X);
    sumJ1Y += analogRead(JOY1_Y);
    sumJ2X += analogRead(JOY2_X);
    sumJ2Y += analogRead(JOY2_Y);
    
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    sumGyroX += g.gyro.x;
    sumGyroY += g.gyro.y;
    delay(20);
  }

  // Establecer los puntos cero reales del hardware actual
  j1x_offset = sumJ1X / samples;
  j1y_offset = sumJ1Y / samples;
  j2x_offset = sumJ2X / samples;
  j2y_offset = sumJ2Y / samples;
  gyroX_offset = sumGyroX / samples;
  gyroY_offset = sumGyroY / samples;

  display.clearDisplay();
  display.setCursor(0, 20);
  display.println("  ¡CALIBRADO OK!  ");
  display.display();
  delay(1000);
}

void readInputs() {
  // 1. Lectura de Joysticks
  int rawJ1X = analogRead(JOY1_X);
  int rawJ1Y = analogRead(JOY1_Y);
  int rawJ2X = analogRead(JOY2_X);
  int rawJ2Y = analogRead(JOY2_Y);

  // Escalar tramos basándose en el punto de reposo dinámico calibrado -> objetivo [-100, 100]
  j1x = (rawJ1X >= j1x_offset) ? map(rawJ1X, j1x_offset, 4095, 0, 100) : map(rawJ1X, 0, j1x_offset, -100, 0);
  j1y = (rawJ1Y >= j1y_offset) ? map(rawJ1Y, j1y_offset, 4095, 0, 100) : map(rawJ1Y, 0, j1y_offset, -100, 0);
  j2x = (rawJ2X >= j2x_offset) ? map(rawJ2X, j2x_offset, 4095, 0, 100) : map(rawJ2X, 0, j2x_offset, -100, 0);
  j2y = (rawJ2Y >= j2y_offset) ? map(rawJ2Y, j2y_offset, 4095, 0, 100) : map(rawJ2Y, 0, j2y_offset, -100, 0);

  // Zona muerta matemática central (elimina oscilaciones sutiles en 0)
  if (abs(j1x) < 7) j1x = 0; if (abs(j1y) < 7) j1y = 0;
  if (abs(j2x) < 7) j2x = 0; if (abs(j2y) < 7) j2y = 0;

  // 2. Lectura del Giroscopio e Inerciales
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  accX = a.acceleration.x;
  accY = a.acceleration.y;
  accZ = a.acceleration.z;

  float cleanGyroX = g.gyro.x - gyroX_offset;
  float cleanGyroY = g.gyro.y - gyroY_offset;
  
  // Mapeamos asumiendo rotaciones máximas útiles de control de ±3.14 Rad/seg (180°/s)
  gyroX_mapped = constrain(map(cleanGyroX * 100, -314, 314, -100, 100), -100, 100);
  gyroY_mapped = constrain(map(cleanGyroY * 100, -314, 314, -100, 100), -100, 100);

  // 3. Lectura de Botones (LOW significa presionado debido al INPUT_PULLUP)
  g_l1 = (digitalRead(GATILLO_L1) == LOW);
  g_r1 = (digitalRead(GATILLO_R1) == LOW);
  g_l2 = (digitalRead(GATILLO_L2) == LOW);
  g_r2 = (digitalRead(GATILLO_R2) == LOW);
  
  b_f1 = (digitalRead(BTN_F1) == LOW);
  b_f2 = (digitalRead(BTN_F2) == LOW);
  b_f3 = (digitalRead(BTN_F3) == LOW);
  b_f4 = (digitalRead(BTN_F4) == LOW);
}

void checkCalibrationTrigger() {
  // Gatillos L1 y R1 apretados simultáneamente por 3 segundos gatillan recalibración en caliente
  if (g_l1 && g_r1) {
    if (calibTimer == 0) {
      calibTimer = millis();
    } else if (millis() - calibTimer > 3000) {
      calibrateSensors();
      calibTimer = 0;
    }
  } else {
    calibTimer = 0;
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Línea 1: Estado del Sistema
  display.setCursor(0, 0);
  display.print("KAKATA-433 | MQTT:");
  display.print(mqttClient.connected() ? "OK" : "ERR");
  
  // Línea 2 y 3: Monitores Joysticks
  display.setCursor(0, 15);
  display.printf("J1 X:%4d Y:%4d", j1x, j1y);
  display.setCursor(0, 25);
  display.printf("J2 X:%4d Y:%4d", j2x, j2y);
  
  // Línea 4: Giroscopio procesado
  display.setCursor(0, 38);
  display.printf("G-X:%4d G-Y:%4d", gyroX_mapped, gyroY_mapped);
  
  // Línea 5: Estados rápidos de Botones activos
  display.setCursor(0, 52);
  display.print("BTN: ");
  bool algunActivo = false;
  if(g_l1) { display.print("L1 "); algunActivo = true; }
  if(g_r1) { display.print("R1 "); algunActivo = true; }
  if(b_f1) { display.print("F1 "); algunActivo = true; }
  if(b_f2) { display.print("F2 "); algunActivo = true; }
  if(!algunActivo) display.print("---");
  
  display.display();
}

void publishTelemetry() {
  StaticJsonDocument<400> doc;
  
  // Estructura JSON organizada
  JsonObject joy1 = doc.createNestedObject("joy1");
  joy1["x"] = j1x; joy1["y"] = j1y;
  
  JsonObject joy2 = doc.createNestedObject("joy2");
  joy2["x"] = j2x; joy2["y"] = j2y;

  JsonObject gyro = doc.createNestedObject("gyro");
  gyro["x"] = gyroX_mapped; gyro["y"] = gyroY_mapped;
  
  JsonObject accel = doc.createNestedObject("acc");
  accel["x"] = accX; accel["y"] = accY; accel["z"] = accZ;

  JsonObject btns = doc.createNestedObject("btn");
  btns["l1"] = g_l1; btns["f1"] = b_f1;
  btns["r1"] = g_r1; btns["f2"] = b_f2;
  btns["l2"] = g_l2; btns["f3"] = b_f3;
  btns["r2"] = g_r2; btns["f4"] = b_f4;

  char output[400];
  serializeJson(doc, output);
  
  if (mqttClient.connected()) {
    mqttClient.publish(mqtt_topic, output);
  }
}