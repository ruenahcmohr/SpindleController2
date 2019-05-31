/* 

spindle rpm regulator, by rue_mohr 

tiny13 @ 4.8Mhz (clear divider fuse)

analog 2 pwm

PB0  (OCRA)         pwm out 
PB1  (OCRB INT0)    shaft tach. (2  pulses/rev)
PB2  (ADC1)         Alarm
PB3  (ADC3)
PB4  (ADC2)         Analog control voltage in
PB5  (ADC0 RESET)


alg:

- pin change interrupt, adds 6555 units to accumulator
- timer, on pwm overflow, multiplies accumulator by 90%
  (this may need to be every few overflows)
 
 88% of 65535 @ 400Hz = 57671 (max rate)
 
 10000 rpm = 47867
   500 rpm = 2403
 
 
 thanks to LKoen from #math for the help working out bucket values.
 
*/
#include <avr/io.h>
#include "avrcommon.h"
#include <interrupt.h>
#include <stdint.h>

#define OUTPUT  1
#define INPUT   0



#define tachHigh()   IsHigh(1, PINB)
#define tachLow()    IsLow(1, PINB)
#define SetAlarm()   SetBit(2, PORTB)
#define ClearAlarm() ClearBit(2, PORTB)

volatile unsigned char pwmFlag;
volatile unsigned int tachCounter; 

void Init_PWM ( void );
void Init_ADC ( void );
void Init_INT0( void );

unsigned int mult0p9( unsigned int i) ;
 
 
int main( void ) {
 
  unsigned char pwmvalue;
  unsigned int AdcValue;
  int error;

  // Set clock prescaler: 0 gives full 4.8 MHz from internal oscillator.
   CLKPR = (1 << CLKPCE);
   CLKPR = 0;  

  DDRB = ((OUTPUT << PB0) | (INPUT << PB1) |(OUTPUT << PB2) |(INPUT << PB3) |(INPUT << PB4) |(INPUT << PB5 ));
  PORTB= (                  (1     << PB1)); 
 
  Init_PWM();
  Init_ADC();
  Init_INT0();

  pwmFlag     = 0;
  tachCounter = 0;

  sei();
  while(1)  {   
           
    while(pwmFlag == 0);  // wait for the pwm to roll over
    pwmFlag = 0;  // we can just clear it
    
    cli();  // dont go changing the accumulator while were doing maths...
    
    tachCounter = mult0p9(tachCounter);
        
    sei();    
       
    // read adc value, restart it    
    while(ADCSRA & (1<<ADSC));  // wait for conversion to end.    
    AdcValue = ADC ; 
    ADCSRA |= _BV(ADSC); // start next conversion

    AdcValue <<= 2;
    AdcValue += 450;  // min speed set

    error =  AdcValue ;     // move the + feedback into our accumulator
    error -= tachCounter ;  // subtract the - feedback  
    error /= 2;             // a bit of scaling, my loop gain is a bit too high otherwise. (1KV at 12V)
    error += 128;           // offset for the 8 bit output
    
    
        	
	
    if (0) {
    } else if (( error) > 160) {  //limit to 160 for the power supply
      pwmvalue = 160;    
    } else if (( error) < 0) {
      pwmvalue = 0;
    } else {
      pwmvalue = error;
    }
    
    

    // update pwm system
    OCR0A = pwmvalue; 

  
  }

}

//---------------------------------------------------------------------------

unsigned int _mult0p9( unsigned int i) {
  unsigned int o = 0;
  
    i >>= 1; o  = i; // i/2
    i >>= 1; o += i; // i/4
    i >>= 1; o += i; // i/8
    i >>= 1; o += i; // i/16
    i >>= 1; o -= i; // i/32
    i >>= 1; o -= i; // i/64
    i >>= 1; o += i; // i/128
    i >>= 1; o += i; // i/256
    i >>= 1; o -= i; // i/512
    i >>= 1; o -= i; // i/1024
    i >>= 1; o += i; // i/2048
    i >>= 1; o += i; // i/4096
    i >>= 1; o -= i; // i/8192
    i >>= 1; o -= i; // i/16384
    i >>= 1; o += i; // i/32768

    return o;
}

unsigned int mult0p9( unsigned int i) {
  unsigned int o;
  
   o =   0;
   o +=  i;   // Add dividend to accumulator (dividend /1.000000)
   i >>= 3;  // Shift dividend right. Shift#1  
   o -=  i;   // Subtract dividend from accumulator (dividend /8.000000)
   i >>= 2;  // Shift dividend right. Shift#4
   o +=  i;   // Add dividend to accumulator (dividend /32.000000)
   i >>= 2;  // Shift dividend right. Shift#6  
   o -=  i;   // Subtract dividend from accumulator (dividend /128.000000)
   i >>= 2;  // Shift dividend right. Shift#8
   o +=  i;   // Add dividend to accumulator (dividend /512.000000)
   i >>= 2;  // Shift dividend right. Shift#10
   o -=  i;   // Subtract dividend from accumulator (dividend /2048.000000)
   i >>= 2;  // Shift dividend right. Shift#12
   o +=  i;   // Add dividend to accumulator (dividend /8192.000000)
   i >>= 2;  // Shift dividend right. Shift#14
   o -=  i;   // Subtract dividend from accumulator (dividend /32768.000000)


    return o;
}

/*
  10krpm = 166.67rps
  4 events/rev = 666.6667Hz
  
*/
ISR( INT0_vect ) {
  tachCounter += 358; //2000; //1150; //6555;
}


ISR( TIM0_OVF_vect ) {    // about 292Hz  !!!???!!!
  static uint8_t countDown;  //and startup delay
  
  countDown--;
  if (!countDown) {  
    pwmFlag++;
    countDown = 64; // compensate for new pwm rate.
  }
}

void Init_INT0( void ) {

  // set up rising edge pin change interrupt for INT0
  //MCUCR |= (1 << ISC01) | (1 << ISC00) ;

  // set up any edge pin change interrupt for int0
  MCUCR |= (0 << ISC01) | (1 << ISC00) ;
  GIMSK |= (1 << INT0 ); 
  

}

void Init_ADC( void ) {
  // VCC voltager ref, ADC2 (PB4) as default input
  ADMUX = 2; //| (1<<ADLAR);
  
  // Enable ADC, ADC prescaler /32
  ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS0) ;

  // Start conversion and wait for it to complete (result not used)
  ADCSRA |= (1<<ADSC);            // Start convertion

}


void Init_PWM() {
  // clear pwm levels
  OCR0A = 0; 
  
  // set up WGM, clock, and mode for timer 0
  TCCR0A = 1 << COM0A1 | 
           1 << COM0A0 | 
           0 << COM0B1 | 
           0 << COM0B0 | 
           1 << WGM00  |
           1 << WGM01  ;
 
 
  // set up WGM, clock, and mode for timer 2 divide by 64
  // 4.8Mhz / 64 / 256 = 292.97Hz
  
  // dec 2017, upping this to /1 to get better switching fitler going.
  // 4.8Mhz / 1 / 256 = 18.75KHz    
 
  TCCR0B = 0 << CS02  |
           0 << CS01  |
           1 << CS00  ;
  
  TIMSK0 |= (1 << TOIE0);  //enable overflow interrupt for accumulator regulation
  
  
 }
 
 


