#include <WiFi.h>                 
#include <ESP_Mail_Client.h>      
#include <time.h>                 
#include <Keypad.h>               
#include <HTTPClient.h>           
#include <ArduinoJson.h>          
#include <Wire.h>                 
#include <Adafruit_GFX.h>         
#include <Adafruit_SSD1306.h>     
#include <Adafruit_AMG88xx.h>     
#include <vector>
#include <base64.h>

// ======================== CONFIGURACIÓN OLED ========================
#define ANCHO_PANTALLA 128        
#define ALTO_PANTALLA 64          
Adafruit_SSD1306 display(ANCHO_PANTALLA, ALTO_PANTALLA, &Wire, -1);  

// ======================== CONFIGURACIÓN WIFI Y MAIL ========================
const char *WIFI_SSID = "prueba1";          
const char *WIFI_PASSWORD = "12345678";    

#define SMTP_HOST "smtp.gmail.com"          
#define SMTP_PORT 465                       
#define AUTHOR_EMAIL "jacha.tech19@gmail.com"   
#define AUTHOR_PASSWORD "jlonmpqnxlxjhaey"      
#define RECIPIENT_EMAIL "hokal2712@gmail.com"   

SMTPSession smtp;

// ======================== CONFIGURACIÓN SERVIDOR FLASK ========================
const char* SERVER_URL = "http://192.168.247.242:5000/verificar";   

// ======================== RELE ========================
const int relePin = 13;     
int estadorele = LOW;       

// ======================== CONFIGURACIÓN DEL TECLADO ========================
const byte FILAS = 4;
const byte COLUMNAS = 4;
char teclas[FILAS][COLUMNAS] = {    
  { '1','2','3','A' },
  { '4','5','6','B' },
  { '7','8','9','C' },
  { '*','0','#','D' }
};
byte pinesFilas[FILAS] = { 19, 18, 5, 17 };        
byte pinesColumnas[COLUMNAS] = { 16, 4, 26, 15 };  
Keypad teclado = Keypad(makeKeymap(teclas), pinesFilas, pinesColumnas, FILAS, COLUMNAS);

// ======================== SENSOR TERMICO AMG8833 ========================
// SDA = 27, SCL = 14
TwoWire I2C_AMG = TwoWire(1);     
Adafruit_AMG88xx amg;             
float pixels[64];                 

// ======================== VARIABLES ========================
String texto = "";                
const char* CAM_HOST = "192.168.247.190";
bool fotoLista = false;
std::vector<uint8_t> photoBuf;   
bool clearPhotoAfterSend = false; 

float temperaturaPromedioGlobal = 0.0;  
bool registroEnviado = false;  // Control para envío único

// ======================== PROTOTIPOS ========================
void sendEmail(const String &subject, const String &messageBody,
               const String &tipoRegistro = "", const String &usuario = "");
void smtpCallback(SMTP_Status status);
bool verificarContrasena(String contrasena, bool &accesoCorrecto, String &nombreUsuario);
void mostrarInicio();
void mostrarContrasena(String texto);
void mostrarMensaje(const String &mensaje);
void mostrarMensajeingreso(const String &mensaje);
void mostrarMensajedenegado(const String &mensaje);
bool verificarTemperatura(float &temperaturaPromedio);
bool fetchSnapshotFromCam(); 
bool enviarFotoABaseDeDatos(const String &tipoRegistro, const String &usuario, float temperatura);

// ======================== SETUP ========================
void setup() {
  Serial.begin(115200);     
  delay(100);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("No se pudo inicializar la pantalla OLED");
    while (true);  
  }
  mostrarInicio();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(400);
  }
  Serial.println();
  Serial.print("WiFi conectada. IP: ");
  Serial.println(WiFi.localIP());

  pinMode(relePin, OUTPUT);

  I2C_AMG.begin(27, 14, 400000);

  if (!amg.begin(0x69, &I2C_AMG)) {
    Serial.println("No se pudo inicializar el sensor AMG8833");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(5, 25);
    display.println("Error sensor termico");
    display.display();
    while (true);  
  }

  configTime(-4 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  MailClient.networkReconnect(true);
  smtp.debug(1);
  smtp.callback(smtpCallback);
}

