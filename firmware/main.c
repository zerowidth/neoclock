/* Name: main.c
 * Author: Nathan Witmer
 * Copyright: 2015 Nathan Witmer
 * License: MIT (see LICENSE)
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "softuart.h"
#include "USI_TWI_Master.h"

#define PIXELS 60
#define PIXEL_OFFSET 37
#define PIXEL_PORT PORTA
#define PIXEL_DDR  DDRA
#define PIXEL_BIT  PORTA0

#define MILLIS_OVERFLOW ((F_CPU / 1000) / 8)

#define RTC_ADDR 0xD0

static uint8_t grb[PIXELS*3];
static uint8_t hour;
static uint8_t minute;
static uint8_t second;
static uint8_t prev_second;

/* How many milliseconds since last second changed */
/* Accessing this directly could be buggy, but not worried for now */
volatile uint16_t millisecond;
static uint16_t prev_max_ms; /* Previous max ms value */

void debug_int(uint32_t value)
{
  char buf[11];
  itoa(value, buf, 10);
  softuart_puts(buf);
}

ISR (TIM1_COMPA_vect)
{
  millisecond++;
}

/* Retrieve the adjusted millisecond value, taking calculated inaccuracy into
 * account by stretching the calculated milliseconds to fit a full second */
uint16_t millis() {
  uint16_t ms = (uint32_t)millisecond * 1000 / prev_max_ms;
  if (ms > 1000) {
    ms = 1000; /* stretching can go too far */
  }
  return ms;
}

/* Keep millisecond count updated based on the last-retrieved second.
 * Tracks the difference between the RTC and internal millisecond counter.
 * When interrupts aren't disabled, the millis timer is accurate to about 950ms
 * out of every second. When interrupts are disabled while writing to the
 * WS2812B pixels, it's closer to 800ms/sec, and with a 15ms delay each
 * iteration of the main loop, it's closer to 900ms/sec. Even at 800ms/sec, the
 * compensated millis() value makes for smooth interpolation across a second. */
void update_millis()
{
  if (prev_second != second) {
    prev_second = second;
    prev_max_ms = millisecond;
    millisecond = 0;
  }
}

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

  update_millis();
}

void show_time() {
  uint8_t i;
  uint16_t ms;

  for(i=0; i<(PIXELS*3); i++) {
    grb[i] = 0;
  }
  add_color(hour * 5, 32, 0, 0);
  add_color(minute, 0, 32, 0);
  add_color(second, 0, 0, 32);

  ms = millis() * 3 / 50;
  add_color(ms, 24, 8, 0);

  write_pixels();
}

int main(void)
{
  PIXEL_DDR |= _BV(PIXEL_BIT);
  USI_TWI_Master_Initialise();
  softuart_init();

  OCR1AH = (MILLIS_OVERFLOW >> 8);
  OCR1AL = (uint8_t)MILLIS_OVERFLOW;
  /* CTC mode, with /8 prescaler */
  TCCR1B |= (1 << WGM12) | (1 << CS11);
  /* interrupt on OCR1A match */
  TIMSK1 |= (1 << OCIE1A);
  sei();

  softuart_puts_P( "ready.\r\n" );

  while(1) {
    get_time();
    show_time();
  }

  return 0;
}
