/* Name: main.c
 * Author: Nathan Witmer
 * Copyright: 2015 Nathan Witmer
 * License: MIT (see LICENSE)
 */

#include <avr/io.h>
#include <util/delay.h>

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "softuart.h"

#define PWM_DDR OC1B_DDR
#define PWM_BIT OC1B_BIT
#define PWM_COM_BIT COM1B1
#define PWM_REGISTER OCR1B
#define DELAY 10

void check_serial()
{
  char c;
  if ( softuart_kbhit() ) {
    c = softuart_getchar();
    softuart_putchar('-');
    softuart_putchar('>');
    softuart_putchar('[');
    softuart_putchar(c);
    softuart_putchar(']');
    softuart_putchar('\r');
    softuart_putchar('\n');
  }
}

int main(void)
{
  softuart_init();
  sei();

  softuart_puts_P( "\r\nready.\r\n" );

  PWM_DDR |= _BV(PWM_BIT);
  TCCR1A   |= _BV(PWM_COM_BIT) | _BV(WGM10);
  TCCR1B   |= _BV(CS10) | _BV(WGM12);
  PWM_REGISTER = 0;

  while(1) {
    while(PWM_REGISTER < 128) {
      check_serial();
      PWM_REGISTER++;
      _delay_ms(DELAY);
    }
    while(PWM_REGISTER > 0) {
      check_serial();
      PWM_REGISTER--;
      _delay_ms(DELAY);
    }
  }
  return 0;
}
