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

void save (auxspi_extra card_type, char gameid[]) {
	consoleClear();
	uint8* buffer;
	uint8 size = auxspi_save_size_log_2(card_type);
	int size_blocks = 1 << std::max(0, (int8(size) - 18));
	uint8 type = auxspi_save_type(card_type);
	FILE * pFile;
	iprintf("Press A to save the savefile as \"%s.sav\".\nPress X to save the savefile as\n\"save.sav\" (use this if garbage\ntext is displayed)\n", gameid);
	while(1) {
		swiWaitForVBlank();
		scanKeys();
		if(keysDown()&KEY_A) {sprintf(txt, "saves/%s.sav", gameid); break;}
		if(keysDown()&KEY_X) {sprintf(txt, "saves/save.sav"); break;}
	}
	pFile = fopen (txt,"w+");
	if(pFile!=NULL){
		iprintf("Creating the savefile\n");
		if (size < 16)
			size_blocks = 1;
		else
			size_blocks = 1 << (size - 16);
		u32 LEN = std::min(1 << size, 1 << 16);
		buffer = (u8 *)malloc(LEN*size_blocks);
		auxspi_read_data(0, buffer, LEN*size_blocks, type, card_type);
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
void restore (auxspi_extra card_type, char gameid[]) {
	consoleClear();
	FILE * pFile;
	iprintf("Press A to load %s.sav.\nPress X to load \"save.sav\" (use this if garbage text is\ndisplayed)\n", gameid);
	while(1) {
		swiWaitForVBlank();
		scanKeys();
		if(keysDown()&KEY_A) {sprintf(txt, "saves/%s.sav", gameid); break;}
		if(keysDown()&KEY_X) {sprintf(txt, "saves/save.sav"); break;}
	}
	pFile = fopen (txt,"rb");
	if(pFile==NULL){
		iprintf("Savefile not found!\n");
	} else {
		uint8* buffer;
		uint8 size = auxspi_save_size_log_2(card_type);
		uint8 type = auxspi_save_type(card_type);
		if (type == 3) {
			iprintf("The savefile in the cartdige has to be cleared, press A to\ncontinue, B to cancel\n");
			if(wait(true)==KEY_B)
				return;
			iprintf("Deleting the previous savefile\n");
			auxspi_erase(card_type);
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
		iprintf("Savefile loaded, press A to\nwrite it in the cartdige, B to\ncancel\n");
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
			auxspi_write_data(i << shift, buffer, LEN, type, card_type);
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

void WaitCard(){
	if (REG_SCFG_MC == 0x11) {
		do { swiWaitForVBlank(); }
		while (REG_SCFG_MC == 0x11);
		disableSlot1();
		for (int i = 0; i < 25; i++) { swiWaitForVBlank(); }
		enableSlot1();
	}
}

bool UpdateCardInfo(sNDSHeader* nds,char* gameid,char* gamename, auxspi_extra* card_type){
	cardReadHeader((uint8*)nds);
	*card_type = auxspi_has_extra();
	int type = cardEepromGetType();
	int size = cardEepromGetSize();
	if(type==999 || size < 1){
		iprintf("Type %d\n",type);
		iprintf("Size %d\n",size);
		return false;
	}
	memcpy(gameid, nds->gameCode, 4);
	gameid[4] = 0x00;
	memcpy(gamename, nds->gameTitle, 12);
	gamename[12] = 0x00;
	return true;
}

void PrintMenu(char gameid[],char gamename[]){
	consoleClear();
	iprintf(gameid);
	iprintf("\n");
	iprintf(gamename);
	iprintf("\n");
	iprintf("Press A to dump the save from\nyour cartdige, press B to restore");
	iprintf("\n");
}

//---------------------------------------------------------------------------------
int main() {
	displayInit();
	auxspi_extra card_type = AUXSPI_FLASH_CARD;
	sNDSHeader nds;
	nds.gameCode[0] = 0;
	nds.gameTitle[0] = 0;
	char gamename[13];
	char gameid[5];
	consoleSelect(&upperScreen);
	consoleClear();
	iprintf("Savegame manager by edo9300");
	consoleSelect(&lowerScreen);
	consoleClear();
	sysSetCardOwner (BUS_OWNER_ARM9);
	if (REG_SCFG_MC == 0x11) {
		iprintf("No cartdige detected!\nPlease insert a cartdige to\ncontinue!\n");
		WaitCard();
	}
	if(REG_SCFG_MC == 0x10) { 
		disableSlot1();
		for (int i = 0; i < 25; i++) { swiWaitForVBlank(); }
		enableSlot1();
	}
	
	if(!fatInitDefault()){
		iprintf("SD init failed");
		while(1) swiWaitForVBlank();
	}
	
	while(!UpdateCardInfo(&nds,&gameid[0],&gamename[0], &card_type)) {
		consoleClear();
		iprintf("Cartdige not read properly!\nPlease reinsert it");
		// Wait until the card is removed, then call the function
		do { swiWaitForVBlank(); } while (REG_SCFG_MC != 0x11);
		WaitCard();
	}
	PrintMenu(gameid, gamename);
	while(1) {
		swiWaitForVBlank();
		if(REG_SCFG_MC == 0x11){
			consoleClear();
			iprintf("The cartdige was removed!\nPlease insert a cartdige to\ncontinue!\n");
			WaitCard();
			while(!UpdateCardInfo(&nds,&gameid[0],&gamename[0], &card_type)) {
				consoleClear();
				iprintf("Cartdige not read properly!\nPlease reinsert it");
				do { swiWaitForVBlank(); } while (REG_SCFG_MC != 0x11);
				WaitCard();
			}
			PrintMenu(gameid, gamename);
		}
		scanKeys();
		if(keysDown()&KEY_A) {
			save(card_type, gameid);
			PrintMenu(gameid, gamename);
		}
		else if(keysDown()&KEY_B) {
			restore(card_type, gameid);
			PrintMenu(gameid, gamename);
		}
	}
	return 0;
}

