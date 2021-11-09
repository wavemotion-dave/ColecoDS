#include <nds/arm7/audio.h>
#include <nds/fifocommon.h>
#include <nds/fifomessages.h>

typedef enum {
  EMUARM7_INIT_SND = 0x123C,
  EMUARM7_STOP_SND = 0x123D,
  EMUARM7_PLAY_SND = 0x123E,
} FifoMesType;

//---------------------------------------------------------------------------------
void soundEmuDataHandler(int bytes, void *user_data) 
{
	int channel = -1;

	FifoMessage msg;

	fifoGetDatamsg(FIFO_USER_01, bytes, (u8*)&msg);

  switch (msg.type) {
    case EMUARM7_PLAY_SND:
      channel = (msg.SoundPlay.format & 0xF0)>>4;
      SCHANNEL_SOURCE(channel) = (u32)msg.SoundPlay.data;
      SCHANNEL_REPEAT_POINT(channel) = msg.SoundPlay.loopPoint;
      SCHANNEL_LENGTH(channel) = msg.SoundPlay.dataSize;
      SCHANNEL_TIMER(channel) = SOUND_FREQ(msg.SoundPlay.freq);
      SCHANNEL_CR(channel) = SCHANNEL_ENABLE | SOUND_VOL(msg.SoundPlay.volume) | SOUND_PAN(msg.SoundPlay.pan) | ((msg.SoundPlay.format & 0xF) << 29) | (msg.SoundPlay.loop ? SOUND_REPEAT : SOUND_ONE_SHOT);
      break;
   
    case EMUARM7_INIT_SND:
      break;

    case EMUARM7_STOP_SND:
      break;
  }
}

//---------------------------------------------------------------------------------
void soundEmuCommandHandler(u32 command, void* userdata) {
	int cmd = (command ) & 0x00F00000;
	int data = command & 0xFFFF;
	int channel = (command >> 16) & 0xF;
	
	switch(cmd) 
  {

	case SOUND_SET_VOLUME:
		SCHANNEL_CR(channel) &= ~0xFF;
		SCHANNEL_CR(channel) |= data;
		break;

	case SOUND_SET_PAN:
		SCHANNEL_CR(channel) &= ~SOUND_PAN(0xFF);
		SCHANNEL_CR(channel) |= SOUND_PAN(data);
		break;

	case SOUND_SET_FREQ:
		SCHANNEL_TIMER(channel) = SOUND_FREQ(data);
		break;

	case SOUND_SET_WAVEDUTY:
		SCHANNEL_CR(channel) &=	 ~(7 << 24);
		SCHANNEL_CR(channel) |=	(data) << 24;
		break;

	case SOUND_KILL:
	case SOUND_PAUSE:
		SCHANNEL_CR(channel) &= ~SCHANNEL_ENABLE;
		break;

	case SOUND_RESUME:
		SCHANNEL_CR(channel) |= SCHANNEL_ENABLE;
		break;

	default: break;
	}
}

//---------------------------------------------------------------------------------
void installSoundEmuFIFO(void) 
{
	fifoSetDatamsgHandler(FIFO_USER_01, soundEmuDataHandler, 0);
	fifoSetValue32Handler(FIFO_USER_01, soundEmuCommandHandler, 0);
}
