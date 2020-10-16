#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266TrueRandom.h>

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

struct Packet 
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
};

/*
 * Broadcast port to listen to in setup mode
 */
unsigned int broadcast_port = 2000;

/* 
 *  On this port number each node will send their data
 */
unsigned int global_write_port = 8888;

/*
 * Number of rounds, for now two rounds.
 */
unsigned char N = 2;

/*
 * Number of times node has been cluster head.
 */
unsigned char times_CH = 0;

/*
 * Current number of round ( 0 to N - 1 )
 */
unsigned short cycle = 0;

/*
 * Time at which uc will wake up?
 */
unsigned short wake_up_time = 5;

/*
 * Declaration of constants for strings
 */
const char UID[UID_LENGTH] = "U"; 
const char UID_number[UID_NUMBER_LENGTH] = "1";
const char T_letter[T_LENGTH] = "T";
const char R_letter[R_LENGTH] = "R";
const char ROUND[ROUND_LENGTH] = "ROUND";
const char TIME[TIME_LENGTH] = "TIME";

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



WiFiUDP Udp;

/*
 * Broadcast address for UDP packages
 */
IPAddress broadcast(192, 168, 4, 255);
/*
 * Base station IP address, will always be constant
 */
IPAddress base(192,168,4,1);

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

/*
 * For debugging purposes.
 */
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
  Serial.println(); 
}

const char* create_packet(struct Packet *ptr)
{
  float t,r;
  char T_str[T_NUMBER_LENGTH];
  char R_str[R_NUMBER_LENGTH];
  char Cyc_str[ROUND_NUMBER_LENGTH];
  char time_str[TIME_NUMBER_LENGTH];
  char text[50];

  t = calculate_threshold(P, r);
  sprintf(T_str, "%.2f", t);

  r = random_number();
  sprintf(R_str, "%.3f", r);

  sprintf(Cyc_str, "%d", cycle);
  sprintf(time_str, "%d", wake_up_time);

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

  return text;
}

void setup() {
  float rnd, T;
  struct Packet packet;
  const char *packet_string;
  
  Serial.begin(9600);
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.print("Connected !\n");

  Udp.begin(broadcast_port);
  packet_string = create_packet(&packet);

  Udp.beginPacket(broadcast, broadcast_port);
  Udp.write(packet_string);
  Udp.endPacket();
}

void loop() {
  int packetSize = Udp.parsePacket();
    if (packetSize) {
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
