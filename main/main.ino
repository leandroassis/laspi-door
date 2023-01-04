#include <EEPROM.h>
#include <MFRC522.h>
#include <SPI.h>
#include <Ethernet.h>
#include <avr/wdt.h>

/*
Todo:
 - refatorar implementação das tags, carregar diretamente do servidor, caso nao esteja up carrega da eeprom
 - melhorar comunicação com servidor cerberus (sockets que cospem as tags salvas toda vez que liga)
*/

// pinos do barramento SPI
#define RFID_SS_PIN 8       // Chip Enable do sensor rfid
#define ETHERNET_SS_PIN 10  // Chip enable do módulo ethernet
#define SDCARD_SS_PIN 4     // Chip enable do leitor de cartão SD
#define RST_PIN 9           // Reset do barramento SPI

// pinos do sistema de acionamento da tranca
#define RELAY_PIN 2    // Pino de acionar a relé
#define RELAY_CHECK 7  // Pino para checar se a relé está acionando

// pinos da indicação luminosa
#define RED A5      // Pino de controle do led de indicação
#define BLUE A4     // Pino de controle do led de indicação
#define GREEN A3    // Pino de controle do led de indicação
#define LED_GND A2  // Pino de GND para os leds de indicação

// configurações de reset do sistema
#define RESET_TIME 3600000    // 1 hora
void (*resetFunc)(void) = 0;  // função de reset

#define TAG_COUNT 45  // Quantidade de cartões que podem ser gravados

// Configurações dos periféricos
MFRC522 rfid(RFID_SS_PIN, RST_PIN);  // inicialização do sensor RFID

byte mac_address[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };  // configurações de rede
byte server[] = { 172, 16, 15, 74 };                          // IP do servidor

String tags_temp[TAG_COUNT];  // Buffer para manter as tags salvas na EEPROM
int free_address;             // ponteiro para o último endereço livre na memória

unsigned long start_time;  // variável para o tempo de inicio do programa

bool EstadoPorta = false;  // porta trancada (true) ou aberta (false)

void setup() {
  start_time = millis();  // Seta o tempo de inicio para reboot regular
  wdt_enable(WDTO_8S);    // Seta o tempo de reset do WDT em 8 segundos

  // Inicialização dos periféricos
  pinMode(ETHERNET_SS_PIN, OUTPUT);
  pinMode(SDCARD_SS_PIN, OUTPUT);

  digitalWrite(ETHERNET_SS_PIN, LOW);
  digitalWrite(SDCARD_SS_PIN, HIGH);

  SPI.begin();
  rfid.PCD_Init();
  Ethernet.begin(mac_address);

  // Inicialização do pinout
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(RELAY_CHECK, INPUT);
  pinMode(LED_GND, OUTPUT);
  pinMode(RED, OUTPUT);
  pinMode(BLUE, OUTPUT);
  pinMode(GREEN, OUTPUT);

  digitalWrite(RED, LOW);
  digitalWrite(BLUE, HIGH);
  digitalWrite(GREEN, LOW);
  digitalWrite(LED_GND, LOW);

  EEPROMDump();
  digitalWrite(RELAY_PIN, HIGH);
}

