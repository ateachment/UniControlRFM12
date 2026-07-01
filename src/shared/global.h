// global.h


#ifndef cbi  
#define cbi(sfr, bit)     (_SFR_BYTE(sfr) &= ~_BV(bit))   // Clear bit in register
#endif
#ifndef sbi
#define sbi(sfr, bit)     (_SFR_BYTE(sfr) |= _BV(bit))    // Set bit in register
#endif




