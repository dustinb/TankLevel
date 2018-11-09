// http://wordpress.codewrite.co.uk/pic/2014/01/25/capacitance-meter-mk-ii/
//
// TODO: Need to determine correct calibration methods.  Think need to do the resistance first, then set stray cap?
//
// For our purposes the accurancy is not important.  It's important to measure change in capacitance from our calibrated empty and full values

#include <U8g2lib.h>
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

const int OUT_PIN = A2;
const int OUT_PIN2 = A3;
const int IN_PIN = A0;
const int IN_PIN2 = A1;

//Capacitance between IN_PIN and Ground
//Stray capacitance value will vary from board to board.
//Calibrate this value using known capacitor.
const float IN_STRAY_CAP_TO_GND = 24.5800; // Default 24.48;
const float IN_CAP_TO_GND  = IN_STRAY_CAP_TO_GND;
//Pullup resistance will vary depending on board.
//Calibrate this with known capacitor R = -1 * (t / (C * ln(1 - Vc/Vin))
const float R_PULLUP = 35.143;  //in k ohms
const int MAX_ADC_VALUE = 1023;

// Calibration Values - water bottle
const float FRESH_EMPTY = 12;
const int FRESH_FULL = 78;
const int FRESH_GALLONS = 10;
const float Href = 22;     // Height of reference mm
const float CLev0 = 4.47;  // Capacitance Level at empty
const float CRef0 = 1.33;   // Capacitance Reference at empty
// Calibration Values - Reliance 7 gallon
/*
const float FRESH_EMPTY = 19;
const int FRESH_FULL = 26;
const int FRESH_GALLONS = 7;
*/

void setup() {
  pinMode(OUT_PIN, OUTPUT);
  pinMode(OUT_PIN2, OUTPUT);
  //digitalWrite(OUT_PIN, LOW);  //This is the default state for outputs
  pinMode(IN_PIN, OUTPUT);
  pinMode(IN_PIN2, OUTPUT);
  //digitalWrite(IN_PIN, LOW);

  Serial.begin(9600);

  u8g2.begin();  
  // u8g2.enableUTF8Print();
}

int counter = 0;
    float capRef;
    float capLevel;
    int samples = 10000;
    
void loop() {
    // LM35DZ temperature reading
    // int temp = analogRead(tempPin);
    // float mv = (temp * (5 / 1023.0)) * 1000;
    
    //Capacitor under test between OUT_PIN and IN_PIN
    //Rising high edge on OUT_PIN
    pinMode(IN_PIN, INPUT);
    digitalWrite(OUT_PIN, HIGH);
    int val = analogRead(IN_PIN);
    char unit[3] = "pF";
    digitalWrite(OUT_PIN, LOW);

    pinMode(IN_PIN2, INPUT);
    digitalWrite(OUT_PIN2, HIGH);
    int val2 = analogRead(IN_PIN2);
    digitalWrite(OUT_PIN2, LOW);

    counter++;
    
    // I think results for this method differ greatly from a 3.3v 8Mhz Micro to the 5v 16Mhz Arduinos.  Time between output HIGH and analogRead is 
    // probably different?  Wasn't able to measure any pF capacitor on the 3.3v
    
    if (val < 1000) {
     
      //Low value calnpacitor
      //Clear everything for next measurement
      pinMode(IN_PIN, OUTPUT);
      pinMode(IN_PIN2, OUTPUT);
      //Calculate and print result

      capRef  = capRef + (float)val * IN_CAP_TO_GND / (float)(MAX_ADC_VALUE - val);
      capLevel = capLevel + (float)val2 * IN_CAP_TO_GND / (float)(MAX_ADC_VALUE - val2);

      float level = 0;
      
      if (counter == samples) {

        capRef = capRef / samples;
        capLevel = capLevel / samples;
        counter = 0;
        level = Href * ((capLevel - CLev0) / (capRef - CRef0));
        
        // Formula for percentage (value - bottom) / range
        //float gallons = FRESH_GALLONS * ((capLevel - FRESH_EMPTY) / (FRESH_FULL - FRESH_EMPTY));
      
        strncpy(unit, "pF", 3);

        /*
        Serial.print(F("Capacitance Value = "));
        Serial.print(capLevel, 3);
        Serial.print(F(" pF ("));
        Serial.print(val);
        Serial.println(F(") "));
        */
        
        u8g2.firstPage();
          do {
            u8g2.setFont(u8g2_font_7x14B_tf);
            u8g2.setCursor(0, 12);
            u8g2.print(capRef);
            u8g2.print(" ");
            u8g2.print(capLevel);
            u8g2.print(unit);
            u8g2.setCursor(0, 42);
            //u8g2.print("Fresh: ");
            //u8g2.print(gallons);
            //u8g2.print(" Gal");
            u8g2.setCursor(0, 58);
            u8g2.print(level);
         } while ( u8g2.nextPage() );
      
      }

    } else {
      
      //Big capacitor - so use RC charging method

      //discharge the capacitor (from low capacitance test)
      pinMode(IN_PIN, OUTPUT);
      delay(1);

      //Start charging the capacitor with the internal pullup
      pinMode(OUT_PIN, INPUT_PULLUP);
      unsigned long u1 = micros();
      unsigned long t;
      int digVal;

      //Charge to a fairly arbitrary level mid way between 0 and 5V
      //Best not to use analogRead() here because it's not really quick enough
      do {
        digVal = digitalRead(OUT_PIN);
        unsigned long u2 = micros();
        t = u2 > u1 ? u2 - u1 : u1 - u2;
      } while ((digVal < 1) && (t < 400000L)); 

      pinMode(OUT_PIN, INPUT);  //Stop charging
      //Now we can read the level the capacitor has charged up to
      val = analogRead(OUT_PIN);

      //Discharge capacitor for next measurement
      digitalWrite(IN_PIN, HIGH);
      int dischargeTime = (int)(t / 1000L) * 5;
      delay(dischargeTime);    //discharge slowly to start with
      pinMode(OUT_PIN, OUTPUT);  //discharge remainder quickly
      digitalWrite(OUT_PIN, LOW);
      digitalWrite(IN_PIN, LOW);

      //Calculate and print result
      float capacitance = -(float)t / R_PULLUP
                              / log(1.0 - (float)val / (float)MAX_ADC_VALUE);

      Serial.print(F("Capacitance Value = "));
      if (capacitance > 1000.0) {
        capacitance = capacitance / 1000;
        strncpy(unit, "uF", 3);
      } else {
        strncpy(unit, "nF", 3);
      }

      /*
      Serial.print(capacitance, 2);
      Serial.print(F(" "));
      Serial.print(unit);
      Serial.print(F(" ("));
      Serial.print(digVal == 1 ? F("Normal") : F("HighVal"));
      Serial.print(F(", t= "));
      Serial.print(t);
      Serial.print(F(" us, ADC= "));
      Serial.print(val);
      Serial.println(F(")"));
      */
      
      u8g2.firstPage();
        do {
          u8g2.setFont(u8g2_font_7x14B_tf);
          u8g2.setCursor(0, 42);
          u8g2.print(capacitance);
          u8g2.print(unit);
      } while ( u8g2.nextPage() );

    }
     
    //while (millis() % 50 != 0);    
}
