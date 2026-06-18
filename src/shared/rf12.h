/*##############################################################################
 * @file        rf12.h 
 * @brief       Header for RFM12 driver
 * @author      Benedikt K., Juergen Eckert, Ulrich Radig, Wolfhard Eick
 * @date        2026
##############################################################################*/

#ifndef RF12_H_
#define RF12_H_

#include <stdint.h>

#define RF12_DataLength 32

// Global variables available to both transmitter and receiver
extern volatile unsigned char RF12_Index;
extern unsigned char RF12_Data[RF12_DataLength+2];

// Struct configuration exclusively for interrupt-driven receiver mode
#ifdef RF12_INTERRUPT
struct RF12_stati
{
    unsigned char Rx:1; 
    unsigned char New:1;
};
extern volatile struct RF12_stati RF12_status;
#endif

// Functions
extern unsigned short rf12_trans(unsigned short wert);
extern void rf12_init(void);
extern void rf12_setfreq(unsigned short freq);
extern void rf12_setbaud(unsigned short baud);
extern void rf12_setpower(unsigned char power, unsigned char mod);
extern void rf12_setbandwidth(unsigned char bandwidth, unsigned char gain, unsigned char drssi);

#ifdef RF12_INTERRUPT
extern void rf12_rxstart(void);
extern void rf12_rxrestart(void);
extern unsigned char* rf12_rxend(void);
#else 
extern void rf12_rxdata(unsigned char *data, unsigned char number);
#endif

extern void rf12_txdata(unsigned char *data);
extern void rf12_ready(void);

// Calculation macro
#define RF12FREQ(freq)  ((freq-430.0)/0.0025)

extern uint16_t checksum(unsigned char data[]);

#endif /* RF12_H_ */