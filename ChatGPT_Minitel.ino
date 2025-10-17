/*******************************************************************
    A ChatGPT terminal for Minitel ESP32 
    Based on the code from goguelnikov’s Minitel ESP32 ChatGPT terminal
    https://github.com/goguelnikov/MinitelChatGPT

    Minitel ESP32 by iodeo
    https://hackaday.io/project/180473-minitel-esp32
    https://www.tindie.com/products/iodeo/minitel-esp32-dev-board/
    github: https://github.com/iodeo/Minitel-ESP32 
    
    Minitel library by eserandour
    github: https://github.com/eserandour/Minitel1B_Hard
    
    ChatGPT Client For ESP32 by Eric Nam
    github: https://github.com/0015/ChatGPT_Client_For_Arduino

 *******************************************************************/


#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Minitel1B_Hard.h> // see https://github.com/eserandour/Minitel1B_Hard

// --- Configuration WiFi ---
const char* ssid = "XXXXXXXXXXX";
const char* password = "XXXXXXXXXXX";

// --- Token ChatGPT ---
const char* openai_api_key = "sk-proj-XXXXXXXXXXXXXXXXXXXX";

// --- Initialisation Minitel ---
Minitel minitel(Serial2);
#define TITRE "3615 ChatGPT"
String texte = "";
int nbCaracteres = 0;
const int PREMIERE_LIGNE = 2;
const int NB_LIGNES = 4;
const String VIDE = ".";
unsigned long touche;
String intro = "Moi - ";

// --- Fonctions auxiliaires ---
void champVide(int premiereLigne, int nbLignes) {
  minitel.noCursor();
  for (int j = 0; j < nbLignes; j++) {
    minitel.attributs(CARACTERE_BLEU);
    minitel.print(VIDE);
    minitel.repeat(39);
    minitel.attributs(CARACTERE_BLANC);
  }
  for (int j = 0; j < nbLignes; j++) {
    minitel.moveCursorUp(1);
  }
  minitel.cursor();
}

void newPage(String titre) {
  minitel.newScreen();
  minitel.attributs(INVERSION_FOND);
  minitel.attributs(CARACTERE_BLEU);
  for (int i = 0; i < 40; i++) minitel.print(" ");
  for (int i = 0; i < 14; i++) minitel.print(" ");
  minitel.print(titre);
  for (int i = 0; i < 14; i++) minitel.print(" ");
  for (int i = 0; i < 40; i++) minitel.print(" ");
  minitel.attributs(CARACTERE_BLANC);
  minitel.attributs(FOND_NORMAL);
  minitel.println();
}

void displayResponse(String response) {
  Serial.print(F("Response: "));
  Serial.println(response);
  minitel.attributs(INVERSION_FOND);
  minitel.attributs(CARACTERE_ROUGE);
  minitel.println();
  minitel.print("GPT - ");
  minitel.print(response);
  minitel.attributs(CARACTERE_BLANC);
  minitel.attributs(FOND_NORMAL);
}

  String envoyerRequeteOpenAI(String prompt) {
  WiFiClientSecure client;
  client.setInsecure();  // Ignore la vérification SSL

  HTTPClient http;
  http.setTimeout(15000);
  http.begin(client, "https://api.openai.com/v1/chat/completions");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + openai_api_key);

  String payload = "{\"model\": \"gpt-4\", \"max_tokens\": 100, \"messages\": [{\"role\": \"user\", \"content\": \"" + prompt + "\"}]}";

  int httpCode = http.POST(payload);
  Serial.printf("Code HTTP : %d\n", httpCode);

  String reply = "";

  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("Réponse brute :");
    Serial.println(response);

    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, response);

    if (!error) {
      reply = doc["choices"][0]["message"]["content"].as<String>();
    } else {
      Serial.print("Erreur JSON : ");
      Serial.println(error.c_str());
      reply = "Erreur de parsing JSON.";
    }
  } else {
    Serial.println("Erreur de requête OpenAI :");
    Serial.println(http.errorToString(httpCode));
    reply = "Erreur réseau.";
  }

  http.end();
  return reply;
}

// --- Setup principal ---
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Connexion au Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnecté au Wi-Fi");

  configTime(0, 0, "pool.ntp.org");
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    delay(100);
    now = time(nullptr);
  }
  Serial.println("Heure synchronisée");

  // --- Test de l’API une seule fois ---
  String testReply = envoyerRequeteOpenAI("Bonjour ?");
  Serial.println("Réponse ChatGPT (test) :");
  Serial.println(testReply);

  // --- Init Minitel ---
  Serial.print("Initialising Minitel...");
  minitel.changeSpeed(minitel.searchSpeed());
  minitel.smallMode();
  minitel.scrollMode();
  newPage(TITRE);
  champVide(PREMIERE_LIGNE, NB_LIGNES);
  Serial.println(" done");
  minitel.print(intro);
}

// --- Boucle principale ---
void loop() {
  touche = minitel.getKeyCode();

  if ((touche != 0) &&
      (touche != CONNEXION_FIN) &&
      (touche != SOMMAIRE) &&
      (touche != ANNULATION) &&
      (touche != RETOUR) &&
      (touche != REPETITION) &&
      (touche != GUIDE) &&
      (touche != CORRECTION) &&
      (touche != SUITE) &&
      (touche != ENVOI)) {

    if (nbCaracteres < 40 * NB_LIGNES) {
      nbCaracteres++;
      texte += char(touche);
    }

    if (nbCaracteres == 40 * NB_LIGNES) {
      minitel.moveCursorXY(40, (PREMIERE_LIGNE - 1) + NB_LIGNES);
    }
  }

 if (touche == ENVOI) {
  Serial.println("[ChatGPT] Envoi du message...");

  if (WiFi.status() != WL_CONNECTED) {
    displayResponse("Wi-Fi déconnecté !");
    Serial.println("Tentative de reconnexion...");
    WiFi.begin(ssid, password);
    delay(5000);
    if (WiFi.status() != WL_CONNECTED) {
      displayResponse("Échec de reconnexion Wi-Fi.");
      return;
    }
  }

  String reply = envoyerRequeteOpenAI(texte);
  displayResponse(reply);

  texte = "";
  nbCaracteres = 0;
  champVide(PREMIERE_LIGNE, NB_LIGNES);
  minitel.moveCursorReturn(1);
  minitel.print(intro);
}


  if (touche == CORRECTION) {
    if ((nbCaracteres > 0) && (nbCaracteres <= 40 * NB_LIGNES)) {
      if (nbCaracteres != 40 * NB_LIGNES) minitel.moveCursorLeft(1);
      minitel.attributs(CARACTERE_BLEU);
      minitel.print(VIDE);
      minitel.attributs(CARACTERE_BLANC);
      minitel.moveCursorLeft(1);
      texte = texte.substring(0, texte.length() - 1);
      nbCaracteres--; 
    }
  }
}