#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

// ============================================
// AQUAALERTA - PROGRAMA INTEGRADO
// ESP32 + SENSOR + OLED + LEDS + API
// ============================================

// ============================================
// WIFI Y API
// ============================================

const char* WIFI_SSID = "TU_WIFI";
const char* WIFI_PASS = "TU_CLAVE_WIFI";

// IP del computador donde corre "npm start" (mismo WiFi que el ESP32)
const char* URL_API =
  "http://10.38.46.115:3000";

// NUMERO DE SENSOR de este ESP32: el mismo que escribes en la app
// (Registro o Perfil -> Sensores -> Editar -> "Numero de sensor").
// Cada ESP32 debe tener uno distinto; cada lectura se guarda en la cuenta
// que tenga este numero asignado.
const char* SENSOR_ID = "ESP32-001";

// OPCIONAL: API key del sensor (Perfil -> Sensores registrados). Si la
// pones, el servidor valida que coincida con SENSOR_ID. Dejala vacia ("")
// para identificarte solo con el numero de sensor. Si el servidor corre con
// REQUIRE_API_KEY=1 es obligatoria.
const char* SENSOR_API_KEY = "";

// Actividad que se guarda con cada lectura. Debe ser una de las que
// conoce la app: Ducha, Lavamanos, Sanitario, Lavado de platos,
// Preparar comida, Lavadora, Limpieza, Riego, Consumo general.
// Si el sensor es de "Tuberia general" usa "Consumo general".
const char* ACTIVIDAD = "Ducha";

const bool ENVIAR_API = true;

// ============================================
// PINES
// ============================================

#define SENSOR_PIN 27

#define LED_VERDE 25
#define LED_AMARILLO 26
#define LED_ROJO 33

#define SDA_OLED 21
#define SCL_OLED 22

// ============================================
// PANTALLA OLED SSD1306
// ============================================

#define ANCHO_OLED 128
#define ALTO_OLED 64
#define RESET_OLED -1

Adafruit_SSD1306 pantalla(
  ANCHO_OLED,
  ALTO_OLED,
  &Wire,
  RESET_OLED
);

bool oledConectada = false;
uint8_t direccionOLED = 0;

// ============================================
// SENSOR YF-S201
// ============================================

volatile uint32_t pulsos = 0;
volatile uint32_t ultimoPulsoUs = 0;

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

uint32_t tiempoAnterior = 0;
uint32_t tiempoInicio = 0;
uint32_t ultimaActividadMs = 0;

float caudalFiltrado = 0.0;

const float FACTOR_CALIBRACION = 7.5;
const float CAUDAL_MAXIMO = 30.0;

const uint32_t FILTRO_US = 2000;
const uint32_t TIEMPO_SIN_FLUJO = 2000;

// Litros calculados por pulsos
float volumenTotal = 0.0;
float volumenPendiente = 0.0;

// ============================================
// CONFIGURACION GENERAL
// ============================================

const uint32_t INTERVALO_LECTURA = 1000;
const uint32_t INTERVALO_WIFI = 10000;

// ============================================
// ENVIO A LA API
// ============================================
// Se envia UNA lectura por uso (ej. una ducha completa):
//  - cuando el agua lleva SESION_FIN_MS sin fluir, o
//  - cada FLUSH_MAX_MS si el uso es muy largo.
// Si falla el envio, los litros quedan pendientes y se reintenta.

const uint32_t SESION_FIN_MS = 5000;
const uint32_t FLUSH_MAX_MS = 60000;
const uint32_t REINTENTO_ENVIO_MS = 5000;
const float MIN_LITROS_ENVIO = 0.05f;

uint32_t ultimoEnvioMs = 0;
uint32_t ultimoIntentoEnvioMs = 0;
uint32_t ultimoIntentoWifiMs = 0;

// 0 = sin envios, 1 = ultimo OK, 2 = ultimo con error
int estadoApi = 0;

// ============================================
// SEMAFORO
// ============================================

const float UMBRAL_AMARILLO = 8.0;
const float UMBRAL_ROJO = 15.0;
const float HISTERESIS = 0.5;

// 0 = verde
// 1 = amarillo
// 2 = rojo

int nivel = 0;

// ============================================
// INTERRUPCION DEL SENSOR
// ============================================

