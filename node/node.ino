#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266TrueRandom.h>
#include <stdlib.h>
#include <Ticker.h>

#define NUMBER_OF_NODES       2
/*
 * Once enabled will print debug messages.
 */
#define DEBUG                 0

/*
 * STASSID will change, depending if node is CH or not.
 */
#ifndef STASSID
#define STASSID "5G server"
#define STAPSK  "teorijazavere"
#endif

/*
 * String length defines. Desired length + 1 for '\0'.
 */
#define UID_LENGTH            2
#define UID_NUMBER_LENGTH     2
#define T_LENGTH              2
#define T_NUMBER_LENGTH       5
#define R_LENGTH              2
#define R_NUMBER_LENGTH       6
#define ROUND_LENGTH          6
#define ROUND_NUMBER_LENGTH   3
#define TIME_LENGTH           5
#define TIME_NUMBER_LENGTH    3
#define CH_LENGTH             3
#define CH_NUMBER_LENGTH      2

/*
 * Change this for each node
 */
#define MY_NAME_IS_NODE       1
/*
 * Node numbers. Two for now [0, 1]
 */
#define NODE_0                0
#define NODE_1                1 

/*
 * Broadcast port to listen to in setup mode
 */
#define BROADCAST_PORT        2000

typedef struct Packet 
{
  char UID[UID_LENGTH];
  char UID_NUMBER[UID_NUMBER_LENGTH];
  char T[T_LENGTH];
  char T_NUMBER[T_NUMBER_LENGTH];
  char R[R_LENGTH];
  char R_NUMBER[R_NUMBER_LENGTH];
  char ROUND[ROUND_LENGTH];
  char ROUND_NUMBER[ROUND_NUMBER_LENGTH];
  char TIME[TIME_LENGTH];
  char TIME_NUMBER[TIME_NUMBER_LENGTH];
  char CH[CH_LENGTH];
  char CH_NUMBER[CH_NUMBER_LENGTH];
} Packet;

typedef struct Node
{
  unsigned char uid;
  float t;
  float r;
  unsigned char round_cnt;
  unsigned char time_cnt;
  unsigned char ch;
} Node;

/* 
 *  On this port number each node will send their data
 */
unsigned int global_write_port = 8888;

/*
 * Number of times node has been cluster head.
 */
unsigned char CH_enable = 1;

/*
 * Current number of round ( 0 to N - 1 )
 */
unsigned short round_cnt = 0;

/*
 * Time at which uc will wake up?
 */
unsigned short time_cnt = 5;

/*
 * Declaration of constants for strings
 */
const char UID[UID_LENGTH] = "U"; 
const char T_letter[T_LENGTH] = "T";
const char R_letter[R_LENGTH] = "R";
const char ROUND[ROUND_LENGTH] = "ROUND";
const char TIME[TIME_LENGTH] = "TIME";
const char CH[CH_LENGTH] = "CH";

/*
 * Probability that node will be cluster head or not.
 * Depends on network topology, energy consumption for 
 * communication or processing etc. For simple test,
 * two nodes will be used.
 */
const float P = 0.5; 

// buffers for receiving and sending data
char packetBuffer[UDP_TX_PACKET_MAX_SIZE + 1]; //buffer to hold incoming packet
char  ReplyBuffer[] = "acknowledged\r\n";       // a string to send back
char rnd_str[7];
Node nodes[NUMBER_OF_NODES];// node 0 will be node running this code,
                            // rest will be other nodes in network.
unsigned char enable_count = 1;

Ticker timeout;

WiFiUDP Udp;

/*
 * Broadcast address for UDP packages
 */
IPAddress broadcast(192, 168, 4, 255);
/*
 * Base station IP address, will always be constant
 */
IPAddress base(192,168,4,1);
void disable_count(void)
{
  enable_count = 0;
}

float calculate_threshold(float P, unsigned char r)
{
  float T;

  T = P/(1 - P*(r % 1/P));

  return T;
}

float random_number(void)
{
  float a;

  a = ESP8266TrueRandom.random(10000);
  a = a/10000;

  return a;
}

