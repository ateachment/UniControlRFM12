/*
 * main.c
 * Function: transmit string with variable length (up to 32 bytes) via RFM12 module
 * with pre-appended checksum
 * Created: 2026-06-11
 * Author: Eick
 */ 


#include <avr/io.h>
#include <string.h>
#include <util/delay.h>
#include "global.h"
#include "rf12.h"



#define LED1    6
#define LED2    5

void send(void);

int main(void)
{
	DDRD |= (1 << LED1) | (1 << LED2);	// Richtungsregister LEDs

	
	
	
	// für Debugging LED2 einschalten
	//PORTD |= (1 << LED2);
	
	//PORTD &= ~(1 << PD5);  // debug: LED2 ausschalten



	rf12_init();						// ein paar Register setzen (z.B. CLK auf 10MHz)
	rf12_setfreq(RF12FREQ(433.67));		// Sende/Empfangsfrequenz auf 433,92MHz einstellen
	rf12_setbandwidth(4, 1, 4);			// 200kHz Bandbreite, -6dB Verst�rkung, DRSSI threshold: -79dBm
	rf12_setbaud(19200);				// 19200 baud
	rf12_setpower(0, 6);				// 1mW Ausgangangsleistung, 120kHz Frequenzshift

	

	while(1)
	{
		send();
		
		if(RF12_Index >= 24)  //Test ohne Pruefsumme 22)
			PORTD |= (1 << LED1);
		else
			PORTD |= (1 << LED2);
		_delay_ms(10);
		PORTD &= ~(1 << LED1) & ~(1 << LED2);
		_delay_ms(2000);
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
	//rf12_txdata("01234567890123456789");  //Test: ohne Pruefsumme
}


