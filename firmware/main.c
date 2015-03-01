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
#include "USI_TWI_Master.h"

#define PIXELS 60
#define PIXEL_OFFSET 37
#define PIXEL_PORT PORTA
#define PIXEL_DDR  DDRA
#define PIXEL_BIT  PORTA0

#define RTC_ADDR 0xD0

static uint8_t grb[PIXELS*3];
static uint8_t hour;
static uint8_t minute;
static uint8_t second;

uint8_t add_clamped_color(uint8_t current, uint8_t value)
{
  uint16_t new_value = current + value;
  if (new_value > 255) {
    new_value = 255;
  }
  return (uint8_t)new_value;
}

void set_pixel(uint8_t i, uint8_t r, uint8_t g, uint8_t b)
{
  uint8_t offset = ((i + PIXEL_OFFSET) % PIXELS) * 3;
  grb[offset] = g;
  grb[offset+1] = r;
  grb[offset+2] = b;
}

void add_color(uint8_t i, uint8_t r, uint8_t g, uint8_t b)
{
  uint8_t offset = ((i + PIXEL_OFFSET) % PIXELS) * 3;
  grb[offset]   = add_clamped_color(grb[offset], g);
  grb[offset+1] = add_clamped_color(grb[offset+1], r);
  grb[offset+2] = add_clamped_color(grb[offset+2], b);
}

void write_pixels() {
  cli();
  asm volatile(
      "1:"                    "\n\t" /* outer loop: iterate bytes */
      "ld %[byte], %a[grb]+"  "\n\t"
      "ldi %[bits], 8"        "\n\t"
      "2:"                    "\n\t" /* inner loop: write a byte */
      "sbi %[port], %[pin]"   "\n\t" /* t = 0 */
      "sbrs %[byte], 7"       "\n\t" /* 2c if skip, 1c if no skip */
      "cbi %[port], %[pin]"   "\n\t" /* 2c, t1 = 375ns */
      "lsl %[byte]"           "\n\t" /* 1c, t = 500 (0) / 375 (1) */
      "dec %[bits]"           "\n\t" /* 1c, t = 625 (0) / 500 (1) */
      "nop"                   "\n\t" /* 1c, t = 750 (0) / 625 (1) */
      "cbi %[port], %[pin]"   "\n\t" /* 2c, t2= 875 (0) / 750 (1) */
      "brne 2b"               "\n\t" /* 2c if skip, 1c if not */
      "dec %[nbytes]"         "\n\t"
      "brne 1b"               "\n\t"
      :: [nbytes]  "d" (PIXELS * 3)  /* how many pixels to write */
      ,  [grb]     "w" (grb)         /* pointer to grb byte array */
      ,  [byte]    "r" (0)
      ,  [bits]    "r" (8)
      ,  [port]    "i" (_SFR_IO_ADDR(PIXEL_PORT))
      ,  [pin]     "i" (PIXEL_BIT)
      );
  _delay_us(10);
  sei();
}

uint8_t bcd_to_dec(uint8_t bcd) {
  return (bcd >> 4) * 10 + (bcd & 0x0F);
}

void get_time() {
  uint8_t xfer[4];
  uint8_t status;

  xfer[0] = RTC_ADDR;
  xfer[1] = 0;

  if(!USI_TWI_Start_Transceiver_With_Data(xfer, 2)) {
    status = USI_TWI_Get_State_Info();
    softuart_puts_P("write: ");
    softuart_putchar(status + 48);
    softuart_puts_P("\r\n");
    return;
  }

  xfer[0] = RTC_ADDR | 1;
  if(USI_TWI_Start_Transceiver_With_Data(xfer, 4)) {
    second = bcd_to_dec(xfer[1]);
    minute = bcd_to_dec(xfer[2]);
    hour   = bcd_to_dec(xfer[3]);
  }
  else {
    status = USI_TWI_Get_State_Info();
    softuart_puts_P("read: ");
    softuart_putchar(status + 48);
    softuart_puts_P("\r\n");
  }
}

void show_time() {
  uint8_t i;

  for(i=0; i<(PIXELS*3); i++) {
    grb[i] = 0;
  }
  add_color(hour * 5, 32, 0, 0);
  add_color(minute, 0, 32, 0);
  add_color(second, 0, 0, 32);

  write_pixels();
}

int main(void)
{
  PIXEL_DDR |= _BV(PIXEL_BIT);
  USI_TWI_Master_Initialise();
  softuart_init();
  sei();

  softuart_puts_P( "ready.\r\n" );

  while(1) {
    get_time();
    show_time();
  }

  return 0;
}
