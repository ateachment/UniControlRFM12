/*##############################################################################
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307, USA.
 *
 * @file        rf12.c
 * @brief
 * @author      Benedikt K.
 * @author      Juergen Eckert
 * @author  	Ulrich Radig (mail@ulrichradig.de) www.ulrichradig.de
 * @author  	Wolfhard Eick (w@eick-at.de) www.eick-at.de
 * @date        11.05.2015
##############################################################################*/


#include <avr/io.h>
#include <avr/interrupt.h>
#include "global.h"
#include "rf12.h"

#ifndef F_CPU
#define F_CPU 8000000UL
#endif

#include <util/delay.h>

#define RF_PORT	PORTB
#define RF_DDR	DDRB
#define RF_PIN	PINB


#if defined(__AVR_ATtiny2313__) || defined(__AVR_ATtiny2313A__)
    // pins for ATtiny2313 on Pollin-Board
    #define SDI     5   // PB5 (DI) -> geht an SDO des RFM12
    #define SDO     6   // PB6 (DO) -> geht an SDI des RFM12
    #define SCK     7   // PB7 (USCK)-> geht an SCK des RFM12
    #define nSEL    4   // PB4       -> geht an nSEL des RFM12
#else
    // pins for ATmega8 on Pollin-Board
    #define SDI     3   // MOSI
    #define SDO     4   // MISO
    #define SCK     5   // SCK
    #define nSEL    2   // SS
#endif

// =====================================================================
// GLOBAL VARIABLES
// =====================================================================
volatile unsigned char RF12_Index = 0;
unsigned char RF12_Data[34]; 

#ifdef RF12_INTERRUPT
volatile struct RF12_stati RF12_status;
#endif


// USI Hilfsfunktion zum Senden/Empfangen eines einzelnen Bytes (Hardware-getaktet)
#ifdef USI_MODE
unsigned char usi_xfer(unsigned char data)
{
    USIDR = data;
    USISR = (1<<USIOIF); // Clear Counter Overflow Flag
    
    // Drei-Draht-Modus (SPI), externer Takt (Software-Strobe via USITC)
    while ( !(USISR & (1<<USIOIF)) )
    {
        USICR = (1<<USIWM0)|(1<<USICS1)|(1<<USICLK)|(1<<USITC);
    }
    return USIDR;
}
#endif

unsigned short rf12_trans(unsigned short wert)
{	
	unsigned short werti=0;
	cbi(RF_PORT, nSEL);		// /SS (slave select auf 0)

#ifdef SPI_MODE		//Routine for Hardware SPI
	SPDR = (0xFF00 & wert)>>8;		// erst MSB dem Dataregister zuweisen
	while(!(SPSR & (1<<SPIF))){};  	// Leerschleife solange das End of transmission flag im Statusregister
									// nicht ungleich 0 (also 0) ist.

	werti = (SPDR<<8);				// 1.Byte als MSB sichern
	
	SPDR = (0x00ff & wert);		// LSB dem Dataregister zuweisen
	while(!(SPSR & (1<<SPIF))){};	// warten s.o.
	werti = werti + SPDR;		// 2.Byte als LSB sichern

#else               // Präzises Software SPI (Mode 0) für ATtiny2313
    unsigned char i;
    
    cbi(RF_PORT, nSEL); // Modul aktivieren (LOW)
    _delay_us(5);

    for (i = 0; i < 16; i++)
    {   
        // 1. Datenbit für das RFM12 bereitstellen
        if (wert & 0x8000)
            sbi(RF_PORT, SDO); 
        else
            cbi(RF_PORT, SDO);
            
        wert <<= 1;
        _delay_us(5);       // Setup-Zeit: Signal stabilisieren lassen
        
        // 2. Steigende Flanke: RFM12 liest das Bit ein
        sbi(RF_PORT, SCK);
        _delay_us(10);      // Haltezeit (High-Phase)
        
        // 3. Fallende Flanke: RFM12 reagiert und legt SEIN Antwortbit an die Leitung
        cbi(RF_PORT, SCK);
        _delay_us(5);       // Dem RFM12 Zeit geben, den Ausgangspegel stabil aufzubauen
        
        // 4. Erst JETZT, wo der Takt wieder LOW ist, lesen wir das stabile Antwortbit ein
        werti <<= 1;
        if (RF_PIN & (1 << SDI))
        {
            werti |= 1;
        }
        _delay_us(5);       // Sicherheitspause vor dem nächsten Bit
    }
    
    sbi(RF_PORT, nSEL); // Modul deaktivieren (HIGH)
#endif
	sbi(RF_PORT, nSEL);  // Slave Select inaktiv (HIGH)
	return werti;
}


