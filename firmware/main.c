/* Name: main.c
 * Author: Nathan Witmer
 * Copyright: 2015 Nathan Witmer
 * License: MIT (see LICENSE)
 */

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdlib.h> /* for itoa */
#include "softuart.h"

#define PWM_DDR OC1B_DDR
#define PWM_BIT OC1B_BIT
#define PWM_COM_BIT COM1B1
#define PWM_REGISTER OCR1B

#define PIXELS 16
#define PIXEL_PORT  PORTA  // Port of the pin the pixels are connected to
#define PIXEL_DDR   DDRA   // Port of the pin the pixels are connected to
#define PIXEL_BIT   PORTA0 // Bit of the pin the pixels are connected to
static uint8_t xxx, yyy;
{

void zero() {
  uint8_t i;
  cli();
  for(i=0; i < 96; i++) {
    asm volatile(
        "sbi %[port], %[pin]" "\n\t"
        "nop"                 "\n\t"
        "cbi %[port], %[pin]" "\n\t"
        "nop"                 "\n\t"
        "nop"                 "\n\t"
        "nop"                 "\n\t"
        "nop"                 "\n\t"
        "nop"                 "\n\t"
        "nop"                 "\n\t"
        "nop"                 "\n\t"
        ::
        [port] "i" (_SFR_IO_ADDR(PIXEL_PORT)),
        [pin] "i" (PIXEL_BIT));
  }
  sei();
}

void write_pixels(uint8_t byte) {
  asm volatile(
      "1:"
      "sbrc %[byte], 7" "\n\t"
      "inc %[xxx]"      "\n\t"
      "inc %[yyy]"      "\n\t"
      "lsl %[byte]"     "\n\t"
      "dec %[bits]"     "\n\t"
      "brne 1b"         "\n\t"
      /* output registers */
      : [xxx] "=d" (xxx)
      , [yyy] "=d" (yyy)
      /* input registers */
      : [byte] "d" (byte)
      , [bits] "r" (8)
      , "0" (xxx)
      , "1" (yyy)
      );
}

int main(void)
{
  PIXEL_DDR |= _BV(PIXEL_BIT);

  softuart_init();
  sei();

  softuart_puts_P( "\r\nready.\r\n" );

  char *buf = "xxxxxxxx";


  xxx = yyy = 0;
  /* _delay_ms(50); */
  write_pixels(1);
  /* _delay_ms(50); */
  itoa(xxx, buf, 10);
  softuart_puts(buf);
  softuart_putchar(' ');
  itoa(yyy, buf, 10);
  softuart_puts(buf);
  softuart_putchar('\r');
  softuart_putchar('\n');

  _delay_ms(1000);
  return 0;
}