void print_packet(struct Packet *ptr)
{
  Serial.print(ptr->UID);
  Serial.print(ptr->UID_NUMBER);
  Serial.print(ptr->T);
  Serial.print(ptr->T_NUMBER);
  Serial.print(ptr->R);
  Serial.print(ptr->R_NUMBER);
  Serial.print(ptr->ROUND);
  Serial.print(ptr->ROUND_NUMBER);
  Serial.print(ptr->TIME);
  Serial.print(ptr->TIME_NUMBER);
  Serial.print(ptr->CH);
  Serial.print(ptr->CH_NUMBER);
  Serial.println(); 
}

const char* create_packet(struct Packet *ptr)
{
  char T_str[T_NUMBER_LENGTH];
  char R_str[R_NUMBER_LENGTH];
  char Cyc_str[ROUND_NUMBER_LENGTH];
  char time_str[TIME_NUMBER_LENGTH];
  char UID_number[UID_NUMBER_LENGTH];
  char CH_number[CH_NUMBER_LENGTH];
  char text[255];

  sprintf(T_str, "%.2f", nodes[MY_NAME_IS_NODE].t);
  sprintf(R_str, "%.3f", nodes[MY_NAME_IS_NODE].r);
  sprintf(Cyc_str, "%d", nodes[MY_NAME_IS_NODE].round_cnt);
  sprintf(time_str, "%d", nodes[MY_NAME_IS_NODE].time_cnt);
  sprintf(UID_number, "%d", nodes[MY_NAME_IS_NODE].uid);
  sprintf(CH_number, "%d", nodes[MY_NAME_IS_NODE].ch);

  strcpy(ptr->UID, UID);
  strcpy(ptr->UID_NUMBER, UID_number);
  strcpy(ptr->T, T_letter);
  strcpy(ptr->T_NUMBER, T_str);
  strcpy(ptr->R, R_letter);
  strcpy(ptr->R_NUMBER, R_str);
  strcpy(ptr->ROUND, ROUND);
  strcpy(ptr->ROUND_NUMBER, Cyc_str);
  strcpy(ptr->TIME, TIME);
  strcpy(ptr->TIME_NUMBER, time_str);
  strcpy(ptr->CH, CH);
  strcpy(ptr->CH_NUMBER, CH_number);

  strcpy(text, ptr->UID);
  strcat(text, ptr->UID_NUMBER);
  strcat(text, ptr->T);
  strcat(text, ptr->T_NUMBER);
  strcat(text, ptr->R);
  strcat(text, ptr->R_NUMBER);
  strcat(text, ptr->ROUND);
  strcat(text, ptr->ROUND_NUMBER);
  strcat(text, ptr->TIME);
  strcat(text, ptr->TIME_NUMBER);
  strcat(text, ptr->CH);
  strcat(text, ptr->CH_NUMBER);

  return text;
}