void IRAM_ATTR contarPulso() {

  uint32_t ahora = micros();

  portENTER_CRITICAL_ISR(&mux);

  if (
    (uint32_t)(ahora - ultimoPulsoUs) >
    FILTRO_US
  ) {

    pulsos++;
    ultimoPulsoUs = ahora;
  }

  portEXIT_CRITICAL_ISR(&mux);
}

// ============================================
// DETECTAR DIRECCION I2C
// ============================================

bool respondeI2C(uint8_t direccion) {

  Wire.beginTransmission(direccion);

  return Wire.endTransmission() == 0;
}

// ============================================
// INICIAR PANTALLA OLED
// ============================================

void iniciarOLED() {

  Wire.begin(SDA_OLED, SCL_OLED);
  Wire.setClock(100000);

  delay(300);

  Serial.println("Buscando OLED...");

  if (respondeI2C(0x3C)) {

    direccionOLED = 0x3C;

  } else if (respondeI2C(0x3D)) {

    direccionOLED = 0x3D;

  } else {

    Serial.println("OLED no detectada");
    return;
  }

  Serial.print("OLED encontrada: 0x");
  Serial.println(direccionOLED, HEX);

  if (!pantalla.begin(
    SSD1306_SWITCHCAPVCC,
    direccionOLED
  )) {

    Serial.println("Error al iniciar OLED");
    return;
  }

  oledConectada = true;

  pantalla.clearDisplay();
  pantalla.setTextColor(SSD1306_WHITE);

  pantalla.setTextSize(2);
  pantalla.setCursor(0, 4);
  pantalla.println("AQUA");

  pantalla.setCursor(0, 26);
  pantalla.println("ALERTA");

  pantalla.setTextSize(1);
  pantalla.setCursor(0, 53);
  pantalla.println("Iniciando...");

  pantalla.display();

  delay(1500);
}

// ============================================
// MOSTRAR INFORMACION EN OLED
// ============================================

void mostrarOLED(float caudal) {

  if (!oledConectada) return;

  const char* estado = "NORMAL";

  if (nivel == 1) {
    estado = "ALERTA";
  }

  if (nivel == 2) {
    estado = "CRITICO";
  }

  pantalla.clearDisplay();
  pantalla.setTextColor(SSD1306_WHITE);
  pantalla.setTextSize(1);

  // ENCABEZADO
  pantalla.setCursor(28, 0);
  pantalla.print("AQUAALERTA");

  pantalla.drawLine(
    0, 11, 127, 11, SSD1306_WHITE
  );

  // CAUDAL
  pantalla.setCursor(0, 15);
  pantalla.print("Caudal: ");
  pantalla.print(caudal, 2);
  pantalla.print(" L/m");

  // VOLUMEN TOTAL
  pantalla.setCursor(0, 27);
  pantalla.print("Litros: ");
  pantalla.print(volumenTotal, 2);
  pantalla.print(" L");

  // SEMAFORO
  pantalla.setCursor(0, 39);
  pantalla.print("Estado: ");
  pantalla.print(estado);

  pantalla.drawLine(
    0, 50, 127, 50, SSD1306_WHITE
  );

  // WIFI Y API
  pantalla.setCursor(0, 55);
  pantalla.print("WiFi:");
  pantalla.print(
    WiFi.status() == WL_CONNECTED ? "OK" : "--"
  );

  pantalla.print(" API:");

  if (!ENVIAR_API || estadoApi == 0) {
    pantalla.print("--");
  } else if (estadoApi == 1) {
    pantalla.print("OK");
  } else {
    pantalla.print("ERR");
  }

  pantalla.display();
}

// ============================================
// ACTUALIZAR SEMAFORO LED
// ============================================

void actualizarSemaforo(float caudal) {

  if (
    nivel < 2 &&
    caudal >= UMBRAL_ROJO
  ) {

    nivel = 2;

  } else if (
    nivel < 1 &&
    caudal >= UMBRAL_AMARILLO
  ) {

    nivel = 1;
  }

  if (
    nivel == 2 &&
    caudal < UMBRAL_ROJO - HISTERESIS
  ) {

    nivel = 1;
  }

  if (
    nivel == 1 &&
    caudal < UMBRAL_AMARILLO - HISTERESIS
  ) {

    nivel = 0;
  }

  digitalWrite(
    LED_VERDE,
    nivel == 0 ? HIGH : LOW
  );

  digitalWrite(
    LED_AMARILLO,
    nivel == 1 ? HIGH : LOW
  );

  digitalWrite(
    LED_ROJO,
    nivel == 2 ? HIGH : LOW
  );
}

