#define F_CPU 1000000UL

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

#define TIME_PUMP_ON 900
#define TIME_RESET 16374
#define TIME_LSL_ON 180

volatile int ms, sec;
uint8_t alm_fl, alm_fl_3s, buz_fl;
uint8_t pump_on_trig;
int SW;
int time_pump_on;

void setup(void) {
  //НАСТРОЙКА ПОРТОВ-------------
  DDRB = (1 << RELAY) | (1 << BUZZER) | (0 << LSL) | (0 << LSH);
  DIDR0 = (1 << ADC1D); // запрещаем цифровой вход на ноге аналогового входа (PB2)
  PORTB = 0x00;
  //НАСТРОЙКА АЦП----------------        
  ADMUX = (1 << ADLAR) | (1 << MUX0); // опорное напряжение - VCC, левое ориентирование данных, выбран вход ADC1 (SWITCH)
  ADCSRA = (1 << ADEN) | (1 << ADSC) | (1 << ADATE) | (1 << ADPS1); // АЦП включен, запуск преобразования,режим автоизмерения, частота CLK/4
  ADCSRB = 0x00; // режим автоизмерения: постоянно запущено
  //НАТСРОКА ТАЙМЕРА-------------
  TCCR0A = (1 << WGM01);
  TCCR0B = (1 << CS01);
  TIMSK0 = (1 << OCIE0A);
  OCR0A = 125;
  //ОБНУЛЯЕМ ВСЕ, ЧТО НАДО-------
  
  ms = 0;
  sec = 0;
  time_pump_on = 0;
  pump_on_trig = 0x00;
  //настраиваем watchdog
  wdt_enable(WDTO_8S);
  //WDTCR = 1 << WDTIE;
  sei();
}

//работаем с watchdog'ом
/*
watchdog сьрасывается в конце каждого цикла. без прерываний.
ISR(WDT_vect){
  wdt_reset();
  WDTCR = 1 << WDTIE;
}
*/
//считаем секунды
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




void main(void) {
    setup();
    _delay_ms(10);
    
    alm_fl = 0x00;
    
    while (1) {

/*
      if ((PINB & (1 << LSH))|(PINB & (1 << LSL))) {
        PORTB |= (1 << BUZZER);
      } else { PORTB &= ~(1 << BUZZER);}
*/

//ADC CONVERTION
      SW = 0;
      for (uint8_t i = 0; i < 8; i++) {  
      SW += ADCH << 2;
      _delay_ms(5);
      }
      SW = SW >> 3;
//END ADC CONVERSION

      if (SW > 900) {                           //PUNP ON
        PORTB |= (1 << RELAY);
        alm_fl = 0x00;
      } else if ((SW > 400) && (SW < 600)) {    //AUTO
//main programm
        if ((PINB & (1 << LSH)) && (PINB & (1 << LSL))) {
          alm_fl = 0x01;
        } else if (PINB & (1 << LSH)) {
          PORTB &= ~(1 << RELAY);
          alm_fl = 0x00;
        } else if (PINB & (1 << LSL)) {
          PORTB |= (1 << RELAY);       
          alm_fl = 0x00;                 
        } else { alm_fl = 0x00; }
//end main programm            
      } else if (SW < 100) {                    //PUMP OFF
        PORTB &= ~(1 << RELAY);
        alm_fl = 0x00;
      } else { alm_fl = 0x01;} 

      
      if (((~pump_on_trig) & 0x01) && ((PINB >> RELAY) & 0x01)) {
        pump_on_trig = 0x01; 
        time_pump_on = sec; 
      }      
      if (((PINB >> RELAY) & 0x01) && (sec - time_pump_on > TIME_PUMP_ON) {
        alm_fl = 0x01;
      }
// CONST ALARM //      
      if ((alm_fl) && (buz_fl)) {
        PORTB |= (1 << BUZZER);
      } else {
        PORTB &= ~(1 << BUZZER);
      }

//сброс watchdog'а или перезагрузка при непрерывной работе дольше 16374 сек      
      if ((sec < TIME_RESET) || ((PINB >> RELAY) & 0x01)){
      wdt_reset();
      }
      //WDTCR = 1 << WDTIE;
    }
}