void rf12_init(void)
{

#if defined(__AVR_ATtiny2313__) || defined(__AVR_ATtiny2313A__)
    // DO, USCK und nSEL als Ausgang. DI (SDI) bleibt Eingang.
    RF_DDR = (1<<SDO)|(1<<SCK)|(1<<nSEL);
#else
    RF_DDR = (1<<SDI)|(1<<SCK)|(1<<nSEL);
#endif
	RF_PORT=(1<<nSEL);

#ifdef SPI_MODE
	//Aktiviren des SPI - Bus, Clock = Idel LOW
	//SPI Clock teilen durch 128, Enable SPI, SPI in Master Mode
	SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR0)|(1<<SPR1);
	SPSR &= ~(0<<SPI2X);
#endif

	_delay_ms(100);			// wait until POR done

	rf12_trans(0xC0E0);			// AVR CLK: 10MHz
	rf12_trans(0x80D7);			// Enable FIFO
	rf12_trans(0xC2AB);			// Data Filter: internal
	rf12_trans(0xCA81);			// Set FIFO mode
	rf12_trans(0xE000);			// disable wakeuptimer
	rf12_trans(0xC800);			// disable low duty cycle
	rf12_trans(0xC4F7);			// AFC settings: autotuning: -10kHz...+7,5kHz
}

void rf12_setbandwidth(unsigned char bandwidth, unsigned char gain, unsigned char drssi)
{
	rf12_trans(0x9400|((bandwidth&7)<<5)|((gain&3)<<3)|(drssi&7));
}

void rf12_setfreq(unsigned short freq)
{	if (freq<96)				// 430,2400MHz
		freq=96;
	else if (freq>3903)			// 439,7575MHz
		freq=3903;
	rf12_trans(0xA000|freq);
}

void rf12_setbaud(unsigned short baud)
{
	if (baud<663)
		return;
	if (baud<5400)					// Baudrate= 344827,58621/(R+1)/(1+nSEL*7)
		rf12_trans(0xC680|((43104/baud)-1));
	else
		rf12_trans(0xC600|((344828UL/baud)-1));
}

void rf12_setpower(unsigned char power, unsigned char mod)
{
	rf12_trans(0x9800|(power&7)|((mod&15)<<4));
}
/*
void rf12_ready(void)
{
	cbi(RF_PORT, nSEL);
	while (!(RF_PIN&(1<<SDO))); // wait until FIFO ready, Beim Tiny horcht MISO am SDI-Pin (PB5)
}
*/

