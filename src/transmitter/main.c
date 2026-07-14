/*
 * main.c
 * Function: Transmit string via RFM12 instantly upon hardware power-up
 *           (Variant 1: Pure mechanical power switch, 0.00 µA standby).
 * Created: 2026-07-11
 * Author: Eick
 */ 

#include <avr/io.h>
#include <string.h>
#include <util/delay.h>
#include "global.h"
#include "rf12.h"

void send(void);

int main(void)
{
    // 0. Configure PB1 as an output and set it to HIGH immediately,
    // to electronically latch the power supply. (Self-holding)
    DDRB  |= (1 << PB1);
    PORTB |= (1 << PB1);

    //_delay_ms(5); // Allow the power supply to stabilize before proceeding with the rest of the initialization.

    /*
    // Debugging: N-Ch MOSFET gate test (PB1) - duty cycle: 10 ms HIGH, 10 ms LOW = 50% ~ 1,5 Volt
    while(1)
    {
        PORTB |= (1 << PB1);
        _delay_ms(10);
        PORTB &= ~(1 << PB1);
        _delay_ms(10);
    }
    */
    // 1. Stabilize all unused pins (enable pull-ups)
    PORTA = 0xFF;
    PORTB |= 0xFF;
    PORTD = 0xFF;

    // 2. Configure SPI pins for the RFM12 as outputs (PB5 = MOSI, PB7 = SCK)
    DDRB |= (1 << PB5) | (1 << PB7);
    PORTB &= ~((1 << PB5) | (1 << PB7));

    // 3. Initialize the RFM12 module
    rf12_init();                        
    rf12_setfreq(RF12FREQ(433.67));     
    rf12_setbandwidth(4, 1, 4);         
    rf12_setbaud(19200);                
    rf12_setpower(0, 6);                

    // Allow the RFM12 crystal oscillator a brief moment to stabilize on cold start
    _delay_ms(5); 

    // 4. FIRE DATA IMMEDIATELY
    send();

    // Wait until the radio module has physically finished transmitting the data.
    // A short safety delay prevents the module from switching off in the middle of a TX burst.
    _delay_ms(10);

    // 5. SELF-POWER-OFF TRIGGER
    // Pull PB1 to LOW. N-channel and P-channel transistors are turned off; the circuit is de-energised.
    PORTB &= ~(1 << PB1);

    // 6. TERMINAL STATE (If residual charge in the electrolytic capacitors keeps the CPU alive for a short while)
    while(1)
    {
        // The CPU will run out of power completely within the next few milliseconds.
    }
}

void send(void)
{   
    unsigned char dataPacket[RF12_DataLength+2];
    char str[] = "01234567890123456789";
    uint16_t sum = checksum(str);

    memcpy(dataPacket, &sum, 2);    
    strcpy(dataPacket+2, str);
    
    rf12_txdata(dataPacket);    
}