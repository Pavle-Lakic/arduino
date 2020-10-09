#include <ESP8266WiFi.h>

void setup(void)
{
  Serial.begin(9600);
  Serial.println();
  WiFi.begin("It burns when IP", "nakacisenamoj");
  Serial.print("Connecting !");

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP address = ");
  Serial.println(WiFi.localIP());
}

void loop(void)
{
  
  unsigned short adc_val;
  adc_val = analogRead(A0);
  Serial.print("ADC value = ");
  Serial.println(adc_val);
  delay(1000);
  
}