void rf12_ready(void)
{
    cbi(RF_PORT, nSEL);
    uint16_t timeout = 0;
    
    // Schleife läuft NUR, wenn der Pin aktiv auf LOW (0) gezogen wird.
    // Sobald das Modul den Pin freigibt (Pegel steigt auf 3.3V), 
    // ist die Bedingung (== 0) falsch und die Schleife bricht sofort ab.
    while ((RF_PIN & (1 << SDI)) == 0) 
    {
        timeout++;
        if (timeout > 60000) 
        {
            break; 
        }
    }
}
void rf12_txdata(unsigned char *data)
{	
	RF12_Index = 0;
	rf12_trans(0x8238);			// TX on
	rf12_ready();
		PORTD &= ~(1 << PD5);  // debug: LED2 ausschalten
	rf12_trans(0xB8AA);
	rf12_ready();
	rf12_trans(0xB8AA);
	rf12_ready();
	rf12_trans(0xB8AA);
	rf12_ready();
	rf12_trans(0xB82D);
	rf12_ready();
	rf12_trans(0xB8D4);
	do 
	{
		rf12_ready();
		rf12_trans(0xB800|(data[RF12_Index++]));
	} while ((data[RF12_Index-2] != '\0' || RF12_Index < 2) && RF12_Index < RF12_DataLength+2);
	
	rf12_ready();
	rf12_trans(0x8208);			// TX off
}

void rf12_rxstart(void)
{
    rf12_trans(0x82C8);         // RX on
    rf12_trans(0xCA81);         // set FIFO mode
    rf12_trans(0xCA83);         // enable FIFO
#ifdef RF12_INTERRUPT
    RF12_Index = 0;
    RF12_status.Rx = 1;
    rf12_trans(0x0000);         // Read status / reset IRQ

#if defined(__AVR_ATtiny2313__) || defined(__AVR_ATtiny2313A__)
    sbi(GIMSK, INT0);           // ATtiny nutzt GIMSK statt GICR!
#else
    sbi(GICR, INT0);            // ATmega8 Register
#endif
    sei();
#endif
}



#ifdef RF12_INTERRUPT
void rf12_rxrestart(void)
{
	if(RF12_status.Rx == 0)
	{
		rf12_trans(0x82C8);			// RX on
		rf12_trans(0xCA81);			// set FIFO mode
		rf12_trans(0xCA83);			// enable FIFO
		RF12_Index = 0;
		RF12_status.Rx = 1;
#if defined(__AVR_ATtiny2313__) || defined(__AVR_ATtiny2313A__)
        sbi(GIMSK, INT0);
#else
        sbi(GICR, INT0);
#endif
	}
}
#endif


#ifdef RF12_INTERRUPT
ISR(INT0_vect)
{
	if(RF12_status.Rx)  // Empfangsbereitschaft
	{
		if(RF12_Index < RF12_DataLength+2)  // Puffer nicht voll
            RF12_Data[RF12_Index++] = rf12_trans(0xB000);
		else	// Puffer voll
		{
			rf12_trans(0x8208); 				//RX off
			RF12_status.Rx = 0;	
#if defined(__AVR_ATtiny2313__) || defined(__AVR_ATtiny2313A__)
            GIMSK &= ~(1<<INT0);
#else				
			GICR &= ~(1<<INT0);		//disable int0
#endif
		}
		//auf EOT pruefen. Achtung Pruefsummenbyte koennte 0 sein
		if(RF12_Data[RF12_Index-1] == '\0' && RF12_Index > 2)		//EOT
		{
			rf12_trans(0x8208);					//RX off
			RF12_status.Rx = 0;
#if defined(__AVR_ATtiny2313__) || defined(__AVR_ATtiny2313A__)
            GIMSK &= ~(1<<INT0);
#else
			GICR &= ~(1<<INT0);		//disable int0
#endif
		}
	}
}
#else  // kein Interrupt
void rf12_rxdata(unsigned char *data, unsigned char number)
{	
	unsigned char i;
	rf12_rxstart();
	for (i=0; i<number; i++)
	{
		rf12_ready();
		*data++=rf12_trans(0xB000);
	}
	rf12_trans(0x8208);			// RX off
}
#endif

uint16_t checksum(unsigned char data[])
{
	uint16_t chksum = 0;
	uint8_t i = 0;
	while(data[i] != '\0' && i < RF12_DataLength)
	{
		chksum += data[i++];
	}
	return chksum;
}