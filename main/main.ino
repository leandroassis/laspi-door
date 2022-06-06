#include <SPI.h>
#include <MFRC522.h>
#include <SD.h>
#include <Ethernet.h>
#include <CSV_Parser.h>
 
#define RFID_SS_PIN      7          // Chip Enable do sensor rfid    
#define SD_SS_PIN        4          // Chip´enable do cartão sd
#define ETHERNET_SS_PIN  10         // Chip enable do módulo ethernet
#define RST_PIN     9           // Reset do sensor rfid
#define RELAY_PIN   4           // Pino de acionar a relé
#define RELAY2     A0           // Pino de apoio ao acionamento da relé 
#define INTERRUP    3           // Pino de conexão com o clock do 555
#define KEEP_PIN    6           // Pino de saída de sinal do arduino para manter circuito de redundância   
#define RED        A5           // Pino de controle do led de indicação
#define BLUE       A4           // Pino de controle do led de indicação
#define GREEN      A3           // Pino de controle do led de indicação

#define TAG_COUNT 160            // Quantidade de cartões a serem implementados(MAX: 230)
 
//MFRC522 rfid(SS_PIN, RST_PIN);  
EthernetServer server(80);
CSV_Parser cp("ss", true, ';'); // Inicializa o parser 

byte mac_address[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
byte ip_address[] = {192, 168, 88, 19};
byte gateway[] = {192, 168, 88, 1};
byte subnet[] = {255, 255, 255, 0};

String *tags_temp, *ids_temp;
String readString;

File tags_database;

void setup(){
  
  Serial.begin(9600);           
  Serial.println("Inicializando o sistema");
  SPI.begin();                 // Inicia a comunicação SPI
  //rfid.PCD_Init();             // Inicia o sensor RFID
  Ethernet.begin(mac_address, ip_address, gateway, subnet);
  server.begin();
  SD.begin(SD_SS_PIN);         // Inicia o leitor de cartão microSD
  // Caso o SD conflite no barramento SPI, ele deve ser desligado e ligado apenas quando o SD for ser utilizado
  Serial.println("Ethernet, SD, e barramento SPI configurados e setados");
  
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

  tags_database = SD.open("database.csv", FILE_WRITE);

  if(tags_database && tags_database.size() == 0){
    tags_database.write("uid;nome\n");
    tags_database.close();
  }
  
  DatabaseDump();

  Serial.println("Tags dumpados para os vetores temporários");
}
 
void loop() {
  bool RFID_Found = false;                // Variável auxiliar para controle de interface do usuário
  String strID = "";                      // Variável para guardar o uid lido

  EthernetClient client = server.available();
  ClientAdmin(client); 
  
  /*
  if(!rfid.PCD_PerformSelfTest()){
    // Performa selftest no sensor RFID, caso falhe a porta é mantida aberta
    PermanentStateError();
  }
  
  strID = read_UID();
  */

  for(int i = 0; i < TAG_COUNT; i++){
    if(strID == tags_temp[i]){ // Se a tag estiver no vetor de tags (foi cadastrada previamente) a porta abre
      RFID_Accepted();
      RFID_Found = true;
      break;
    }
  }
  if(!RFID_Found) RFID_Rejected();
  
  //rfid.PICC_HaltA();
  //rfid.PCD_StopCrypto1();
}

/*
String read_UID(){
  String uid = "";

  while(!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()){}
  
  for (byte i = 0; i < 4; i++) {
    uid += String(rfid.uid.uidByte[i], HEX);     // Realiza o parser da tag lida pelo sensor em um string
  }
  uid.toUpperCase();
  
  return uid;
}
*/

void ClientAdmin(EthernetClient client){
  if(client){
    Serial.println("Client web OK");
    while(client.connected() && client.available()){
      char c = client.read();
      Serial.println(c);
      if (readString.length() < 100) readString += c;  
      if (c == '\n') {
        if(readString.indexOf("Nome=")){
          String nome = readString.substring(readString.indexOf("Nome=")+4, readString.indexOf("&"));
          Serial.println(nome);
          if(readString.indexOf("cadastrar=") > 0) writeTagInDatabase(nome);
          else if(readString.indexOf("deletar=")) removeTagInDatabase(nome);
        }
        
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println("<!DOCTYPE HTML><html><head>"); 
        client.println("<link rel='icon' type='image/png' href='https://zenprospect-production.s3.amazonaws.com/uploads/pictures/605c0a99e4a118000171b6b7/picture'/>");
        client.println("<title>Portal WEB do Controle de Acesso</title></head><body style='background-color:#f0f3f5'>");
        client.println("<br/><center><font color='#1262c0'><h1>Controle de Acesso - LASPI</h1></font></center>");
        client.println("<br/><center><form method='get'><label for='nome-input'>Nome associado ao cartão:</label><br><input id='nome-input' name='Nome' placeholder='Nome ou identificação'/><br><br><input type='submit' name='cadastrar' value='Cadastrar'/><input type='submit' name='deletar' value='Deletar'/></form></center>");
        client.println("<br/><br/><center><font color='#1262c0'><h2>Cartões cadastrados atualmente:</h2></font></center><hr/>");
        client.println("<center><table><tr><td><h3>Nome</h3></td><td><h3>UID</h3></td></tr>");
        for(int i = 0; i < cp.getRowsCount(); i++) client.println("<tr><td><h3>"+ids_temp[i]+"</h3></td><td><h3>"+tags_temp[i]+"</h3></td></tr>");
        client.println("</table></center><hr/><br/></body></html>");
        readString="";
        client.stop();
      }
    }
  }
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

void removeTagInDatabase(String nome){
  DatabaseDump();
  SD.remove("database.csv");
  tags_database = SD.open("database.csv", FILE_WRITE);
  for(int i = 0; i < cp.getRowsCount(); i++){
    if(ids_temp[i] != nome) tags_database.write(String(tags_temp[i]+";"+ids_temp[i]+"\n").c_str());
  }
  tags_database.close();
}

void writeTagInDatabase(String nome){
    // Função para escrever a string ID no database.csv e depois atualizar o database externo

    String tag = "";
    
    digitalWrite(RED, LOW);
    digitalWrite(GREEN, LOW);
    digitalWrite(BLUE, HIGH);
    for(int i = 0; i < 10; i++){
      digitalWrite(BLUE, !digitalRead(BLUE));
      delay(200);
    }

    if(cp.getRowsCount() == TAG_COUNT+1){
      ErrorAddingTag();
      return;
    }

    //tag = read_UID();
    for(int i = 0; i < cp.getRowsCount(); i++){
      if(tag == tags_temp[i]){
        ErrorAddingTag();
        return;
      }
    }

    tags_database = SD.open("database.csv", FILE_WRITE);

    if(!tags_database){
      ErrorAddingTag();
      return;
    }

    if(!tags_database.write(String(tag+";"+nome+"\n").c_str())){
      ErrorAddingTag();
      return;
    }
    
    tags_database.close();
    
    // Após escrever a tag no database.csv, dumpa tudo pro tag_temp 
    DatabaseDump();

    digitalWrite(BLUE, LOW);
    digitalWrite(GREEN, HIGH);
    digitalWrite(RED, LOW);
    delay(1500);
    digitalWrite(BLUE, HIGH);
    digitalWrite(GREEN, LOW);
    digitalWrite(RED, LOW);

    return;
}

void ErrorAddingTag(){
  // Pisca 10 vezes em vermelho com intervalos de 0.2s e depois volta ao azul sinalizando erro ao cadastrar
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

void DatabaseDump(){
  cp.readSDfile("database.csv");
  tags_temp = (String *) cp["uid"];
  ids_temp = (String *) cp["nome"];
}

void KeepRelayAlive(){
  // Toda vez que o vetor de interrupção for chamado, o valor no pino KEEP é invertido
  digitalWrite(KEEP_PIN, !digitalRead(KEEP_PIN));
}
