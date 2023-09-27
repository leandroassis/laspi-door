#include <avr/wdt.h>
#include <SPI.h>
#include <Ethernet.h>
#include <MFRC522.h>

// pinos do barramento SPI
#define RFID_SS_PIN 8       // Chip Enable do sensor rfid
#define ETHERNET_SS_PIN 10  // Chip enable do módulo ethernet
#define SDCARD_SS_PIN 4     // Chip enable do leitor de cartão SD
#define RST_PIN 9           // Reset do barramento SPI

// pinos do sistema de acionamento da tranca
#define RELAY_PIN 2    // Pino de acionar a relé
#define RELAY_CHECK 7  // Pino para checar se a relé está acionando
#define GND 5          // Pino de GND virtual para o relay_check

// pinos da indicação luminosa
#define RED A5      // Pino de controle do led de indicação
#define BLUE A4     // Pino de controle do led de indicação
#define GREEN A3    // Pino de controle do led de indicação
#define LED_GND A2  // Pino de GND para os leds de indicação

// configurações de caracteristicas do sistema
void (*resetFunc)(void) = 0;  // função de reset
#define RESET_RFID_INT  45000UL

// Configurações dos periféricos
MFRC522 rfid(RFID_SS_PIN, RST_PIN);  // inicialização do sensor RFID
EthernetClient client;

byte mac_address[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };  // configurações de rede
char server[] = "172.16.15.1";                              // IP do servidor

unsigned long rfid_reset_timer = 0;

void setup() {
  //Serial.begin(9600);

  wdt_enable(WDTO_8S);  // Seta o tempo de reset do wdt em 8 segundos
  wdt_reset();

  // Inicialização dos periféricos
  pinMode(SDCARD_SS_PIN, OUTPUT);
  digitalWrite(SDCARD_SS_PIN, HIGH);

  SPI.begin();
  rfid.PCD_Init();
  delay(1000);

  wdt_reset();

  Ethernet.init(ETHERNET_SS_PIN);
  Ethernet.begin(mac_address);
  delay(1000);

  //Serial.println(Ethernet.localIP());

  // Inicialização do pinout
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(RELAY_CHECK, INPUT);
  pinMode(LED_GND, OUTPUT);
  pinMode(RED, OUTPUT);
  pinMode(BLUE, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(GND, OUTPUT);

  //Serial.println("1");

  digitalWrite(RED, LOW);
  digitalWrite(BLUE, HIGH);
  digitalWrite(GREEN, LOW);
  digitalWrite(LED_GND, LOW);
  digitalWrite(GND, LOW);

  wdt_reset();

  client.setConnectionTimeout(1300);
  client.connect(server, 8000);    // conecta com o servidor
  digitalWrite(RELAY_PIN, LOW);  // tranca a porta
  //Serial.println("Inicializado");
}

void loop() {
  wdt_reset();  // reseta o watchdog timer

  unsigned int rfid_down_count = 0;
  String strID = "";  // Variável para guardar o uid lido
  byte c[3] = { 0 };  // buffer pra receber resposta do servidor
  short len = 0;      // quantidades de bytes pra ler

check_conn:
  while(!client.connected()) {
    // enquanto o servidor não estiver ligado

    if(client.connect(server, 8000) == 1){
      digitalWrite(BLUE, LOW);
      digitalWrite(GREEN, LOW);
      digitalWrite(RED, LOW);
      break;// tenta reconectar
    }

    wdt_reset();

    digitalWrite(BLUE, LOW);
    digitalWrite(GREEN, LOW);
    digitalWrite(RED, LOW);
    digitalWrite(RELAY_PIN, HIGH);
    delay(1500);
    
    digitalWrite(BLUE, LOW);
    digitalWrite(GREEN, LOW);
    digitalWrite(RED, HIGH);
    digitalWrite(RELAY_PIN, HIGH);
    delay(1500);
    //Serial.println("Não conectado");
  }
  
  digitalWrite(ETHERNET_SS_PIN, HIGH);
  delay(50);
  while(!rfid.PCD_PerformSelfTest() || (millis() - rfid_reset_timer >= RESET_RFID_INT)) {
  // se o sensor RFID não passar no selftest ou a verificação da relé falhar, reiniciar
  //Serial.println(digitalRead(RELAY_CHECK));

    //client.println("GET /logs?text="+String(rfid.PCD_PerformSelfTest()));
    //client.println("GET /logs?text=rfiderror");
    wdt_reset();

    if(millis() - rfid_reset_timer >= RESET_RFID_INT) rfid_reset_timer += RESET_RFID_INT;

    if(rfid_down_count >= 10) {
      digitalWrite(BLUE, LOW);
      digitalWrite(GREEN, HIGH);
      digitalWrite(RED, LOW);
      digitalWrite(RELAY_PIN, HIGH);
    }

    digitalWrite(RST_PIN, LOW);
    delay(50);
    digitalWrite(RST_PIN, HIGH);
    rfid.PCD_Init();
    delay(500);
    rfid.PCD_Reset();
    delay(500);

    rfid_down_count += 1;

    //resetar();
  }
  
  //Serial.println("Conectado");
  digitalWrite(GREEN, LOW);
  digitalWrite(RED, LOW);
  digitalWrite(BLUE, HIGH);
  digitalWrite(RELAY_PIN, LOW);
  delay(50);

  // rotina de controle da porta e interface com usuário
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()){
    digitalWrite(ETHERNET_SS_PIN, LOW);
    return;
  }

  wdt_reset();  // reseta o watchdog timer

  //client.println("GET /logs?text=tagreconhecida");

  // se um cartão se aproximar da leitora
  for (byte i = 0; i < 4; i++) strID += String(rfid.uid.uidByte[i], HEX);  // Realiza o parser da tag lida pelo sensor em uma string
  strID.toUpperCase();

  digitalWrite(ETHERNET_SS_PIN, LOW);
  delay(50);
  wdt_reset();
  client.println("GET /5e9315ffcbd7274dafe3a2fbe4e51f00ef38a53e2a78cfcf8e8303a3b2bd1dca?uid=" + strID);  // envia a ultima tag lida
  client.println();
  
  //Serial.println("send");
  wdt_reset();
  if((len = client.available())) {
    client.read(c, len);
    //Serial.println(c[1]);
    //Serial.println(len);

    if (c[1] == 83){
        //client.println("GET /logs?text=aberto");
        RFID_Accepted();
    }
    else{
      //client.println("GET /logs?text=naoabre");
      RFID_Rejected();
    }
  }
  else goto check_conn;

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  //client.println("GET /logs?text=fimdeciclo");
  wdt_reset();
  delay(3000);
}