// ======================== LOOP ========================
void loop() {
  char key = teclado.getKey();   
  if (key != NO_KEY) {           
    Serial.print("Tecla: ");
    Serial.println(key);

    if (key == 'D') {  
      registroEnviado = false;  // Reiniciar control en nuevo intento          
      if (texto.length() > 0) {
        if (registroEnviado) {  // Ya se envió un registro, ignorar
          Serial.println("Registro ya enviado, esperar nuevo intento.");
          return;
        }

        bool accesoCorrecto = false;
        String nombreUsuario = "";

        if (verificarContrasena(texto, accesoCorrecto, nombreUsuario)) {
          if (!accesoCorrecto) {
            mostrarMensajedenegado("Clave Incorrecta");

            Serial.println("Tomando foto por clave incorrecta...");
            if (fetchSnapshotFromCam()) {
              fotoLista = true;
              
              // SOLO enviar registro si hay foto lista
              if (!registroEnviado && fotoLista) {
                enviarFotoABaseDeDatos("incorrecta", "desconocido", 0.0);
                registroEnviado = true;
              }
              sendEmail("⚠ Alerta: clave incorrecta",
                        "Se detectó un intento fallido de acceso.\nClave ingresada: " + texto,
                        "incorrecta",
                        "desconocido");
            } else {
              // NO enviar registro, dado que no hay foto
              Serial.println("No se tomó foto, no se envía registro.");
              sendEmail("⚠ Alerta: clave incorrecta (sin foto)",
                        "Se detectó un intento fallido de acceso.\nNo se pudo obtener foto.",
                        "",
                        "");
            }
            texto = "";
            delay(3000);
            mostrarInicio();
            return;
          }

          // Contraseña correcta: solicitar temperatura
          display.clearDisplay();
          display.setTextSize(1);
          display.setTextColor(SSD1306_WHITE);
          display.setCursor(10, 20);
          display.println("Acerque la muneca");
          display.setCursor(25, 35);
          display.println("al sensor...");
          display.display();

          delay(2000);

          bool temperaturaOK = verificarTemperatura(temperaturaPromedioGlobal);

          Serial.println("Tomando foto para registro correcto...");
          if (fetchSnapshotFromCam()) {
            fotoLista = true;

            if (!registroEnviado && fotoLista) {
              enviarFotoABaseDeDatos("ingreso", nombreUsuario, temperaturaPromedioGlobal);
              registroEnviado = true;
            }
            if (temperaturaOK) {
              mostrarMensajeingreso("  Pase");

              sendEmail("✔ Registro de ingreso",
                        "Usuario: " + nombreUsuario + "\nIngreso autorizado con temperatura normal.",
                        "ingreso",
                        nombreUsuario);
              digitalWrite(relePin, HIGH);
              estadorele = HIGH;
              delay(7000);
              digitalWrite(relePin, LOW);
              estadorele = LOW;
            } else {
              mostrarMensajedenegado("  FIEBRE");
              enviarFotoABaseDeDatos("alta_temp", nombreUsuario, temperaturaPromedioGlobal);
              sendEmail("🔥 Alerta: temperatura alta",
                        "Usuario: " + nombreUsuario + "\nIngreso denegado por temperatura elevada.",
                        "alta_temp",
                        nombreUsuario);
            }
          } else {
            Serial.println("No se tomó foto, no se envía registro.");
            mostrarMensajedenegado("Error foto");
            sendEmail("🔥 Alerta: fallo en toma de foto",
                      "Usuario: " + nombreUsuario + "\nNo se pudo obtener foto para registro.",
                      "",
                      "");
          }
        } else {
          mostrarMensaje("Error servidor");
        }

        texto = "";
        delay(3000);
        mostrarInicio();
      }
    }
    else if (key == 'C') {   
      if (texto.length() > 0) texto.remove(texto.length() - 1);
      mostrarContrasena(texto);
    }
    else {                   
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
  for (int i = 0; i < texto.length(); i++) display.print("*");  
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
  display.setTextSize(2.4);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 25);
  display.println(mensaje);
  display.display();
}

void mostrarMensajedenegado(const String &mensaje) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 25);
  display.println(mensaje);
  display.display();
}

// ======================== SENSOR TERMICO ========================
bool verificarTemperatura(float &temperaturaPromedio) {
  unsigned long inicio = millis();
  float suma = 0;
  int contador = 0;
  bool deteccionValida = false;

  Serial.println("Esperando temperatura > 30°C para iniciar medición...");

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

  temperaturaPromedio = suma / contador;   
  Serial.print("Promedio total: ");
  Serial.println(temperaturaPromedio);

  return (temperaturaPromedio >= 30.0 && temperaturaPromedio <= 37.5);
}

