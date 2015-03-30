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
/* #define PIXEL_OFFSET 37 */
#define PIXEL_PORT PORTA
#define PIXEL_DDR  DDRA
#define PIXEL_BIT  PORTA5

#define BTN_PORT PORTB
#define BTN_PINS PINB
#define BTN0 PORTB0
#define BTN1 PORTB1
#define BTN2 PORTB2

#define MILLIS_OVERFLOW ((F_CPU / 1000) / 8)

#define RTC_ADDR 0xD0

#define SCALE(val) ((val) * output_level / 128)
#define PENDULUM_PERIOD 4000
#define HALF_PERIOD 2000

static uint8_t grb[PIXELS*3];
static uint8_t hour;
static uint8_t minute;
static uint8_t second;
static uint8_t prev_second;

typedef enum {UP, DOWN} direction;
static enum {CLOCK, ALT, SERIAL} mode = CLOCK;

static uint8_t btn0_pressed;
static uint8_t btn1_pressed;
static uint8_t btn2_pressed;

/* How many milliseconds since last second changed */
/* Accessing this directly could be buggy, but not worried for now */
volatile uint16_t millisecond;
static uint16_t prev_max_ms = 1000; /* Previous max ms value */

static uint8_t output_level = 1;
static uint8_t draw_pendulum = 1;

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

void timer_init()
{
  OCR1AH = (MILLIS_OVERFLOW >> 8);
  OCR1AL = (uint8_t)MILLIS_OVERFLOW;
  /* CTC mode, with /8 prescaler */
  TCCR1B |= (1 << WGM12) | (1 << CS11);
  /* interrupt on OCR1A match */
  TIMSK1 |= (1 << OCIE1A);
}

void adc_init()
{
  PRR &= ~(1 << PRADC); /* disable ADC powersave */
  ADCSRA |= (1 << ADEN); /* enable ADC */
  ADCSRB |= (1 << ADLAR); /* left-align ADC value, only need 8 bits */
}

void io_init()
{
  PIXEL_DDR |= (1 << PIXEL_BIT); /* pixel output */
  BTN_PORT |= (1 << BTN0) | (1 << BTN1) | (1 << BTN2); /* button input with pullup */
}

