/* Name: main.c
 * Author: Nathan Witmer
 * Copyright: 2015 Nathan Witmer
 * License: MIT
 */

#include <avr/io.h>
#include <util/delay.h>
#define DELAY 10

int main(void)
{
    OC1A_DDR |= _BV(OC1A_BIT);
    TCCR1A |= _BV(COM1A1) | _BV(WGM10);
    TCCR1B |= _BV(CS10) | _BV(WGM12);
    OCR1A = 0;
    while(1) {
      while(OCR1A < 128) {
        OCR1A++;
        _delay_ms(DELAY);
      }
      while(OCR1A > 0) {
        OCR1A--;
        _delay_ms(DELAY);
      }
    }
    return 0;
}
