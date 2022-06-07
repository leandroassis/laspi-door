#include <SPI.h>
#include <MFRC522.h>
#include <Ethernet.h>
#include <EEPROM.h>
//#include <WebSocketsServer.h>
 
#define RFID_SS_PIN      7          // Chip Enable do sensor rfid    
#define ETHERNET_SS_PIN  10         // Chip enable do módulo ethernet
#define RST_PIN     9           // Reset do barramento SPI
#define RELAY_PIN   4           // Pino de acionar a relé
#define RELAY2     A0           // Pino de apoio ao acionamento da relé 
#define INTERRUP    3           // Pino de conexão com o clock do 555
#define KEEP_PIN    6           // Pino de saída de sinal do arduino para manter circuito de redundância   
#define RED        A5           // Pino de controle do led de indicação
#define BLUE       A4           // Pino de controle do led de indicação
#define GREEN      A3           // Pino de controle do led de indicação

#define TAG_COUNT 50            // Quantidade de cartões a serem implementados(MAX: 230)
 
MFRC522 rfid(RFID_SS_PIN, RST_PIN);
//WebSocketsServer webSocket = WebSocketsServer(80);

byte mac_address[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
byte ip_address[] = {192, 168, 1, 15}; // 192.168.88.19

String tags_temp[TAG_COUNT];
int free_address;

/*
void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length){
    switch(type){
      case WStype_DISCONNECTED:
        Serial.println("Server desconectado.");
        break;
      case WStype_CONNECTED:
        Serial.println("Server conectado.");
        webSocket.sendTXT(num, "Okay!");
        break;
      case WStype_TEXT:
        Serial.println("Recebido: "+String((char *)payload));
        if(payload == "cadastrar"){
          // Le UID, cadastrar na eeprom e envia via TXT

          digitalWrite(RED, LOW);
          digitalWrite(GREEN, LOW);
          for(int i = 0; i < 10; i++){
            digitalWrite(BLUE, !digitalRead(BLUE));
            delay(300);
          }
        
          //String tag = read_UID();
          String tag = "FF85E2A0";
          writeTagInEEPROM(tag);
          EEPROMDump();
          webSocket.sendTXT(num, tag);

          digitalWrite(BLUE, LOW);
          digitalWrite(GREEN, HIGH);
          digitalWrite(RED, LOW);
          delay(1000);
          digitalWrite(BLUE, HIGH);
          digitalWrite(GREEN, LOW);
          digitalWrite(RED, LOW);

          for(int i = 0; i < TAG_COUNT; i++) Serial.println(tags_temp[i]);
        }
        else if(payload == "excluir"){
          // seta o free_address para 1 e limpa toda a memória eeprom
          EEPROM.write(0, 1);
          for(int i = 1; i < 1024; i++) EEPROM.write(i, 0xFF);
          webSocket.sendTXT(num, "Limpo");
        }
        else{
          // coloca a tag recebida na eeprom
          writeTagInEEPROM(String((char *) payload));
          webSocket.sendTXT(num, "Ok");
        }
      default:
        Serial.println(type);
        break;
    }
}
*/

void setup(){
  
  Serial.begin(115200);           
  Serial.println("Inicializando o sistema");

  SPI.begin();                 // Inicia a comunicação SPI 
  rfid.PCD_Init();             // Inicia o sensor RFID
  Ethernet.begin(mac_address, ip_address); // Inicia o shield Ethernet

  Serial.println("Ethernet, sensor RFID, e barramento SPI configurados e setados.");
  delay(1500);
  
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(KEEP_PIN, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(A2, OUTPUT);
  pinMode(RED, OUTPUT);
  pinMode(BLUE, OUTPUT);
  pinMode(GREEN, OUTPUT);

  pinMode(INTERRUP, INPUT); 
  
  digitalWrite(RELAY_PIN, LOW); // Porta trancada
  digitalWrite(RELAY2, LOW);    // Porta trancada
  digitalWrite(KEEP_PIN, HIGH); // Inicia pulso do arduino em 5V

  // Inicia o RGB em azul
  digitalWrite(RED, LOW);       
  digitalWrite(BLUE, HIGH);
  digitalWrite(GREEN, LOW);

  attachInterrupt(digitalPinToInterrupt(INTERRUP), KeepRelayAlive, CHANGE); // O arduino emite um sinal de "prova de vida" sincronizado ao clock

  Serial.println("Pinos do arduino inicializados com sucesso.");

  EEPROMDump();
  Serial.println("Tags dumpadas da memória para o vetor temporário.");
  
  //webSocket.begin();
  //webSocket.onEvent(handleWebSocketEvent);
  Serial.println("Websocket configurado e conectado com sucesso.");

  /*
  Linha de código rodada para inicializar pela primeira vez a eeprom do arduino
  EEPROM.write(0, 1);
  for(int i = 1; i < 1024; i++) EEPROM.write(i, 0xFF);
  */
}
 
void loop() {
  bool RFID_Found = false;                // Variável auxiliar para controle de interface do usuário
  String strID = "";                      // Variável para guardar o uid lido

  if(!rfid.PCD_PerformSelfTest()){
    // Performa selftest no sensor RFID, caso falhe a porta é mantida aberta
    PermanentStateError();
  }
  
  strID = read_UID();
 
  //webSocket.loop();

  for(int i = 0; i < TAG_COUNT; i++){
    if(strID == tags_temp[i]){ // Se a tag estiver no vetor de tags (foi cadastrada previamente) a porta abre
      RFID_Accepted();
      RFID_Found = true;
      break;
    }
  }
  if(!RFID_Found) RFID_Rejected();
  
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

String read_UID(){
  String uid = "";

  while(!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()){}
  
  for (byte i = 0; i < 4; i++) {
    uid += String(rfid.uid.uidByte[i], HEX);     // Realiza o parser da tag lida pelo sensor em um string
  }
  uid.toUpperCase();
  
  return uid;
}

void EEPROMDump(){
  // le todos os 1024 endereços da eeprom, faz o parser (monta uma tag a cada 4 bytes lidos) e copia para o vetor tags_temp
  int contador = 0;
  String temp = "";
  
  for(int j = 0; j < TAG_COUNT; j++) tags_temp[j] = "";
  Serial.println("Vetor de tags limpo.");
  
  for(int i = 1; i <= 4*TAG_COUNT; i++){ // Dumpa a EEPROM em um array para evitar consultas excessivas    
    contador++;
    temp += String(EEPROM.read(i), HEX);
    if(contador == 4){
      contador = 0;
      temp.toUpperCase();
      tags_temp[i/4 - 1] = temp;
      Serial.print(temp);
      Serial.print("  ");
      Serial.print(i);
      Serial.print("\n");
      temp = "";
    }  
  }

  Serial.println("Tags dumpadas com sucesso.");
}

void writeTagInEEPROM(String ID){
    // Função para parsear a String ID em 4 bytes e escreve-os em endereços consecutivos na EEPROM
    char temp_char[2];
    free_address = EEPROM.read(0);
    
    Serial.println(ID);

    for(int i = 0; i < TAG_COUNT; i++){
      if(ID == tags_temp[i]) return;
    }
   
    for(int i = 0; i < 8; i+=2){
      temp_char[0] = ID[i];
      temp_char[1] = ID[i+1];
      EEPROM.write(free_address, strtoul(temp_char, NULL, 16));
      free_address++;
    }

    // Após parsear e escrever a tag na EEPROM, salva no endereço 0 o próximo endereço livre
    EEPROM.write(0, free_address);
}

void RFID_Rejected(){
  // Controle do led caso a tag RFID não esteja cadastrado

  // Liga o vermelho por 3s
  digitalWrite(RED, HIGH);
  digitalWrite(BLUE, LOW);
  digitalWrite(GREEN, LOW);
  delay(3000);
  // Volta ao estado inicial
  digitalWrite(RED, LOW);
  digitalWrite(BLUE, HIGH);
  digitalWrite(GREEN, LOW);
}

void RFID_Accepted(){
  // Controle do led caso a tag RFID esteja cadastrado
  
  // Liga o led verde e abre a porta por 5s
  digitalWrite(RELAY_PIN, HIGH); 
  digitalWrite(RELAY2, HIGH);
  digitalWrite(GREEN, HIGH);
  digitalWrite(RED, LOW);
  digitalWrite(BLUE, LOW);
  delay(5000);
  // Volta o led para o azul e tranca a porta
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(RELAY2, LOW);   
  digitalWrite(GREEN, HIGH);
  digitalWrite(RED, LOW);
  digitalWrite(BLUE, HIGH);
}

void ErrorAddingTag(){
  // Pisca 10 vezes em vermelho com intervalos de 0.2s e depois volta ao azul sinalizando erro ao cadastrar
  Serial.println("Erro adicionando uma tag no database.");
  for(int i = 0; i < 10; i++){
        digitalWrite(BLUE, LOW);
        digitalWrite(GREEN, LOW);
        digitalWrite(RED, HIGH);
        delay(200);
        digitalWrite(RED, LOW);
        delay(200);
      }
      digitalWrite(BLUE, HIGH);
      digitalWrite(GREEN, LOW);
      digitalWrite(RED, LOW);
      return;  
}

void PermanentStateError(){
  // Led RGB fica piscando em vermelho com intervalos de 1.2s e abre a porta
  Serial.println("Entrando em estado de erro permanente.");
  digitalWrite(BLUE, LOW);
  digitalWrite(GREEN, LOW);
  while(1){
    digitalWrite(RED, HIGH);
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(RELAY2, HIGH);
    delay(1200);
    digitalWrite(RED, LOW);
    delay(1200);
  }  
}

void KeepRelayAlive(){
  // Toda vez que o vetor de interrupção for chamado, o valor no pino KEEP é invertido
  digitalWrite(KEEP_PIN, !digitalRead(KEEP_PIN));
}