// ============================================
// CONECTAR WIFI
// ============================================

void conectarWiFi() {

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.println("Conectando WiFi...");

  uint32_t inicio = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - inicio < 15000
  ) {

    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("\nWiFi conectado");

    Serial.print("IP ESP32: ");
    Serial.println(WiFi.localIP());

  } else {

    Serial.println("\nWiFi no conectado");
  }
}

// ============================================
// ENVIAR LECTURAS A AQUACONSCIENCE
// POST /api/readings  {device_id, liters, activity}
// ============================================

bool enviarDato(float litros) {

  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient http;

  http.setTimeout(5000);

  String url =
    String(URL_API) + "/api/readings";

  if (!http.begin(url)) {
    return false;
  }

  http.addHeader(
    "Content-Type",
    "application/json"
  );

  http.addHeader(
    "x-device-id",
    SENSOR_ID
  );

  if (strlen(SENSOR_API_KEY) > 0) {

    http.addHeader(
      "x-api-key",
      SENSOR_API_KEY
    );
  }

  String json =
    "{\"device_id\":\"" +
    String(SENSOR_ID) +
    "\",\"liters\":" +
    String(litros, 3) +
    ",\"activity\":\"" +
    String(ACTIVIDAD) +
    "\"}";

  Serial.println("Enviando:");
  Serial.println(json);

  int codigo = http.POST(json);

  Serial.print("Respuesta API: ");
  Serial.println(codigo);

  if (codigo > 0) {
    Serial.println(http.getString());
  } else {
    Serial.println(http.errorToString(codigo));
  }

  http.end();

  // La API responde 201 cuando guarda la lectura
  return codigo >= 200 && codigo < 300;
}

// ============================================
// SETUP
// ============================================

void setup() {

  Serial.begin(115200);
  delay(500);

  // LEDS

  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AMARILLO, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);

  // PRUEBA INICIAL LEDS

  digitalWrite(LED_VERDE, HIGH);
  delay(300);
  digitalWrite(LED_VERDE, LOW);

  digitalWrite(LED_AMARILLO, HIGH);
  delay(300);
  digitalWrite(LED_AMARILLO, LOW);

  digitalWrite(LED_ROJO, HIGH);
  delay(300);
  digitalWrite(LED_ROJO, LOW);

  // OLED

  iniciarOLED();

  // SENSOR
  // Conservamos INPUT_PULLUP del codigo anterior
  // Solo usar si la senal en GPIO27 es de 3.3 V
  // y el circuito electrico es compatible.

  pinMode(SENSOR_PIN, INPUT_PULLUP);

  attachInterrupt(
    digitalPinToInterrupt(SENSOR_PIN),
    contarPulso,
    FALLING
  );

  // WIFI

  conectarWiFi();

  // INICIALIZAR TIEMPOS

  tiempoAnterior = millis();
  tiempoInicio = millis();
  ultimaActividadMs = millis();

  ultimoEnvioMs = millis();
  ultimoIntentoEnvioMs = millis();
  ultimoIntentoWifiMs = millis();

  actualizarSemaforo(0.0);
  mostrarOLED(0.0);

  Serial.println("AquaAlerta iniciado");
}

// ============================================
// LOOP PRINCIPAL
// ============================================