void update_light_level()
{
  uint8_t analog_level, light_level;

  ADCSRA |= (1 << ADSC);
  while ( ADCSRA & (1<<ADSC) ) { }
  analog_level = ADCH;

  /* map 0-255 to 0-3, then to 16, 32, 64, or 128, and 8 as a lower bound */
  if (analog_level < 16) {
    light_level = 8;
  }
  else {
    light_level = 1 << (4 + analog_level / 64);
  }

  /* fade output level between calculated light levels */
  if(output_level < light_level) { output_level++; }
  else if(output_level > light_level) { output_level--; }
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
    prev_max_ms = (millisecond + prev_max_ms) / 2; /* smooth it a bit */
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

uint8_t dec_to_bcd(uint8_t dec) {
  return ((dec / 10) << 4) | (dec % 10);
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

void set_time() {
  uint8_t xfer[5];
  uint8_t status;

  xfer[0] = RTC_ADDR;
  xfer[1] = 0;
  xfer[2] = dec_to_bcd(second);
  xfer[3] = dec_to_bcd(minute);
  xfer[4] = dec_to_bcd(hour); /* 0 in bit 6 means 24-hour, which is fine */

  if(!USI_TWI_Start_Transceiver_With_Data(xfer, 5)) {
    status = USI_TWI_Get_State_Info();
    softuart_puts_P("write: ");
    softuart_putchar(status + 48);
    softuart_puts_P("\r\n");
    return;
  }
  millisecond = 0;
}

void change_hour(direction dir) {
  if (dir == UP) {
    if (hour == 23) {
      hour = 0;
    }
    else {
      hour++;
    }
  }
  else {
    if (hour == 0) {
      hour = 23;
    }
    else {
      hour--;
    }
  }
}

void change_minute(direction dir) {
  if (dir == UP) {
    second = 0;
    if (minute == 59) {
      minute = 0;
      change_hour(UP);
    }
    else {
      minute++;
    }
  }
  else {
    if (second > 0) {
      second = 0;
    }
    else {
      if (minute == 0) {
        minute = 59;
        change_hour(DOWN);
      }
      else {
        minute--;
      }
    }
  }
}

void update_buttons()
{
  uint8_t pressed = !(BTN_PINS & (1 << BTN0));
  if(!btn0_pressed && pressed) {
    btn0_pressed = 1;
    mode = ALT;
    draw_pendulum = !draw_pendulum;
  }
  else if(btn0_pressed && !pressed) {
    btn0_pressed = 0;
    mode = CLOCK;
  }

  pressed = !(BTN_PINS & (1 << BTN1));
  if(!btn1_pressed && pressed) {
    btn1_pressed = 1;

    if(mode == ALT) {
      change_minute(DOWN);
    }
    else {
      change_hour(DOWN);
    }

    set_time();
  }
  else if(btn1_pressed && !pressed) {
    btn1_pressed = 0;
  }

  pressed = !(BTN_PINS & (1 << BTN2));
  if(!btn2_pressed && pressed) {
    btn2_pressed = 1;

    if(mode == ALT) {
      change_minute(UP);
    }
    else {
      change_hour(UP);
    }

    set_time();
  }
  else if(btn2_pressed && !pressed) {
    btn2_pressed = 0;
  }
}

void show_time() {
  uint8_t i;
  uint16_t ms = millis();

  uint16_t level;
  uint8_t hour_pos;
  uint32_t pendulum;
  uint16_t period_ms;
  uint8_t pendulum_pos;

  /* clear the clock face */
  for(i=0; i<(PIXELS*3); i++) {
    grb[i] = 0;
  }

  /* second hand: quadratic ease in-out */
  /* first half:  y = (x/500)*(x/500)*64 */
  /* second half: y = 128 - ((x-1000)/500)*(x-1000)/500)*64 */
  if (ms < 500) {
    level = (uint32_t)64 * ms * ms * output_level / 500 / 500 / 128;
  }
  else {
    /* should be (ms - 1000) but the signs cancel so keep it positive */
    level = output_level - (uint32_t)64 * (1000-ms) * (1000-ms) * output_level / 500 / 500 / 128;
  }
  add_color(second, 0, 0, output_level - level);
  add_color(second + 1, 0, 0, level);

  /* minute hand */
  /* 60000 ms -> 128 levels, LCM is 240000: 60k * 4, 128 * 1875 */
  level = (uint32_t)(second * 1000 + ms) * 4 / 1875 * output_level / 128;
  add_color(minute, 0, output_level - level, 0);
  add_color(minute + 1, 0, level, 0);

  /* hour hand */
  /* know the current hour, but need to interpolate across a 5-minute span */
  /* 3600 sec -> 640 level (128 * 5), LCM 28800: 3600 * 8, 640 * 45 */
  level = ((uint32_t)(minute * 60 + second)) * 8 / 45;
  hour_pos = hour * 5 + level / 128;
  level = SCALE(level % 128);
  add_color(hour_pos, output_level - level, 0, 0);
  add_color(hour_pos + 1, level, 0, 0);

  /* pendulum */
  /* 128 levels * 30 pixels = 3840 */
  /* using y=x^2 instead of cosine so no floating point is needed */
  /* y = (x/half_period)^2 * 3840 for first half */
  /* y = 3840*2 - ((x-period)/half_period)^2 * 3840 for second half */
  /* try and preserve precision but don't overflow 32 bytes */
  period_ms = (ms + (second * 1000)) % PENDULUM_PERIOD;
  if (period_ms <= HALF_PERIOD) {
    pendulum = (uint32_t)period_ms * period_ms;
  }
  else {
    pendulum = (uint32_t)period_ms - PENDULUM_PERIOD;
    pendulum = pendulum * pendulum;
  }
  pendulum = pendulum / (uint32_t)HALF_PERIOD;
  pendulum = pendulum * (uint32_t)3840;
  pendulum = pendulum / (uint32_t)HALF_PERIOD;
  if (period_ms > HALF_PERIOD) {
    pendulum = (uint32_t)3840*2 - pendulum;
  }
  pendulum_pos = pendulum / 128;
  /* 128 --> 48/24 (3/8 and 3/16 multipliers) */
  level = (pendulum - pendulum_pos * 128) * 3 / 8;
  if (draw_pendulum) {
    add_color(pendulum_pos, SCALE(48 - level), SCALE(24 - level / 2), 0);
    add_color(pendulum_pos + 1, SCALE(level), SCALE(level / 2), 0);
  }

  /* clock face */
  /* with output levels at 16, 32, 64, or 128: should be 8/4 at 128, 4/2 at 64 */
  for (i=0; i<PIXELS; i+=5) {
    level = output_level / ((i % 15 == 0) ? 16 : 32);
    if(level == 0 && ( output_level > 8 || ( output_level == 8 && i == 0 ) ) ) {
      level = 1;
    }
    add_color(i, level, level, level);
  }

  write_pixels();
}

int main(void)
{
  io_init();
  adc_init();
  USI_TWI_Master_Initialise();
  softuart_init();
  timer_init();
  sei();

  softuart_puts_P( "time begins.\r\n" );

  while(1) {
    update_light_level();
    update_buttons();
    get_time();
    show_time();
  }

  return 0;
}
