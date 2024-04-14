// =====================================================================================
//  This entire file is redacted as a courtesy to OpCode Games.
// =====================================================================================
#include <nds.h>

u8  SGC_Bank[4]         = {0,0,0,0};
u8  SGC_EEPROM_State    = 0;
u8  SGC_EEPROM_CmdPos   = 0;

void SuperGameCartSetup(int romSize) {}
void SuperGameCartWrite(u16 address, u8 value) {}
u8   SuperGameCartRead(u16 address) {return 0;}
void SuperGameCartWriteEE(void) {}
