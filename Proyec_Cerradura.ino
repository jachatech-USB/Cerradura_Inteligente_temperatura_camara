#include <WiFi.h>                 // Permite la conexión WiFi en el ESP32
#include <ESP_Mail_Client.h>      // Para enviar correos electrónicos
#include <time.h>                 // Manejo de hora y fecha con servidores NTP
#include <Keypad.h>               // Librería para leer teclados matriciales
#include <HTTPClient.h>           // Permite enviar solicitudes HTTP a un servidor
#include <ArduinoJson.h>          // Permite trabajar con datos en formato JSON
#include <Wire.h>                 // Comunicación I2C
#include <Adafruit_GFX.h>         // Librería base para gráficos en pantalla
#include <Adafruit_SSD1306.h>     // Controlador de pantallas OLED
#include <Adafruit_AMG88xx.h>     // Librería para el sensor térmico AMG8833
#include <vector>

// ======================== CONFIGURACIÓN OLED ========================
#define ANCHO_PANTALLA 128        // Ancho en píxeles
#define ALTO_PANTALLA 64          // Alto en píxeles
Adafruit_SSD1306 display(ANCHO_PANTALLA, ALTO_PANTALLA, &Wire, -1);  // Objeto pantalla

// ======================== CONFIGURACIÓN WIFI Y MAIL ========================
const char *WIFI_SSID = "prueba1";          // Nombre de red WiFi
const char *WIFI_PASSWORD = "12345678";    // Contraseña de WiFi

#define SMTP_HOST "smtp.gmail.com"          // Servidor de correo SMTP
#define SMTP_PORT 465                       // Puerto seguro SSL
#define AUTHOR_EMAIL "jacha.tech19@gmail.com"   // Correo del remitente
#define AUTHOR_PASSWORD "jlonmpqnxlxjhaey"      // Contraseña de aplicación de Gmail
#define RECIPIENT_EMAIL "hokal2712@gmail.com"   // Correo del destinatario

SMTPSession smtp;  // Objeto para la sesión de envío de correo

// ======================== CONFIGURACIÓN SERVIDOR FLASK ========================
const char* SERVER_URL = "http://192.168.223.242:5000/verificar";   // URL del servidor Flask

// ======================== RELE ========================
const int relePin = 13;     // Pin digital donde se conecta el relé
int estadorele = LOW;       // Estado inicial del relé (apagado)

// ======================== CONFIGURACIÓN DEL TECLADO ========================
const byte FILAS = 4;
const byte COLUMNAS = 4;
char teclas[FILAS][COLUMNAS] = {    // Mapa de teclas del teclado matricial
  { '1','2','3','A' },
  { '4','5','6','B' },
  { '7','8','9','C' },
  { '*','0','#','D' }
};
byte pinesFilas[FILAS] = { 19, 18, 5, 17 };        // Pines conectados a las filas
byte pinesColumnas[COLUMNAS] = { 16, 4, 26, 15 };  // Pines conectados a las columnas
Keypad teclado = Keypad(makeKeymap(teclas), pinesFilas, pinesColumnas, FILAS, COLUMNAS); // Objeto teclado

// ======================== SENSOR TERMICO AMG8833 ========================
// SDA = 27, SCL = 14
TwoWire I2C_AMG = TwoWire(1);     // Se crea un segundo bus I2C
Adafruit_AMG88xx amg;             // Objeto del sensor térmico
float pixels[64];                 // Arreglo para almacenar los 64 valores del sensor (8x8)

// ======================== VARIABLES ========================
String texto = "";                // Guarda los caracteres de la clave ingresada

// ======================== PROTOTIPOS ========================
void sendEmail(const String &subject, const String &messageBody);
void smtpCallback(SMTP_Status status);
bool verificarContrasena(String contrasena, bool &accesoCorrecto, String &nombreUsuario);
void mostrarInicio();
void mostrarContrasena(String texto);
void mostrarMensaje(const String &mensaje);
void mostrarMensajeingreso(const String &mensaje);
void mostrarMensajedenegado(const String &mensaje);
bool verificarTemperatura();

// ======================== SETUP ========================
void setup() {
  Serial.begin(115200);     // Inicia comunicación serial
  delay(100);

  // Inicializar OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("No se pudo inicializar la pantalla OLED");
    while (true);  // Se detiene el programa si falla la pantalla
  }
  mostrarInicio();  // Muestra mensaje de inicio

  // Conexión a WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(400);
  }
  Serial.println();
  Serial.print("WiFi conectada. IP: ");
  Serial.println(WiFi.localIP());

  pinMode(relePin, OUTPUT);   // Configura el pin del relé como salida

  // Inicializar bus I2C del sensor térmico
  I2C_AMG.begin(27, 14, 400000);

  // Inicializar sensor térmico
  if (!amg.begin(0x69, &I2C_AMG)) {
    Serial.println("No se pudo inicializar el sensor AMG8833");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(5, 25);
    display.println("Error sensor termico");
    display.display();
    while (true);  // Se detiene si no se detecta el sensor
  }

  // Configurar hora y correo
  configTime(-4 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  MailClient.networkReconnect(true);
  smtp.debug(1);
  smtp.callback(smtpCallback);
}

