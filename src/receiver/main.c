/*
 * main.c
 * Funktion: Interrupt-gesteuerter Empfang von String mit variabler Laenge
 * vorangestellte 2 Byte Pruefsumme
 * Created: 25.06.2015
 * Author: Eick
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>

#include "global.h"
#include "rf12.h"

#define LED1    6
#define RELAY   5                       // = LED2 on Pollin-Board v1.2 

// Pins for the slide switch
#define SWITCH_AUTO     PD7   // Position on1 (auto => timer function) at Pin 13
#define SWITCH_ON       PD4   // Position on2 (permanent on) at Pin 6

// Pins for the push buttons
#define BUTTON_S1       PC4   // Manual trigger button at Pin 27
#define BUTTON_S2       PD3   // Manual reset/off button at Pin 5

// Pins for the DIP switches
#define DIP1            PC0   // DIP switch 1 at Pin 23
#define DIP2            PC1   // DIP switch 2 at Pin 24
#define DIP3            PC2   // DIP switch 3 at Pin 25
#define DIP4            PC3   // DIP switch 4 at Pin 26

// Operational mode definitions
#define MODE_TIMED      0
#define MODE_TOGGLE     1

int main(void)
{
    rf12_init();                        // Initialize RFM12 registers
    rf12_setfreq(RF12FREQ(433.67));     // Set frequency to 433.67 MHz
    rf12_setbandwidth(4, 1, 4);         // 200kHz bandwidth, -6dB gain
    rf12_setbaud(19200);                // 19200 baud
    rf12_setpower(0, 6);                // 1mW power, 120kHz frequency shift

    // Hardware setup: Relay explicitly OFF at startup (inverted logic: HIGH = OFF)
    DDRD |= (1 << LED1) | (1 << RELAY);  
    PORTD |= (1 << RELAY);               // Force Relay OFF immediately
    PORTD &= ~(1 << LED1);              // LED1 OFF

    // Configure switch pins as inputs and enable internal pull-ups
    DDRD &= ~((1 << SWITCH_AUTO) | (1 << SWITCH_ON));   // Set as input
    PORTD |= (1 << SWITCH_AUTO) | (1 << SWITCH_ON);     // Enable pull-ups

    // Configure push buttons as inputs and enable pull-ups
    DDRC &= ~(1 << BUTTON_S1);
    PORTC |= (1 << BUTTON_S1);
    DDRD &= ~(1 << BUTTON_S2);
    PORTD |= (1 << BUTTON_S2);

    // Configure DIP switches (Port C) as inputs and enable pull-ups
    DDRC &= ~((1 << DIP1) | (1 << DIP2) | (1 << DIP3) | (1 << DIP4));
    PORTC |= (1 << DIP1) | (1 << DIP2) | (1 << DIP3) | (1 << DIP4);

    _delay_ms(100);                     // Let lines settle

    uint16_t relay_timer = 0;           // Countdown timer for the relay
    uint16_t max_timer_interval = 750;  // Default interval (30s)
    uint8_t op_mode = MODE_TIMED;       // Operational mode tracker

    rf12_rxstart();
    while(1)
    {
        PORTD &= ~(1 << LED1);   // LED1 off at loop start

        // ---------------------------------------------------------------------
        // DYNAMIC CONFIGURATION VIA DIP SWITCHES
        // ---------------------------------------------------------------------
        if ((PINC & (1 << DIP1)) && (PINC & (1 << DIP2)))
        {
            max_timer_interval = 750;   // DIP1=OFF (HIGH), DIP2=OFF (HIGH) -> 30s
            op_mode = MODE_TIMED;
        }
        else if ((PINC & (1 << DIP1)) && !(PINC & (1 << DIP2)))
        {
            max_timer_interval = 1125;  // DIP1=OFF (HIGH), DIP2=ON (LOW) -> 45s
            op_mode = MODE_TIMED;
        }
        else if (!(PINC & (1 << DIP1)) && (PINC & (1 << DIP2)))
        {
            max_timer_interval = 1500;  // DIP1=ON (LOW), DIP2=OFF (HIGH) -> 60s
            op_mode = MODE_TIMED;
        }
        else
        {
            op_mode = MODE_TOGGLE;      // DIP1=ON (LOW), DIP2=ON (LOW) -> Toggle Mode
        }

        // ---------------------------------------------------------------------
        // MODE 1: SLIDE SWITCH ALWAYS ON (SWITCH_ON is pulled to GND)
        // ---------------------------------------------------------------------
        if (!(PIND & (1 << SWITCH_ON)))  
        {
            PORTD &= ~(1 << RELAY);      // Turn relay ON (inverted logic)
            relay_timer = 0;             // Clear timer
        }
        
        // ---------------------------------------------------------------------
        // MODE 2: AUTOMATIC (SWITCH_AUTO is pulled to GND)
        // ---------------------------------------------------------------------
        else if (!(PIND & (1 << SWITCH_AUTO)))
        {
            // Check Button S1: Manual trigger / Toggle
            if (!(PINC & (1 << BUTTON_S1)))
            {
                PORTD |= (1 << LED1);   // Visual feedback
                
                if (op_mode == MODE_TOGGLE)
                {
                    PORTD ^= (1 << RELAY); // Invert relay state directly
                }
                else
                {
                    PORTD &= ~(1 << RELAY); // Turn relay ON
                    relay_timer = max_timer_interval;
                }
                
                _delay_ms(40);
                while (!(PINC & (1 << BUTTON_S1))) { _delay_ms(10); } // Wait for release
            }

            // Check Button S2: Manual reset
            if (!(PIND & (1 << BUTTON_S2)))
            {
                PORTD |= (1 << RELAY);   // Force OFF immediately
                relay_timer = 0;
            }

            // Standard RF reception handling
            if(RF12_status.Rx == 0)
            {
                cbi(GICR, INT0);    // Disable INT0 to prevent further interrupts until handled

                // 
                rf12_trans(0xCA80);  // FIFO reset => clear FIFO buffer => Interrupt flag cleared
                rf12_trans(0x8208);  // Receive off

                uint16_t sum;
                memcpy(&sum, RF12_Data, 2);

                // Validate checksum and expected data pattern
                if((sum == checksum(RF12_Data+2)) && (strncmp((char*)(RF12_Data+2), "0123456789", 10) == 0))
                {
                    PORTD |= (1 << LED1);           // Success blink
                    
                    if (op_mode == MODE_TOGGLE)
                    {
                        PORTD ^= (1 << RELAY);       // Invert relay state via RF
                    }
                    else
                    {
                        PORTD &= ~(1 << RELAY);      // Turn relay ON
                        relay_timer = max_timer_interval;
                    }
                }
                else  // Checksum mismatch or unexpected data => wrong data received or noise
                {
                    PORTD |= (1 << LED1);   // Error indication: double blink
                    _delay_ms(40);
                    PORTD &= ~(1 << LED1);  
                    _delay_ms(80);
                    PORTD |= (1 << LED1);   // Still on error indication        
                }
                sbi(GIFR, INTF0); // Clear the interrupt flag
            }

            // -----------------------------------------------------------------
            // AUTOMATIC MODE TIMER CONTROL
            // -----------------------------------------------------------------
            if (op_mode == MODE_TIMED)
            {
                if (relay_timer > 0)
                {
                    relay_timer--;
                    if (relay_timer == 0)
                    {
                        PORTD |= (1 << RELAY);       // Timeout reached -> Relay OFF
                    }
                }
                else
                {
                    PORTD |= (1 << RELAY);           // Ensure relay remains OFF when timer is 0
                }
                
                if ((GICR & (1 << INT0)) == 0)      // If INT0 is disabled, re-enable it after a short delay
                {
                    _delay_ms(10);      // Small hardware stabilization delay
                    rf12_rxrestart();   // Cleanly restart the RFM12 module for reception
                }
            }
        }
        
        // ---------------------------------------------------------------------
        // MODE 3: SLIDE SWITCH OFF (Middle position, both pins are HIGH)
        // ---------------------------------------------------------------------
        else
        {
            PORTD |= (1 << RELAY);       // Force relay OFF
            relay_timer = 0;
        }

        _delay_ms(40); // Main loop timebase
    }
}