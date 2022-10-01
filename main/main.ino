#include <EEPROM.h>
#include <MFRC522.h>
#include <SPI.h>
#include <Ethernet.h>

/*
Todo:
 - refatorar implementação das tags, carregar diretamente do servidor, caso nao esteja up carrega da eeprom
 - reiniciar a cada x minutos para evitar erros e travamentos DONE
 - implementar realimentação da relé (verificar se ela realmente chaveou dps que o sinal foi dado)
 - implementar novo sistema de redundância e de desligamento correto
 - melhorar comunicação com servidor cerberus
*/
 
// pinos do barramento SPI
#define RFID_SS_PIN      8          // Chip Enable do sensor rfid    
#define ETHERNET_SS_PIN  10         // Chip enable do módulo ethernet
#define RST_PIN     9           // Reset do barramento SPI

// pinos do sistema de acionamento da porta/pinos do sistema de redundância
#define RELAY_PIN   5           // Pino de acionar a relé
#define RELAY2      2           // Pino de apoio ao acionamento da relé 

// pinos da indicação luminosa
#define RED        A5           // Pino de controle do led de indicação
#define BLUE       A4           // Pino de controle do led de indicação
#define GREEN      A3           // Pino de controle do led de indicação
#define LED_GND    A2           // Pino de GND para os leds de indicação

// configurações de reset regular do sistema
#define ARDUINO_AUTO_RST  7     // Pino de reset do arduino
#define RESET_TIME        1200000 // 20 min

#define TAG_COUNT 45            // Quantidade de cartões a serem implementados(MAX: 50)
 
// Configurações dos periféricos
MFRC522 rfid(RFID_SS_PIN, RST_PIN); // inicialização do sensor RFID

byte mac_address[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED}; // configurações de rede
byte server[] = {XX, XX, XX, XX};

String tags_temp[TAG_COUNT]; // Array de tags válidas
String readString = String(30); //
int free_address; // ponteiro para o último endereço livre na memória

unsigned long start_time; // variável para o tempo de inicio do programa

void setup(){
  digitalWrite(ARDUINO_AUTO_RST, HIGH);
  delay(50);

  start_time  = millis(); // Seta o tempo de inicio para reboot regular
  
  pinMode(ARDUINO_AUTO_RST, OUTPUT);
  pinMode(ETHERNET_SS_PIN, OUTPUT);
  pinMode(4,OUTPUT);

  digitalWrite(ETHERNET_SS_PIN, LOW); 
  digitalWrite(4, HIGH);
  //Serial.begin(9600);           
  //Serial.println("Inicializando o sistema");

  EEPROMDump();
  //Serial.println("Tags dumpadas da memória para o vetor temporário.");
  
  SPI.begin();                 // Inicia a comunicação SPI 
  rfid.PCD_Init();             // Inicia o sensor RFID
  Ethernet.begin(mac_address); // Inicia o shield Ethernet

  //Serial.println("Ethernet, sensor RFID, e barramento SPI configurados e setados.");
  
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(LED_GND, OUTPUT);
  pinMode(RED, OUTPUT);
  pinMode(BLUE, OUTPUT);
  pinMode(GREEN, OUTPUT);

  // Inicia o RGB em azul
  digitalWrite(RED, LOW);       
  digitalWrite(BLUE, HIGH);
  digitalWrite(GREEN, LOW);
  digitalWrite(LED_GND, LOW);

  //Serial.println("Pinos do arduino inicializados com sucesso.");

  digitalWrite(RELAY2, LOW);    // Porta trancada
}
 