void parse_packet(Packet *ptr, char *txt)
{
  char uid[UID_NUMBER_LENGTH];
  char time_cnt[TIME_NUMBER_LENGTH];
  char round_cnt[ROUND_NUMBER_LENGTH];
  char t[T_NUMBER_LENGTH];
  char r[R_NUMBER_LENGTH];
  char ch[CH_NUMBER_LENGTH];
  unsigned char node;
  char *tmp;

  uid[UID_NUMBER_LENGTH - 1] = '\0';
  time_cnt[TIME_NUMBER_LENGTH - 1] = '\0';
  round_cnt[ROUND_NUMBER_LENGTH - 1] = '\0';
  t[T_NUMBER_LENGTH - 1] = '\0';
  r[R_NUMBER_LENGTH - 1] = '\0';
  ch[CH_NUMBER_LENGTH - 1] = '\0';
  
  txt += UID_LENGTH;
  memcpy(uid, txt, sizeof(char));
  node = strtol(uid, &tmp, 10);

  txt += (T_LENGTH - 1) + (UID_NUMBER_LENGTH - 1);
  memcpy(t, txt, (T_NUMBER_LENGTH - 1) * sizeof(char));
  
  txt += (T_NUMBER_LENGTH - 1) + (R_LENGTH - 1);
  memcpy(r, txt, (R_NUMBER_LENGTH - 1) * sizeof(char));

  txt += (R_NUMBER_LENGTH - 1) + (ROUND_LENGTH - 1);
  memcpy(round_cnt, txt, (ROUND_NUMBER_LENGTH - 1) * sizeof(char));

  txt += (TIME_LENGTH - 1) +  (ROUND_NUMBER_LENGTH - 1);
  memcpy(time_cnt, txt, (TIME_NUMBER_LENGTH - 1) * sizeof(char));

  txt += (CH_LENGTH - 1);
  memcpy(ch, txt, (CH_NUMBER_LENGTH - 1) * sizeof(char));

  switch (node) {

    case NODE_0:

    nodes[NODE_0].t = atof(t);
    nodes[NODE_0].r = atof(r);
    nodes[NODE_0].round_cnt = atol(round_cnt);
    nodes[NODE_0].time_cnt = atol(time_cnt);
    nodes[NODE_0].ch = atol(ch);

    break;

    case NODE_1:
    
    nodes[NODE_1].t = atof(t);
    nodes[NODE_1].r = atof(r);
    nodes[NODE_1].round_cnt = atol(round_cnt);
    nodes[NODE_1].time_cnt = atol(time_cnt);
    nodes[NODE_1].ch = atol(ch);
    
    break;
    
  }
}

void setup() {
  float rnd, T;
  Packet packet;
  const char *packet_string;
  char *tmp;
  nodes[NODE_0].uid = NODE_0;
  nodes[NODE_1].uid = NODE_1;

  nodes[MY_NAME_IS_NODE].r = random_number();
  nodes[MY_NAME_IS_NODE].round_cnt = round_cnt;
  nodes[MY_NAME_IS_NODE].t = calculate_threshold(P, nodes[MY_NAME_IS_NODE].round_cnt);
  nodes[MY_NAME_IS_NODE].time_cnt = time_cnt;
  nodes[MY_NAME_IS_NODE].ch = CH_enable;
  
  Serial.begin(9600);
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.print("Connected !\n");

  Udp.begin(BROADCAST_PORT);
  packet_string = create_packet(&packet);

  #if DEBUG
    print_packet(&packet);
    Serial.println(packet_string);
  #endif

  Udp.beginPacket(broadcast, BROADCAST_PORT);
  Udp.write(packet_string);
  Udp.endPacket();
}

void loop() {
  int packetSize = Udp.parsePacket();
    if (packetSize && enable_count) {
      timeout.attach(10, disable_count);
      Serial.printf("Received packet of size %d from %s:%d\n    (to %s:%d, free heap = %d B)\n",
                    packetSize,
                    Udp.remoteIP().toString().c_str(), Udp.remotePort(),
                    Udp.destinationIP().toString().c_str(), Udp.localPort(),
                    ESP.getFreeHeap());
  
      // read the packet into packetBufffer
      int n = Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
      packetBuffer[n] = 0;
      Serial.println("Contents:");
      Serial.println(packetBuffer);
      struct Packet remote;
      parse_packet(&remote, packetBuffer);

#if DEBUG
      Serial.println(nodes[NODE_0].t, 2);
      Serial.println(nodes[NODE_0].r, 3);
      Serial.println(nodes[NODE_0].round_cnt);
      Serial.println(nodes[NODE_0].time_cnt);
      Serial.println(nodes[NODE_0].ch);

      Serial.println(nodes[NODE_1].t, 2);
      Serial.println(nodes[NODE_1].r, 3);
      Serial.println(nodes[NODE_1].round_cnt);
      Serial.println(nodes[NODE_1].time_cnt);
      Serial.println(nodes[NODE_1].ch);
#endif      
/*
      Udp.beginPacket(broadcast, broadcast_port);
      Udp.write(T_letter);
      Udp.write(T_str);
      Udp.write(R_letter);
      Udp.write(rnd_str);
      Udp.endPacket();
*/
    }
}
