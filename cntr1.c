#define F_CPU 1200000UL

#include <avr/io.h>
#include <stdio.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

#define RELAY PB0
#define BUZZER PB1
#define SWITCH PB2  //ADC1
#define LSH PB3
#define LSL PB4

#define TIME_PUMP_ON 600        //10 мин
#define TIME_RESET 16374        //~4,5 часа
#define TIME_LSL_ON 120         //2 мин

volatile int ms, sec;
uint8_t alm_pump_on, alm_lsl_on, alm_on, alm_3s_fl, alm_3s_on, buz_fl;
uint8_t pump_on_trig, lsl_on_trig, alm_3s_trig;
uint8_t mode;  //режим работы 
int SW;
int pump_on_time;
int lsl_on_time;
int alm_3s_time;


ISR(TIM0_COMPA_vect){
  ms++;
  if (ms > 999) {
    sec++;
    ms = 0;
  }
  if ((ms == 500) || (ms == 0)) {
    buz_fl ^= (1 << 0);     //генератор импульсов ~0.5сек  
  }
}

void ADC_conv(){
  SW = 0;
  for (uint8_t i = 0; i < 8; i++) {  
    SW += ADCH << 2;
    _delay_ms(5);
  }
  SW = SW >> 3;
  if (SW > 900) {                         
    mode = 0x02;    //PUNP ON
  } else if ((SW > 400) && (SW < 600)) {    
    mode = 0x01;     //AUTO           
  } else if (SW < 100) {                    
    mode = 0x00;     //PUMP OFF
  } else { 
    mode = 0x03;    //ERROR
  } 
}

void alarm_on(){
  if (buz_fl) {
    PORTB |= (1 << BUZZER);
  } else {
    PORTB &= ~(1 << BUZZER);
  }
}

void alarm_off(){
  PORTB &= ~(1 << BUZZER);
}
 
void wdt_rst(){
  if ((sec < TIME_RESET) || ((PINB >> RELAY) & 0x01)){
    wdt_reset();
  }
}

void alarm_3s_control() {
  if ((((1 << LSL) & PINB) && ((1 << LSH) & PINB)) || (((~(PINB >> LSL) & ~(PINB >> LSH)) & 0x01))){
    alm_3s_fl = 0x00;
  } else if (~alm_3s_trig & 0x01) {
    alm_3s_fl = 0x01;
    alm_3s_trig = 0x01;
    alm_3s_time = sec;
  } 
  if ((sec - alm_3s_time < 3) && alm_3s_trig){
    alm_3s_on = 0x01;
  } else {
    alm_3s_on = 0x00;
  }
  if ((sec - alm_3s_time >= 3) && alm_3s_trig && (~alm_3s_fl & 0x01)){
    alm_3s_trig = 0x00;
  }
}

void alarm_main_control() {
//relay control
  if (((PINB >> RELAY) & 0x01) && (~pump_on_trig & 0x01)) {
    pump_on_trig = 0x01;
    pump_on_time = sec;
  }
  if ((~(PINB >> RELAY) & 0x01) && pump_on_trig) {
    pump_on_trig = 0x00;
  }
  //LSL control
  if (((PINB >> LSL) & 0x01) && (~lsl_on_trig & 0x01)) {
    lsl_on_trig = 0x01;
    lsl_on_time = sec;
  }
  if ((~(PINB >> LSL) & 0x01) && lsl_on_trig) {
    lsl_on_trig = 0x00;
  }
  //alarm control    
  if ((PINB & (1 << LSH)) && (PINB & (1 << LSL))) {
    alm_on = 0x01;
  } else
  if (((PINB >> RELAY) & 0x01) && (sec - pump_on_time > TIME_PUMP_ON)){
    alm_on = 0x01;
  } else
  if (((PINB >> LSL) & 0x01) && (sec - lsl_on_time > TIME_LSL_ON)){
    alm_on = 0x01;
  } else {
    alm_on = 0x00;
  }
  //сброс всей сигнализации в ручном режиме
  if ((mode & 0x02) || (((~mode) & 0x01) & (((~mode) >> 1) & 0x01))){
    alm_on = 0x00;
  }
}

void main_prog() {      
  if ((PINB & (1 << LSH)) && (PINB & (1 << LSL))) {     //оба конечника
    PORTB &= ~(1 << RELAY);   
  } else if (PINB & (1 << LSH)) {       //верх
    PORTB &= ~(1 << RELAY);
  } else if (PINB & (1 << LSL)) {       //низ
    PORTB |= (1 << RELAY);                        
  } else {                              //ни одного
  }
}

void setup(void) {
  //НАСТРОЙКА ПОРТОВ-------------
  DDRB = (1 << RELAY) | (1 << BUZZER) | (0 << LSL) | (0 << LSH);
  DIDR0 = (1 << ADC1D); 
  PORTB = 0x00;
  //НАСТРОЙКА АЦП----------------        
  ADMUX = (1 << ADLAR) | (1 << MUX0); 
  ADCSRA = (1 << ADEN) | (1 << ADSC) | (1 << ADATE) | (1 << ADPS1); 
  ADCSRB = 0x00; 
  //НАТСРОКА ТАЙМЕРА-------------
  TCCR0A = (1 << WGM01);
  TCCR0B = (1 << CS01);
  TIMSK0 = (1 << OCIE0A);
  OCR0A = 125;
  //ОБНУЛЯЕМ ВСЕ, ЧТО НАДО-------  
  ms = 0;
  sec = 0;
  pump_on_trig = 0x00;
  //настраиваем watchdog--------
  wdt_enable(WDTO_8S);
  sei();
    
  OSCCAL -= 10;
}

void main(void) {
  setup();
  _delay_ms(10);    
  while (1) {
    ADC_conv();
  
    switch (mode) 
    {
      case 0x00 : //PUMP_OFF
        PORTB &= ~(1 << RELAY);        
        break;
      case 0x01 : //AUTO
        main_prog();
        break;
      case 0x02 : //PUMP_ON
        PORTB |= (1 << RELAY);
        break;
      case 0x03 : //ERROR
        break;
    }


    if (mode == 0x01){            
      alarm_3s_control();
    } else {
      alm_3s_on = 0x00;
    }
    alarm_main_control();

   
    if (alm_on || alm_3s_on || (mode == 0x03)){
      alarm_on();
    } else {
      alarm_off();
    }
    wdt_rst();
  }
}




























