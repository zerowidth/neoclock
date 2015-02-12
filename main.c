/* Name: main.c
 * Author: Nathan Witmer
 * Copyright: 2015 Nathan Witmer
 * License: MIT
 */

#include <avr/io.h>
#include <util/delay.h>

int main(void)
{
    DDRA |= _BV(DDA0);
    while(1)
    {
        PORTA ^= _BV(PORTA0);
        _delay_ms(500);
    }
    return 0;
}
