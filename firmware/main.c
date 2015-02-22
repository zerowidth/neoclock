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

static uint8_t grb[PIXELS*3];
static uint8_t xxx, yyy;

void set_pixel(uint8_t i, uint8_t r, uint8_t g, uint8_t b)
{
  uint8_t offset = i*3;
  grb[offset] = g;
  grb[offset+1] = r;
  grb[offset+2] = b;
}

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

void write_pixels(uint8_t pixels) {
  cli();
  asm volatile(
      "1:"                    "\n\t" /* outer loop: iterate bytes */
      "ld %[byte], %a[grb]+"  "\n\t"
      "inc %[yyy]"            "\n\t"
      "ldi %[bits], 8"         "\n\t"
      "2:"                    "\n\t" /* inner loop: write a byte */
      "sbrc %[byte], 7"       "\n\t"
      "inc %[xxx]"            "\n\t"
      "lsl %[byte]"           "\n\t"
      "dec %[bits]"           "\n\t"
      "brne 2b"               "\n\t"
      "dec %[nbytes]"         "\n\t"
      "brne 1b"               "\n\t"
      /* output registers */
      : [xxx] "=d" (xxx)
      , [yyy] "=d" (yyy)
      /* input registers */
      : [nbytes]  "d" (pixels * 3) /* how many pixels to write */
      , [grb]     "w" (grb) /* pointer to grb byte array */
      , [byte]    "r" (grb[0]) /* pointer to grb byte array */
      , [bits]    "r" (8)
      , "0" (xxx)
      , "1" (yyy)
      );
  sei();
}

int main(void)
{
  PIXEL_DDR |= _BV(PIXEL_BIT);

  softuart_init();
  sei();

  softuart_puts_P( "\r\nready.\r\n" );

  set_pixel(0, 0, 1, 0);
  set_pixel(1, 0, 0, 1);

  char *buf = "xxxxxxxx";

  xxx = yyy = 0;
  _delay_ms(50);
  write_pixels(2);
  _delay_ms(50);
  itoa(xxx, buf, 10);
  softuart_puts(buf);
  softuart_putchar(' ');
  itoa(yyy, buf, 10);
  softuart_puts(buf);
  softuart_putchar('\r');
  softuart_putchar('\n');
  uint8_t i;
  for (i=0; i<6; i++) {
    itoa(grb[i], buf, 2);
    softuart_puts(buf);
    softuart_putchar('\r');
    softuart_putchar('\n');
  }
  softuart_putchar('\r');
  softuart_putchar('\n');

  _delay_ms(1000);
  return 0;
}
