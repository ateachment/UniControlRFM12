/*
 * main.c
 * Funktion: Interrupt-gesteuerter Empfang von String mit variabler Laenge
 * vorangestellte 2 Byte Pruefsumme
 * Created: 11.05.2015
 * Author: Eick
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>

#include "global.h"
#include "rf12.h"


#define LED1    6
#define LED2    5

int main(void)
{
	rf12_init();						// ein paar Register setzen (z.B. CLK auf 10MHz)
	rf12_setfreq(RF12FREQ(433.67));		// Sende/Empfangsfrequenz auf 433,67MHz einstellen
	rf12_setbandwidth(4, 1, 4);			// 200kHz Bandbreite, -6dB Verst�rkung, DRSSI threshold: -79dBm
	rf12_setbaud(19200);				// 19200 baud
	rf12_setpower(0, 6);				// 1mW Ausgangangsleistung, 120kHz Frequenzshift

	DDRD |= (1 << LED1)|(1 << LED2);	// Richtungsregister LEDs

	rf12_rxstart();
	while(1)
	{
		PORTD &= ~(1 << LED1) & ~(1 << LED2);   // LEDs aus
		
		
		
		if(RF12_status.Rx == 0)
		{
			//if(RF12_Data[2] == '0' && RF12_Data[12] == '0' && RF12_Data[22] == '\0')

			//if(checksum(RF12_Data+2) == 0x041A) 	//OK -> LED1 an		
			//if(RF12_Data[0] == '0' && RF12_Data[10] == '0' && RF12_Data[20] == '\0')   //Test: ohne Pr�fsumme
			
			uint16_t sum;
			memcpy( &sum, RF12_Data, 2);
				
			//if( RF12_Data[0] == 0x1A)  //ok
			//if( RF12_Data[1] == 0x04)  //ok
			if(sum == checksum(RF12_Data+2)) 	//OK -> LED1 an			
				PORTD |= (1 << LED1);			
			else
				PORTD |= (1 << LED2);			//Fail -> LED2 an
			_delay_ms(40);
			rf12_rxrestart();
		}
	}
}


