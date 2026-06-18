/*
 * main.c
 * Function: Transmit string via RFM12 triggered by waking up the ATtiny2313 
 * via Pin Change Interrupt (PCINT1 on PB1) from Power-Down mode.
 * Created: 2026-06-18
 * Author: Eick
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <string.h>
#include <util/delay.h>
#include "global.h"
#include "rf12.h"

#define LED1     6   // PD6
#define LED2     5   // PD5
#define BUTTON1  1   // PB1 (Pin 13 on ATtiny2313) is used as wakeup button

void send(void);

// Pin Change Interrupt Service Routine
// This handles all pin changes on PORTB
ISR(PCINT_vect)
{
    // Waking up is handled automatically. 
    // Code resumes right after sleep_cpu().
}

int main(void)
{
    // 1. Configure LEDs on PORTD as Output
    DDRD |= (1 << LED1) | (1 << LED2);  
    
    // 2. Configure Button Pin (PB1) as Input and enable internal Pull-Up
    DDRB &= ~(1 << BUTTON1);            
    PORTB |= (1 << BUTTON1);            

    // 3. Configure Pin Change Interrupt for PB1
    PCMSK |= (1 << PCINT1);             // Enable PCINT1 (specifically for pin PB1)
    GIMSK |= (1 << PCIE);               // Enable general Pin Change Interrupts

    // 4. Set Sleep Mode to Power-Down
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);

    // Enable Global Interrupts
    sei();

    // Initialize RFM12 Module
    rf12_init();                        
    rf12_setfreq(RF12FREQ(433.67));     
    rf12_setbandwidth(4, 1, 4);         
    rf12_setbaud(19200);                
    rf12_setpower(0, 6);                

    while(1)
    {
        // --- 1. TRANSMIT DATA ---
        send();
        
        // Visual feedback based on RF12_Index
        if(RF12_Index >= 24)
            PORTD |= (1 << LED1);   // LED1 on
        else
            PORTD |= (1 << LED2);   // LED2 on
            
        _delay_ms(10);
        PORTD &= ~(1 << LED1) & ~(1 << LED2); // Turn off both LEDs
        
        // --- 2. DEBOUNCE & WAIT FOR RELEASE ---
        // Wait until the button is released (Pin goes HIGH again)
        _delay_ms(50); 
        while (!(PINB & (1 << BUTTON1)))
        {
            _delay_ms(10);
        }
        _delay_ms(50); // Additional bounce protection

        // --- 3. GO TO SLEEP (POWER-DOWN) ---
        rf12_trans(0x8208); // Shut down RFM12 internal power stage

        sleep_enable();
        sleep_cpu();        // MCU enters deep sleep here.
                            // Wakes up asynchronously when PB1 changes level.
        sleep_disable();    // Execution continues here after wakeup
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