// ======================== LOOP ========================
void loop() {
  char key = teclado.getKey();   // Lee una tecla
  if (key != NO_KEY) {           // Si se presionó alguna
    Serial.print("Tecla: ");
    Serial.println(key);

    if (key == 'D') {            // Tecla 'D' = enviar clave
      if (texto.length() > 0) {
        bool accesoCorrecto = false;
        String nombreUsuario = "";

        // Verificar contraseña con el servidor Flask
        if (verificarContrasena(texto, accesoCorrecto, nombreUsuario)) {
          if (accesoCorrecto) {
            // Mide temperatura corporal
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(10, 20);
            display.println("Acerque la muneca");
            display.setCursor(25, 35);
            display.println("al sensor...");
            display.display();

            delay(2000);

            bool temperaturaOK = verificarTemperatura();

            if (temperaturaOK) {  // Si la temperatura está en el rango permitido
              mostrarMensajeingreso("Bienvenido");
              digitalWrite(relePin, HIGH);   // Activa relé (abre puerta)
              estadorele = HIGH;
              delay(7000);                   // Mantiene la puerta abierta 7 segundos
              digitalWrite(relePin, LOW);    // Cierra la puerta
              estadorele = LOW;
            } else {  // Si la temperatura es alta
              mostrarMensajedenegado("Alta Temp");
              sendEmail("Acceso denegado", "Usuario: " + nombreUsuario + " tiene temperatura fuera del rango.");
            }
          } else {  // Clave incorrecta
            mostrarMensajedenegado("Clave Incorrecta");
            sendEmail("Clave incorrecta", "Intento fallido de acceso con clave: " + texto);
          }
        } else {
          mostrarMensaje("Error servidor");  // No hay respuesta del servidor
        }

        texto = "";        // Limpia la clave ingresada
        delay(3000);
        mostrarInicio();   // Vuelve a la pantalla inicial
      }
    }
    else if (key == 'C') {   // Tecla 'C' = borrar último dígito
      if (texto.length() > 0) texto.remove(texto.length() - 1);
      mostrarContrasena(texto);
    }
    else {                   // Cualquier otra tecla se agrega a la clave
      texto += key;
      mostrarContrasena(texto);
    }
  }
}

// ======================== FUNCIONES DE PANTALLA ========================
void mostrarInicio() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(10, 15);
  display.println("Ingrese");
  display.setCursor(15, 35);
  display.println("la clave");
  display.display();
}

void mostrarContrasena(String texto) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println("Clave:");
  display.setCursor(0, 30);
  for (int i = 0; i < texto.length(); i++) display.print("*");  // Muestra asteriscos por cada dígito
  display.display();
}

void mostrarMensaje(const String &mensaje) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 25);
  display.println(mensaje);
  display.display();
}

void mostrarMensajeingreso(const String &mensaje) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 25);
  display.println(mensaje);
  display.display();
}

void mostrarMensajedenegado(const String &mensaje) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(5, 25);
  display.println(mensaje);
  display.display();
}

// ======================== SENSOR TERMICO ========================
bool verificarTemperatura() {
  unsigned long inicio = millis();
  float suma = 0;
  int contador = 0;
  bool deteccionValida = false;

  Serial.println("Esperando temperatura > 30°C para iniciar medición...");

  // Esperar hasta detectar temperatura mayor a 30°C
  while (!deteccionValida) {
    amg.readPixels(pixels);
    float sumaFrame = 0;
    for (int i = 0; i < 64; i++) sumaFrame += pixels[i];
    float promedioFrame = sumaFrame / 64.0;

    if (promedioFrame > 30.0) {
      deteccionValida = true;
      Serial.println("Temperatura detectada, iniciando medición...");
      break;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(5, 25);
    display.println("Esperando...");
    display.display();
    delay(500);
  }

  // Toma mediciones durante 5 segundos
  inicio = millis();
  while (millis() - inicio < 5000) {
    amg.readPixels(pixels);
    float sumaFrame = 0;
    for (int i = 0; i < 64; i++) sumaFrame += pixels[i];
    float promedioFrame = sumaFrame / 64.0;
    suma += promedioFrame;
    contador++;

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 10);
    display.println("Midiendo temp...");
    display.setCursor(10, 30);
    display.print(promedioFrame, 1);
    display.println(" C");
    display.display();

    Serial.print("Frame: ");
    Serial.println(promedioFrame);
    delay(500);
  }

  float promedio = suma / contador;   // Promedio total de temperatura
  Serial.print("Promedio total: ");
  Serial.println(promedio);

  // Verifica si la temperatura está en el rango aceptado
  if (promedio >= 30.0 && promedio <= 37.5) {
    return true;   // Aprobado
  } else {
    return false;  // Denegado
  }
}

// ======================== SERVIDOR Y MAIL ========================
bool verificarContrasena(String contrasena, bool &accesoCorrecto, String &nombreUsuario) {
  if (WiFi.status() != WL_CONNECTED) return false;   // Si no hay WiFi, error
  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");

  // Enviar contraseña al servidor Flask en formato JSON
  String json = "{\"contraseña\":\"" + contrasena + "\"}";
  int httpResponseCode = http.POST(json);

  if (httpResponseCode == 200) {    // Si el servidor responde correctamente
    String payload = http.getString();
    StaticJsonDocument<200> doc;
    if (!deserializeJson(doc, payload)) {
      const char* resultado = doc["resultado"];
      if (strcmp(resultado, "correcto") == 0) {
        accesoCorrecto = true;
        nombreUsuario = String(doc["nombre"]);   // Guarda el nombre recibido
      } else {
        accesoCorrecto = false;
      }
      http.end();
      return true;
    }
  }
  http.end();
  return false;
}

// Enviar correo electrónico
void sendEmail(const String &subject, const String &messageBody) {
  ESP_Mail_Session session;
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;

  SMTP_Message message;
  message.sender.name = "ESP32";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = subject;
  message.addRecipient(F("Destinatario"), RECIPIENT_EMAIL);
  message.text.content = messageBody.c_str();
  MailClient.sendMail(&smtp, &message);
}

// Función callback del envío de correo
void smtpCallback(SMTP_Status status) {
  Serial.println(status.info());  // Muestra el resultado del envío
}
