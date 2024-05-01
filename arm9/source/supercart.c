// =====================================================================================
//  This entire file is redacted as a courtesy to OpCode Games.
// =====================================================================================
#include <nds.h>

u8 SGC_Bank[4]         = {0,0,0,0};
u8 SGC_SST_State;
u8 SGC_SST_CmdPos;

void SuperGameCartSetup(int romSize) {}
void SuperGameCartWrite(u16 address, u8 value) {}
u8   SuperGameCartRead(u16 address) {return 0x00;}
void SuperGameCartSaveFlash(void) {}
