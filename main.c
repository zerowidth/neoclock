/* Name: main.c
 * Author: Nathan Witmer
 * Copyright: 2015 Nathan Witmer
 * License: MIT
 */

#include <avr/io.h>
#include <util/delay.h>

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "softuart.h"

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

  OC1A_DDR |= _BV(OC1A_BIT);
  TCCR1A   |= _BV(COM1A1) | _BV(WGM10);
  TCCR1B   |= _BV(CS10)   | _BV(WGM12);
  OCR1A = 0;
  while(1) {
    while(OCR1A < 128) {
      check_serial();
      OCR1A++;
      _delay_ms(DELAY);
    }
    while(OCR1A > 0) {
      check_serial();
      OCR1A--;
      _delay_ms(DELAY);
    }
  }
  return 0;
}