void loop() {

  uint32_t ahora = millis();

  // ==========================================
  // RECONEXION WIFI
  // ==========================================

  if (
    WiFi.status() != WL_CONNECTED &&
    ahora - ultimoIntentoWifiMs >=
    INTERVALO_WIFI
  ) {

    ultimoIntentoWifiMs = ahora;

    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }

  // ==========================================
  // LECTURA DEL SENSOR CADA SEGUNDO
  // ==========================================

  if (
    ahora - tiempoAnterior >=
    INTERVALO_LECTURA
  ) {

    float segundos =
      (ahora - tiempoAnterior) / 1000.0f;

    tiempoAnterior = ahora;

    // LEER PULSOS DE FORMA SEGURA

    portENTER_CRITICAL(&mux);

    uint32_t lectura = pulsos;
    pulsos = 0;

    portEXIT_CRITICAL(&mux);

    // IGNORAR LOS PRIMEROS 3 SEGUNDOS

    if (ahora - tiempoInicio < 3000) {

      caudalFiltrado = 0.0;

      actualizarSemaforo(0.0);
      mostrarOLED(0.0);

      Serial.println(
        "Inicializando sensor..."
      );

    } else {

      // ======================================
      // FRECUENCIA Y CAUDAL
      // ======================================

      float frecuencia =
        lectura / segundos;

      float caudalInstantaneo =
        frecuencia / FACTOR_CALIBRACION;

      // REGISTRAR ACTIVIDAD

      if (lectura > 0) {

        ultimaActividadMs = ahora;
      }

      // DESCARTAR PICOS FUERA DE RANGO

      if (
        caudalInstantaneo >
        CAUDAL_MAXIMO
      ) {

        Serial.println(
          "Lectura fuera de rango"
        );

        caudalInstantaneo = 0.0;
        lectura = 0;
      }

      // ======================================
      // SUAVIZADO ORIGINAL DEL SENSOR
      // ======================================

      if (caudalInstantaneo > 0.0) {

        if (caudalFiltrado == 0.0) {

          caudalFiltrado =
            caudalInstantaneo;

        } else {

          caudalFiltrado =
            0.25f * caudalInstantaneo +
            0.75f * caudalFiltrado;
        }
      }

      // DECLARAR CERO SIN PULSOS

      if (
        ahora - ultimaActividadMs >=
        TIEMPO_SIN_FLUJO
      ) {

        caudalFiltrado = 0.0;
      }

      // ======================================
      // CALCULAR LITROS CON PULSOS
      // ======================================

      // 7.5 Hz por L/min equivale a
      // aproximadamente 450 pulsos por litro

      float litrosNuevos =
        lectura /
        (FACTOR_CALIBRACION * 60.0f);

      volumenTotal += litrosNuevos;
      volumenPendiente += litrosNuevos;

      // ======================================
      // LEDS Y OLED
      // ======================================

      actualizarSemaforo(caudalFiltrado);

      mostrarOLED(caudalFiltrado);

      // ======================================
      // MONITOR SERIE
      // ======================================

      Serial.print("Pulsos: ");
      Serial.print(lectura);

      Serial.print(" | Caudal: ");
      Serial.print(caudalFiltrado, 2);

      Serial.print(" L/min | Total: ");
      Serial.print(volumenTotal, 3);

      Serial.print(" L | Pendiente: ");
      Serial.print(volumenPendiente, 3);

      Serial.println(" L");
    }
  }

  // ==========================================
  // ENVIO A LA API (una lectura por uso)
  // ==========================================

  if (ENVIAR_API) {

    // Sin nada pendiente, el reloj del "uso largo"
    // arranca cuando entra el primer litro nuevo.
    if (volumenPendiente <= 0.0f) {

      ultimoEnvioMs = ahora;

    } else if (
      volumenPendiente >= MIN_LITROS_ENVIO &&
      ahora - ultimoIntentoEnvioMs >=
      REINTENTO_ENVIO_MS
    ) {

      bool usoTerminado =
        ahora - ultimaActividadMs >=
        SESION_FIN_MS;

      bool usoLargo =
        ahora - ultimoEnvioMs >=
        FLUSH_MAX_MS;

      if (usoTerminado || usoLargo) {

        ultimoIntentoEnvioMs = ahora;

        // Se envia una copia y se descuenta solo
        // lo enviado, por si entra agua mientras
        // se hace la peticion HTTP.
        float aEnviar = volumenPendiente;

        if (enviarDato(aEnviar)) {

          volumenPendiente -= aEnviar;

          if (volumenPendiente < 0.0f) {
            volumenPendiente = 0.0f;
          }

          ultimoEnvioMs = ahora;
          estadoApi = 1;

          Serial.println(
            "Datos guardados en la API"
          );

        } else {

          estadoApi = 2;

          Serial.println(
            "Error: volumen pendiente, se reintenta"
          );
        }
      }
    }
  }
}
