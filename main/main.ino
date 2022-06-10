#include <EEPROM.h>
#include <MFRC522.h>
#include <SPI.h>
#include <Ethernet.h>
 
#define RFID_SS_PIN      8          // Chip Enable do sensor rfid    
#define ETHERNET_SS_PIN  10         // Chip enable do módulo ethernet
#define RST_PIN     9           // Reset do barramento SPI
#define RELAY_PIN   5           // Pino de acionar a relé
#define RELAY2     A0           // Pino de apoio ao acionamento da relé 
#define INTERRUP    3           // Pino de conexão com o clock do 555
#define KEEP_PIN    6           // Pino de saída de sinal do arduino para manter circuito de redundância   
#define RED        A5           // Pino de controle do led de indicação
#define BLUE       A4           // Pino de controle do led de indicação
#define GREEN      A3           // Pino de controle do led de indicação

#define TAG_COUNT 30            // Quantidade de cartões a serem implementados(MAX: 35)
 
MFRC522 rfid(RFID_SS_PIN, RST_PIN);
EthernetServer server(80);

byte mac_address[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
byte ip_address[] = {192, 168, 88, 12};
byte gateway[] = {192, 168, 88, 1};
byte subnet[] = {255, 255, 255, 0};

String tags_temp[TAG_COUNT], readString = String(30);
int free_address;

void setup(){
  
  //Serial.begin(9600);           
  //Serial.println("Inicializando o sistema");
  
  SPI.begin();                 // Inicia a comunicação SPI 
  rfid.PCD_Init();             // Inicia o sensor RFID
  Ethernet.begin(mac_address, ip_address, gateway, subnet); // Inicia o shield Ethernet

  //Serial.println("Ethernet, sensor RFID, e barramento SPI configurados e setados.");
  delay(100);
  
  server.begin();
  //Serial.println("Server configurado com sucesso.");
  delay(100);
  
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(KEEP_PIN, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(A2, OUTPUT);
  pinMode(RED, OUTPUT);
  pinMode(BLUE, OUTPUT);
  pinMode(GREEN, OUTPUT);

  pinMode(INTERRUP, INPUT); 

  // Inicia o RGB em azul
  digitalWrite(RED, LOW);       
  digitalWrite(BLUE, HIGH);
  digitalWrite(GREEN, LOW);
  digitalWrite(A2, LOW);

  digitalWrite(KEEP_PIN, HIGH); // Inicia pulso do arduino em 5V  
  attachInterrupt(digitalPinToInterrupt(INTERRUP), KeepRelayAlive, CHANGE); // O arduino emite um sinal de "prova de vida" sincronizado ao clock

  //Serial.println("Pinos do arduino inicializados com sucesso.");

  digitalWrite(RELAY_PIN, LOW); // Porta trancada
  digitalWrite(RELAY2, LOW);    // Porta trancada
  
  EEPROMDump();
  //Serial.println("Tags dumpadas da memória para o vetor temporário.");
}
 
void loop() {
  bool RFID_Found = false;                // Variável auxiliar para controle de interface do usuário
  String strID = "";                      // Variável para guardar o uid lido

  if(!rfid.PCD_PerformSelfTest()){
    // Performa selftest no sensor RFID, caso falhe a porta é mantida aberta
    PermanentStateError();
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
  EthernetClient client = server.available();
  if(client){
    while(client.connected() && client.available()){
        char c = client.read();
        if(readString.length() < 100) readString += c;
        if(c == '\n'){
          //Serial.println(readString);
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("");
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          client.println("<body>");
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
            }
            else client.println("FULL OR ALREADY SIGN");
            
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
            client.println("Limpo");
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
            else client.println("FULL OR ALREADY SIGN");

            //for(int i = 0; i < TAG_COUNT; i++) Serial.println(tags_temp[i]);
          }
          else if(readString.indexOf("atualizarserver=") > 0){
            //Serial.println("Atualizar servidor.");
            EEPROMDump();
            for(int i = 0; i < TAG_COUNT; i++){
              client.println(tags_temp[i]);
            }
            //Serial.println("Servidor atualizado.");
          }
          else if(readString.indexOf("desligarsistema=") > 0){
            PermanentStateError();
          }
          client.println("</body>");
          client.println("</html>");
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
      //Serial.print(temp);
      //Serial.print("  ");
      //Serial.print(i);
      //Serial.print("\n");
      temp = "";
    }  
  }
  //Serial.println("--------- Vetor de tags ---------");
  //for(int i = 0; i < TAG_COUNT; i++) Serial.println(tags_temp[i]);
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

void KeepRelayAlive(){
  // Toda vez que o vetor de interrupção for chamado, o valor no pino KEEP é invertido
  digitalWrite(KEEP_PIN, !digitalRead(KEEP_PIN));
}