void loop() {
  bool RFID_Found = false;                // Variável auxiliar para controle de interface do usuário
  String strID = "";                      // Variável para guardar o uid lido

  if((millis() > start_time + RESET_TIME) || !rfid.PCD_PerformSelfTest()){
    // se o programa rodar por RESET_TIME, ele reboota para diminuir janela de erros
    // ou se o sensor RFID parar de funcionar
    digitalWrite(ARDUINO_AUTO_RST, LOW);
  }
    
  ClientHandler();

  strID = read_UID();
  if(strID == "") return;
  //Serial.println(strID);
  
  
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

void ClientHandler(){
  EthernetClient client;

  if(client.connect(server, 80)){

    client.println("HTTP/1.1 200 OK");
    while(client.available()){
        char c = client.read();

        if(readString.length() < 100) readString += c;
        if(c == '\n'){
          //Serial.println(readString);
          
          if(readString.indexOf("cadastrar=") > 0){
            // Le UID, cadastra na eeprom e envia a tag
            //Serial.println("Cadastrar tag");
            
            digitalWrite(RED, LOW);
            digitalWrite(GREEN, LOW);
            for(int i = 0; i < 10; i++){
              digitalWrite(BLUE, !digitalRead(BLUE));
              delay(300);
            }
            
            String tag = "";
            do{
              tag = read_UID();
            }while(tag == "");

            if(!writeTagInEEPROM(tag)){
              EEPROMDump();
              client.println(tag);
              client.println("OK\n");
            }
            else client.println("NOP\n");
            
            digitalWrite(BLUE, LOW);
            digitalWrite(GREEN, HIGH);
            digitalWrite(RED, LOW);
            delay(2500);
            digitalWrite(BLUE, HIGH);
            digitalWrite(GREEN, LOW);
            digitalWrite(RED, LOW);
  
            //for(int i = 0; i < TAG_COUNT; i++) Serial.println(tags_temp[i]);
          }
          else if(readString.indexOf("deletar=") > 0){
            // seta o free_address para 1 e limpa toda a memória eeprom
            //Serial.println("Resetar toda a memória.");
            EEPROM.write(0, 1);
            for(int i = 1; i < 1024; i++){
              EEPROM.write(i, 0xFF);
              delay(6);
            }
            EEPROMDump();
            client.println("OK");
            //Serial.println("Memória resetada.");
          }
          else if(readString.indexOf("addtag=") > 0){
            // Adiciona uma tag na memória
            //Serial.println("Adicionar tag: ");
            //Serial.println(readString.substring(readString.indexOf("addtag=")+7, readString.indexOf("addtag=")+15));
            if(!writeTagInEEPROM(readString.substring(readString.indexOf("addtag=")+7, readString.indexOf("addtag=")+15))){
              EEPROMDump();
              client.println("OK");
            }
            else client.println("NOP");

            //for(int i = 0; i < TAG_COUNT; i++) Serial.println(tags_temp[i]);
          }
          else if(readString.indexOf("atualizarserver=") > 0){
            Serial.println("Atualizar servidor.");
            EEPROMDump();
            for(int i = 0; i < TAG_COUNT; i++){
              client.println(tags_temp[i]);
            }
            client.println("OK");
            //Serial.println("Servidor atualizado.");
          }
          else if(readString.indexOf("desligarsistema=") > 0){
            Serial.println("Desligar sistema");
            client.println("OK");
            client.stop();
            PermanentStateError();
          }
          else if(readString.indexOf("resetarsistema=") > 0){
            Serial.println("Resetando...");
            client.println("OK");
            client.stop();
            resetFunc();
          }
          readString = "";
          client.stop();
        }
    }
  }
}

String read_UID(){
  String uid = "";

  if(!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return "";
  
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
  //Serial.println("Vetor de tags limpo.");
  
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
  Serial.println("--------- Vetor de tags ---------");
  for(int i = 0; i < TAG_COUNT; i++) Serial.println(tags_temp[i]);
  //Serial.println("Tags dumpadas com sucesso.");
}

unsigned short writeTagInEEPROM(String ID){
    // Função para parsear a String ID em 4 bytes e escreve-os em endereços consecutivos na EEPROM
    char temp_char[2];
    free_address = EEPROM.read(0);
    
    //Serial.println(ID);
    //Serial.println("Endereço livre: "+String(free_address));

    if(free_address == 1+TAG_COUNT*4){
      ErrorAddingTag();
      return 1;
    }

    for(int i = 0; i < TAG_COUNT; i++){
      if(ID == tags_temp[i]){
        ErrorAddingTag();
        return 1;
      }
    }
   
    for(int i = 0; i < 8; i+=2){
      temp_char[0] = ID[i];
      temp_char[1] = ID[i+1];
      EEPROM.write(free_address, strtoul(temp_char, NULL, 16));
      free_address++;
    }

    // Após parsear e escrever a tag na EEPROM, salva no endereço 0 o próximo endereço livre
    EEPROM.write(0, free_address);
    return 0;
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
  digitalWrite(GREEN, LOW);
  digitalWrite(RED, LOW);
  digitalWrite(BLUE, HIGH);
}

void ErrorAddingTag(){
  // Pisca 10 vezes em vermelho com intervalos de 0.2s e depois volta ao azul sinalizando erro ao cadastrar
  //Serial.println("Erro adicionando uma tag no database.");
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
  //Serial.println("Entrando em estado de erro permanente.");
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

/*
void KeepRelayAlive(){
  // Toda vez que o vetor de interrupção for chamado, o valor no pino KEEP é invertido
  digitalWrite(KEEP_PIN, !digitalRead(KEEP_PIN));
}
*/
