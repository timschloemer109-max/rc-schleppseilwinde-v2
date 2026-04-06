#define Taster 2
#define LED 13
#define Geschwindigkeit_Pin 3
#define Ausloeser_Pin 5
#define Regler 4
#define Stromsensor A3
#define min_pwm_aktiv 1800
#define min_pwm_reset 1200
#define min_pwm 1000
#define max_pwm 2000
#define langsam_pwm 1390
#define pwm_Testwert 1500.0
#define gelaufene_Meter_test 20.0
#define gelaufene_millisekunden_test 9500.0
#define schnelle_strecke 23.5
#define max_laufzeit 20000






#define max_Strom 25.0
const double vRef = 6.03; 
// Versorgungsspannung

// Variables for Measured Voltage and Calculated Current
double Vout = 0;
double Current = 0;
 
// Constants for Scale Factor
// Use one that matches your version of ACS712
 
//const double scale_factor = 0.185; // 5A
//const double scale_factor = 0.1; // 20A
const double scale_factor = 0.066; // 30A
 
// Constants for A/D converter resolution
// Arduino has 10-bit ADC, so 1024 possible values
// Reference voltage is 5V if not using AREF external reference
// Zero point is half of Reference Voltage
 

const double resConvert = 1024;
double resADC = vRef/resConvert/1.22;
double zeroPoint = vRef/2;


//ms_pro_meter = gelaufene_millisekunden_test/gelaufene_Meter_test
//ms_pro_meter_pro_pwm = ms_pro_meter/pwm_Testwert
//ms_pro_meter_pro_pwm_Geschwindigkeit = ms_pro_meter_pro_pwm*pwm_Geschwindigkeit
//laufzeit = ms_pro_meter_pro_pwm_Geschwindigkeit*schnelle_Strecke

//Laufzeit = (((gelaufene_millisekunden_test/gelaufene_Meter_test)*pwm_Testwert)/Geschwindigkeit)*schnelle_strecke



#include <Servo.h>

Servo Winde;
bool Stop_Trigger = false;
bool Start_Zeit_Trigger = false;
int Empfaengerwert = min_pwm;
float Geschwindigkeit;
int Regler_Ausgang = min_pwm;
float Laufzeit = 0;
unsigned long Start_Zeit = 0;

void setup() 
{
  pinMode(Taster, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  Winde.attach(Regler, min_pwm, max_pwm);
  digitalWrite(LED, LOW);
  
  Serial.begin(9600);
}

void loop() 
{
  Geschwindigkeit = pulseIn(Geschwindigkeit_Pin, HIGH);
  Laufzeit = (((gelaufene_millisekunden_test/gelaufene_Meter_test)*pwm_Testwert)/Geschwindigkeit)*schnelle_strecke;
Serial.print("PWM: ");
Serial.println(digitalRead(Ausloeser_Pin));
Serial.print("Stop Triggger ");
Serial.println(Stop_Trigger);
Serial.println(resADC * analogRead(Stromsensor));
  //STROMSENSOR
  Vout=0;
    // Vout is read 100 Times for precision
  for(int i = 0; i < 10; i++) {
    Vout = (Vout + (resADC * analogRead(Stromsensor)));   
    delay(1);
  }
  
  // Get Vout in mv
  Vout = Vout /10;
  Serial.print("Vout:");
  Serial.println(Vout);
  
  // Convert Vout into Current using Scale Factor
  Current = (Vout - zeroPoint)/ scale_factor; 
    Serial.print("Current:");
    Serial.println(Current);
//Stromsensor Ende


  
  if(!digitalRead(Taster))
  {
    Stop_Trigger = true;
    digitalWrite(LED, HIGH);
  }
  if(pulseIn(Ausloeser_Pin, HIGH) <= min_pwm_reset)
  {
    Start_Zeit_Trigger = false;
    Stop_Trigger = false;
    digitalWrite(LED, LOW);
  }
  if((pulseIn(Ausloeser_Pin, HIGH) >= min_pwm_aktiv) && (Start_Zeit_Trigger == false))
  {
    Start_Zeit_Trigger = true;
    Start_Zeit = millis();
  }
  if((Stop_Trigger == false))
  {
     if(Start_Zeit_Trigger == true)
     {
      Regler_Ausgang = Geschwindigkeit;
      if((millis() - Start_Zeit) >= Laufzeit)
      {
        Regler_Ausgang = langsam_pwm;
        if ((millis() - Start_Zeit) >= max_laufzeit)
        {
          Regler_Ausgang = min_pwm;
        }
      }
     }
  }
  else
  {
    Regler_Ausgang = min_pwm;
  }
if(Current >= max_Strom)
          {
            Regler_Ausgang = min_pwm;
            Stop_Trigger = true;
            digitalWrite(LED, HIGH);
          }
          
  Winde.writeMicroseconds(Regler_Ausgang);
}
