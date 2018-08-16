#include <nds.h>

#include <cstdio>
#include <fat.h>
#include <malloc.h>
#include <list>
#include <ctype.h>

#include "auxspi.h"
#include "globals.h"

int wait(bool cancel=false){
	while(1) {
		swiWaitForVBlank();
		scanKeys();
		if(keysDown()&KEY_A) return KEY_A;
		if(cancel && (keysDown()&KEY_B)) return KEY_B;
	}
	return 0;
}

void save () {
	uint8* buffer;
	uint8 size = auxspi_save_size_log_2(slot_1_type);
	int size_blocks = 1 << std::max(0, (int8(size) - 18));
	uint8 type = auxspi_save_type(slot_1_type);
	FILE * pFile;
	sprintf(txt, "saves/%s.sav", gameid);
	pFile = fopen (txt,"w+");
	if(pFile!=NULL){
		iprintf("Creating the savefile\n");
		if (size < 16)
			size_blocks = 1;
		else
			size_blocks = 1 << (size - 16);
		u32 LEN = std::min(1 << size, 1 << 16);
		buffer = (u8 *)malloc(LEN*size_blocks);
		auxspi_read_data(0, buffer, LEN*size_blocks, type, slot_1_type);
		fwrite(buffer, 1, LEN*size_blocks, pFile);
		fclose(pFile);
		free(buffer);
		iprintf("Savefile created\n");
	} else {
		iprintf("Couldn't create savefile\n");
	}
	iprintf("Press A to continue\n");
	wait();
	return;
}
void restore () {
	FILE * pFile;
	sprintf(txt, "saves/%s.sav", gameid);
	pFile = fopen (txt,"rb");
	if(pFile==NULL){
		iprintf("Savefile not found!\n");
	} else {
		uint8* buffer;
		uint8 size = auxspi_save_size_log_2(slot_1_type);
		uint8 type = auxspi_save_type(slot_1_type);
		if (type == 3) {
			iprintf("The savefile in the cartige has to be cleared, press A to\ncontinue, B to cancel\n");
			if(wait(true)==KEY_B)
				return;
			iprintf("Deleting the previous savefile\n");
			auxspi_erase(slot_1_type);
			iprintf("Savefile deleted\n");
		}
		u32 num_blocks = 0, shift = 0;
		switch (type) {
		case 1:
			shift = 4; // 16 bytes
			break;
		case 2:
			shift = 5; // 32 bytes
			break;
		case 3:
			shift = 8; // 256 bytes
			break;
		default:
			return;
		}
		u32 LEN = 1 << shift;
		num_blocks = 1 << (size - shift);
		iprintf("Savefile loaded, press A to\nwrite it in the cartige, B to\ncancel\n");
		if(wait(true)==KEY_B){
			fclose(pFile);
			return;
		}
		buffer = (u8 *)malloc(LEN);
		int step = num_blocks/32;
		for (unsigned int i = 0; i < num_blocks; i++) {
			if (i % step == 0)
				iprintf("#");
			fread(buffer, 1, LEN, pFile);
			auxspi_write_data(i << shift, buffer, LEN, type, slot_1_type);
		}
		fclose(pFile);
		free(buffer);
		iprintf("Savefile successfully written!\n");
	}
	iprintf("Press A to continue\n");
	wait();
	return;
}
PrintConsole upperScreen;
PrintConsole lowerScreen;

void displayInit()
{
	videoSetMode(MODE_0_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	consoleInit(&upperScreen, 3,BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);

	videoSetModeSub(MODE_0_2D);
	vramSetBankC(VRAM_C_SUB_BG);
	consoleInit(&lowerScreen, 3,BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);
}
//---------------------------------------------------------------------------------
int main() {
	displayInit();
	sNDSHeader nds;
	nds.gameTitle[0] = 0;
	char gamename[13];
	consoleSelect(&upperScreen);
	consoleClear();
	iprintf("Savegame manager by edo9300");
	consoleSelect(&lowerScreen);
	consoleClear();
	// If slot is powered off, tell Arm7 slot power on is required.
	if(REG_SCFG_MC == 0x11) { fifoSendValue32(FIFO_USER_02, 1); }
	if(REG_SCFG_MC == 0x10) { fifoSendValue32(FIFO_USER_02, 1); }

	if(REG_SCFG_MC == 0x11) {
		iprintf("No cartridge detected!\nPlease insert a cartridge to continue!\n");
		do { swiWaitForVBlank(); } while (REG_SCFG_MC == 0x11);
		for (int i = 0; i < 20; i++) { swiWaitForVBlank(); }
	}
	
	// Tell Arm7 it's ready for card reset (if card reset is nessecery)
	fifoSendValue32(FIFO_USER_01, 1);
	// Waits for Arm7 to finish card reset (if nessecery)
	fifoWaitValue32(FIFO_USER_03);
	
	if(!fatInitDefault()){
		iprintf("SD init failed");
		while(1) swiWaitForVBlank();
	}
	
	sysSetCardOwner (BUS_OWNER_ARM9);

	// Delay half a second for the DS card to stabilise
	for (int i = 0; i < 30; i++) { swiWaitForVBlank(); }
	
	cardReadHeader((uint8*)&nds);
	slot_1_type = auxspi_has_extra();
	int type = cardEepromGetType();
	int size = cardEepromGetSize();
	iprintf("Type %d\n",type);
	iprintf("Size %d\n",size);
	if(type==999 || size==0){
		iprintf("Cartige not read properly, please restart the program");
		while(1){}
	}
	memcpy(&gameid[0], &nds.gameCode[0], 4);
			gameid[4] = 0x00;
	memcpy(&gamename[0], &nds.gameTitle[0], 12);
			gamename[12] = 0x00;
	iprintf(gameid);
	iprintf("\n");
	iprintf(nds.gameTitle);
	iprintf("\n");
	iprintf("Press A to dump the save from\nyour cartige, press B to restore");
	iprintf("\n");
	while(1) {
		swiWaitForVBlank();
		scanKeys();
		if(keysDown()&KEY_A) {
			save();
			consoleClear();
			iprintf(gameid);
			iprintf("\n");
			iprintf(nds.gameTitle);
			iprintf("\n");
			iprintf("Press A to dump the save from\nyour cartige, press B to restore");
			iprintf("\n");
		}
		else if(keysDown()&KEY_B) {
			restore();
			consoleClear();
			iprintf(gameid);
			iprintf("\n");
			iprintf(nds.gameTitle);
			iprintf("\n");
			iprintf("Press A to dump the save from\nyour cartige, press B to restore");
			iprintf("\n");
		}
	}
	return 0;
}