void loop() {
  bool RFID_Found = false;  // Variável auxiliar para controle de interface do usuário
  String strID = "";        // Variável para guardar o uid lido

  wdt_reset();
  if ((millis() >= start_time + RESET_TIME) || !digitalRead(RELAY_CHECK) || !rfid.PCD_PerformSelfTest()) {
    // se o programa rodar por RESET_TIME, ele reboota para diminuir janela de erros
    // se o sensor RFID não passar no selftest ou a verificação da relé falhar
    resetFunc();
  }

  if (rfid.PICC_IsNewCardPresent() || rfid.PICC_ReadCardSerial()) {
    for (byte i = 0; i < 4; i++) {
      strID += String(rfid.uid.uidByte[i], HEX);  // Realiza o parser da tag lida pelo sensor em um string
    }
    strID.toUpperCase();
  }

  for (int i = 0; i < TAG_COUNT; i++) {
    if (strID == tags_temp[i]) {
      // Se a tag estiver no vetor de tags, abre a porta
      RFID_Accepted();
      RFID_Found = true;
      break;
    }
  }
  if (!RFID_Found) RFID_Rejected();

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

/*
void ClientHandler() {
  EthernetClient client;

  if (client.connect(server, 80)) {

    client.println("HTTP/1.1 200 OK");
    while (client.available()) {
      char c = client.read();

      if (readString.length() < 100) readString += c;
      if (c == '\n') {
        //Serial.println(readString);

        if (readString.indexOf("cadastrar=") > 0) {
          // Le UID, cadastra na eeprom e envia a tag
          //Serial.println("Cadastrar tag");

          digitalWrite(RED, LOW);
          digitalWrite(GREEN, LOW);
          for (int i = 0; i < 10; i++) {
            digitalWrite(BLUE, !digitalRead(BLUE));
            delay(300);
          }

          String tag = "";
          do {
            tag = read_UID();
          } while (tag == "");

          if (!writeTagInEEPROM(tag)) {
            EEPROMDump();
            client.println(tag);
            client.println("OK\n");
          } else client.println("NOP\n");

          digitalWrite(BLUE, LOW);
          digitalWrite(GREEN, HIGH);
          digitalWrite(RED, LOW);
          delay(2500);
          digitalWrite(BLUE, HIGH);
          digitalWrite(GREEN, LOW);
          digitalWrite(RED, LOW);

          //for(int i = 0; i < TAG_COUNT; i++) Serial.println(tags_temp[i]);
        } else if (readString.indexOf("deletar=") > 0) {
          // seta o free_address para 1 e limpa toda a memória eeprom
          //Serial.println("Resetar toda a memória.");
          EEPROM.write(0, 1);
          for (int i = 1; i < 1024; i++) {
            EEPROM.write(i, 0xFF);
            delay(6);
          }
          EEPROMDump();
          client.println("OK");
          //Serial.println("Memória resetada.");
        } else if (readString.indexOf("addtag=") > 0) {
          // Adiciona uma tag na memória
          //Serial.println("Adicionar tag: ");
          //Serial.println(readString.substring(readString.indexOf("addtag=")+7, readString.indexOf("addtag=")+15));
          if (!writeTagInEEPROM(readString.substring(readString.indexOf("addtag=") + 7, readString.indexOf("addtag=") + 15))) {
            EEPROMDump();
            client.println("OK");
          } else client.println("NOP");

          //for(int i = 0; i < TAG_COUNT; i++) Serial.println(tags_temp[i]);
        } else if (readString.indexOf("atualizarserver=") > 0) {
          Serial.println("Atualizar servidor.");
          EEPROMDump();
          for (int i = 0; i < TAG_COUNT; i++) {
            client.println(tags_temp[i]);
          }
          client.println("OK");
          //Serial.println("Servidor atualizado.");
        } else if (readString.indexOf("desligarsistema=") > 0) {
          Serial.println("Desligar sistema");
          client.println("OK");
          client.stop();
          PermanentStateError();
        } else if (readString.indexOf("resetarsistema=") > 0) {
          Serial.println("Resetando...");
          client.println("OK");
          client.stop();
          digitalWrite(ARDUINO_AUTO_RST, LOW);
        }
        readString = "";
        client.stop();
      }
    }
  }
}
*/

void EEPROMDump() {
  // le todos os 1024 endereços da eeprom, faz o parser (monta uma tag a cada 4 bytes lidos) e copia para o vetor tags_temp
  int contador = 0;
  String temp = "";
  for (int j = 0; j < TAG_COUNT; j++) tags_temp[j] = "";
  //Serial.println("Vetor de tags limpo.");

  for (int i = 1; i <= 4 * TAG_COUNT; i++) {  // Dumpa a EEPROM em um array para evitar consultas excessivas
    contador++;
    temp += String(EEPROM.read(i), HEX);
    if (contador == 4) {
      contador = 0;
      temp.toUpperCase();
      tags_temp[i / 4 - 1] = temp;
      Serial.print(temp);
      Serial.print("  ");
      Serial.print(i);
      Serial.print("\n");
      temp = "";
    }
  }
  Serial.println("--------- Vetor de tags ---------");
  for (int i = 0; i < TAG_COUNT; i++) Serial.println(tags_temp[i]);
  //Serial.println("Tags dumpadas com sucesso.");
}

unsigned short writeTagInEEPROM(String ID) {
  // Função para parsear a String ID em 4 bytes e escreve-os em endereços consecutivos na EEPROM
  char temp_char[2];
  free_address = EEPROM.read(0);  // endereço que armazena o proximo endereço livre

  if (free_address == 1 + TAG_COUNT * 4) return 1;

  for (int i = 0; i < TAG_COUNT; i++) {
    if (ID == tags_temp[i]) return 1;  // verifica se a nova tag já não está no buffer (consequentemente na EEPROM)
  }

  for (int i = 0; i < 8; i += 2) {
    temp_char[0] = ID[i];
    temp_char[1] = ID[i + 1];
    EEPROM.write(free_address, strtoul(temp_char, NULL, 16));
    free_address++;
  }

  EEPROM.write(0, free_address);  // atualiza o próximo endereço livre
  return 0;
}

void RFID_Rejected() {
  // Controle do led caso a tag RFID não esteja cadastrado

  if(digitalRead(RELAY_CHECK) != true) resetFunc();

  // Liga o vermelho por 2.5 s
  digitalWrite(RED, HIGH);
  digitalWrite(BLUE, LOW);
  digitalWrite(GREEN, LOW);

  delay(2500);

  // Volta ao estado inicial
  digitalWrite(RED, LOW);
  digitalWrite(BLUE, HIGH);
  digitalWrite(GREEN, LOW);
}

void RFID_Accepted() {
  // Controle do led caso a tag RFID esteja cadastrado

  digitalWrite(RELAY_PIN, LOW); // Desarma a tranca

  // Coloca o led em verde
  digitalWrite(GREEN, HIGH);
  digitalWrite(RED, LOW);
  digitalWrite(BLUE, LOW);

  if(digitalRead(RELAY_CHECK)) resetFunc();

  delay(5000);

  digitalWrite(RELAY_PIN, HIGH); // Arma a tranca

  // Volta o led para o azul 
  digitalWrite(GREEN, LOW);
  digitalWrite(RED, LOW);
  digitalWrite(BLUE, HIGH);

  if(!digitalRead(RELAY_CHECK)) resetFunc();
}  