void RFID_Rejected() {
  // Controle do led caso a tag RFID não esteja cadastrado

  wdt_reset();

  // Liga o vermelho por 2.5 s
  digitalWrite(BLUE, LOW);
  digitalWrite(RED, HIGH);
  digitalWrite(GREEN, LOW);
  digitalWrite(RELAY_PIN, LOW);
  delay(50);

//  if (!digitalRead(RELAY_CHECK)) resetar();

  delay(2500);

  wdt_reset();

  // Volta ao estado inicial
  digitalWrite(RED, LOW);
  digitalWrite(BLUE, HIGH);
  digitalWrite(GREEN, LOW);
  digitalWrite(RELAY_PIN, LOW);
  delay(200);

  //if (!digitalRead(RELAY_CHECK)) resetar();
  wdt_reset();
}

void RFID_Accepted() {
  // Controle do led caso a tag RFID esteja cadastrado
  wdt_reset();

  digitalWrite(RELAY_PIN, HIGH);  // abre a tranca

  // Coloca o led em verde
  digitalWrite(BLUE, LOW);
  digitalWrite(GREEN, HIGH);
  digitalWrite(RED, LOW);
  delay(250);

  //if (digitalRead(RELAY_CHECK)) resetar();
  wdt_reset();

  delay(4800);

  digitalWrite(RELAY_PIN, LOW);  // Fecha a tranca

  // Volta o led para o azul
  digitalWrite(GREEN, LOW);
  digitalWrite(RED, LOW);
  digitalWrite(BLUE, HIGH);
  delay(250);

  //if (!digitalRead(RELAY_CHECK)) resetar();

  wdt_reset();
}

void resetar() {
  //client.println("GET /logs?text=resetando");
  //client.println();

  digitalWrite(GREEN, LOW);
  digitalWrite(RED, LOW);
  digitalWrite(BLUE, LOW);
  digitalWrite(RELAY_PIN, HIGH);

  resetFunc();
}
