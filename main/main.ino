#include <EEPROM.h>
#include <MFRC522.h>
#include <Ethernet.h>
#include <SPI.h>
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

// configurações de caracteristicas do sistema
#define RESET_TIME 3600000    // 1 hora
void (*resetFunc)(void) = 0;  // função de reset

#define TAG_COUNT 45        // Quantidade de cartões que podem ser gravados
#define UPDATE_TIME 300000  // 5 minutos

// Configurações dos periféricos
MFRC522 rfid(RFID_SS_PIN, RST_PIN);  // inicialização do sensor RFID
EthernetClient client;

byte mac_address[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };  // configurações de rede
char server[] = "172.16.15.74";                               // IP do servidor

// Variáveis de controle
String tags_temp[TAG_COUNT];  // Buffer para manter as tags durante trabalho
int free_address;             // ponteiro para o último endereço livre na memória

unsigned long start_time;   // variável para o tempo de inicio do programa
unsigned long last_update;  // variável para o tempo da última atualização das tags

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
  Ethernet.init(ETHERNET_SS_PIN);
  Ethernet.begin(mac_address);
  delay(1000);

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

  client.setConnectionTimeout(200);
  // To do: trocar o hardware para uma placa arduino que suporte HTTPS
  if (client.connect(server, 80)) DumpTagsFromServer();  // Se o servidor estiver UP, pega as tags do servidor
  else EEPROMDump();                                     // Copia todas as tags da EEPROM pro buffer
  digitalWrite(RELAY_PIN, HIGH);                         // tranca a porta
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

  // se o servidor estiver UP e passar 5 minutos, atualiza as tags a partir do servidor
  if ((millis() >= last_update + UPDATE_TIME) && (client.connect(server, 80))) DumpTagsFromServer();

  if (rfid.PICC_IsNewCardPresent() || rfid.PICC_ReadCardSerial()) {
    for (byte i = 0; i < 4; i++) {
      strID += String(rfid.uid.uidByte[i], HEX);  // Realiza o parser da tag lida pelo sensor em uma string
    }
    strID.toUpperCase();  // salva a string em uppercase
  }

  for (int i = 0; i < TAG_COUNT; i++) {
    if (strID == tags_temp[i]) {
      // Se a tag estiver no vetor de tags, abre a porta
      RFID_Accepted();
      RFID_Found = true;
      break;
    }
  }
  if (!RFID_Found) RFID_Rejected();  // senão, rejeita o cartão

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

void DumpTagsFromServer() {
  short len;
  byte temp[4];
  byte *buffer = NULL;

  // Request HTTP
  client.println("GET /tags HTTP/1.1");
  client.println("Host: " + String(server));
  client.println("Connection: close");
  client.println();

  len = client.available();  // ve quantos bytes o server respondeu
  if (len > 0) {
    if (len > TAG_COUNT * 5) len = TAG_COUNT * 5;
    buffer = (byte *)calloc(len, sizeof(byte));  // buffer com no max. 225 bytes (45 tags de 4 bytes + \n)
    client.read(buffer, len);                    // escreve os bytes no buffer
  }

  if (sizeof(buffer) == len) {
    free_address = 1;  // seta o ponteiro para o inicio da EEPROM
    for (int i = 0; i < len; i += 5) {
      temp[0] = buffer[i];
      temp[1] = buffer[i + 1];
      temp[2] = buffer[i + 2];
      temp[3] = buffer[i + 3];
      writeTagInEEPROM(String((char *)temp));
    }
  }
  client.stop();           // fecha conexão
  last_update = millis();  // salva o momento da última atualização
  EEPROMDump();            // copia as tags atualizadas da EEPROM pro buffer
}

void EEPROMDump() {
  // le todos os 1024 endereços da eeprom, faz o parser (monta uma tag a cada 4 bytes lidos) e copia para o vetor tags_temp
  int contador = 0;
  String temp = "";
  for (int j = 0; j < TAG_COUNT; j++) tags_temp[j] = "";

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
  for (int i = 0; i < TAG_COUNT; i++) Serial.println(tags_temp[i]);
}

unsigned short writeTagInEEPROM(String ID) {
  // Função para parsear a String ID em 4 bytes e escreve-os em endereços consecutivos na EEPROM
  char temp_char[2];
  free_address = EEPROM.read(0);  // endereço que armazena o proximo endereço livre

  if (free_address == 1 + TAG_COUNT * 4) return 1; // se o próximo endereço livre for além do limite de tags, cancela

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

  if (!digitalRead(RELAY_CHECK)) resetFunc();

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

  digitalWrite(RELAY_PIN, LOW);  // Desarma a tranca

  // Coloca o led em verde
  digitalWrite(GREEN, HIGH);
  digitalWrite(RED, LOW);
  digitalWrite(BLUE, LOW);

  if (digitalRead(RELAY_CHECK)) resetFunc();

  delay(5000);

  digitalWrite(RELAY_PIN, HIGH);  // Arma a tranca

  // Volta o led para o azul
  digitalWrite(GREEN, LOW);
  digitalWrite(RED, LOW);
  digitalWrite(BLUE, HIGH);

  if (!digitalRead(RELAY_CHECK)) resetFunc();
}