// ======================== VERIFICAR CONTRASEÑA ========================
bool verificarContrasena(String contrasena, bool &accesoCorrecto, String &nombreUsuario) {
  if (WiFi.status() != WL_CONNECTED) return false;   
  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");

  String json = "{\"contraseña\":\"" + contrasena + "\"}";
  int httpResponseCode = http.POST(json);

  if (httpResponseCode == 200) {    
    String payload = http.getString();
    StaticJsonDocument<200> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
      const char* resultado = doc["resultado"];
      if (strcmp(resultado, "correcto") == 0) {
        accesoCorrecto = true;
        nombreUsuario = String((const char*)doc["nombre"]);   
      } else {
        accesoCorrecto = false;
      }
      http.end();
      return true;
    } else {
      Serial.print("Error parseando JSON: ");
      Serial.println(err.c_str());
    }
  } else {
    Serial.printf("HTTP POST fallo. Codigo: %d\n", httpResponseCode);
  }
  http.end();
  return false;
}

// ======================== ENVIAR CORREO ELECTRÓNICO ========================
void sendEmail(const String &subject, const String &messageBody,
               const String &tipoRegistro, const String &usuario) {
  Serial.println("Enviando correo sin adjuntar foto...");

  ESP_Mail_Session session;
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  session.login.user_domain = "";

  session.time.ntp_server = FPSTR("pool.ntp.org");
  session.time.gmt_offset = -4;
  session.time.day_light_offset = 0;

  SMTP_Message message;
  message.sender.name = "ESP32";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = subject;
  message.addRecipient(F("Destinatario"), RECIPIENT_EMAIL);
  message.text.charSet = "utf-8";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_base64;
  message.text.content = messageBody.c_str();

  MailClient.networkReconnect(true);

  if (!smtp.connect(&session)) {
    Serial.printf("Error conectando SMTP. StatusCode: %d  ErrorCode: %d  Reason: %s\n",
                  smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    return;
  }

  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.printf("Error enviando correo. StatusCode: %d  ErrorCode: %d  Reason: %s\n",
                  smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
  } else {
    Serial.println("Correo enviado correctamente (sin foto).");
  }

  // No se envía foto por correo, sólo a la base de datos
}

// ======================== ENVIAR FOTO Y TEMPERATURA A LA BASE DE DATOS ========================
bool enviarFotoABaseDeDatos(const String &tipoRegistro, const String &usuario, float temperatura) {
  if (photoBuf.size() == 0) {
    Serial.println("No hay foto para enviar a la BD.");
    return false;
  }

  HTTPClient http;
  String url = SERVER_URL;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  String fotoBase64 = base64::encode(photoBuf.data(), photoBuf.size());

  StaticJsonDocument<600> doc;
  doc["contraseña"] = texto;
  doc["foto"] = fotoBase64;
  doc["temperatura"] = temperatura;

  String json;
  serializeJson(doc, json);

  int httpResponseCode = http.POST(json);

  if (httpResponseCode == 200) {
    Serial.println("Foto y temperatura enviada correctamente a la base de datos.");
    http.end();
    return true;
  } else {
    Serial.printf("Error enviando a la BD. Código: %d\n", httpResponseCode);
    http.end();
    return false;
  }
}

// ======================== TOMAR FOTO CON ESP32-CAM ========================
bool fetchSnapshotFromCam() {
  HTTPClient http;
  String url = String("http://") + CAM_HOST + "/capture";
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode == 200) {
    WiFiClient * stream = http.getStreamPtr();
    photoBuf.clear();

    unsigned long timeout = millis() + 8000;
    while ((millis() < timeout) && (http.connected() || stream->available())) {
      while (stream->available()) {
        uint8_t c = stream->read();
        photoBuf.push_back(c);
        timeout = millis() + 8000; 
      }
      delay(1);
    }
    http.end();
    if (photoBuf.size() > 0) {
      Serial.printf("Foto recibida: %u bytes\n", (unsigned)photoBuf.size());
      return true;
    } else {
      Serial.println("No se recibió contenido de la CAM.");
      return false;
    }
  } else {
    Serial.printf("Error GET /capture. HTTP code: %d\n", httpCode);
    http.end();
    return false;
  }
}

// ======================== CALLBACK SMTP ========================
void smtpCallback(SMTP_Status status) {
  Serial.println();
  Serial.print("SMTP Status: ");
  Serial.println(status.info());
  if (status.success()) {
    Serial.println("----------------");
    Serial.printf("Mensajes enviados correctamente: %d\n", status.completedCount());
    Serial.printf("Mensajes fallidos: %d\n", status.failedCount());
    Serial.println("----------------");
    if (clearPhotoAfterSend) {
      photoBuf.clear();
      photoBuf.shrink_to_fit();
      fotoLista = false;
      clearPhotoAfterSend = false;
      Serial.println("Buffer de foto liberado después del envío.");
    }
    smtp.sendingResult.clear();
  } else {
    Serial.printf("Envío NO completado. Código estado: %d  Error: %d  Razon: %s\n",
                  smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
  }
}
