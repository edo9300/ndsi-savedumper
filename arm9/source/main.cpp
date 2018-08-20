/*
 *  This file is part of ndi-savedumper.
 *  Copyright (C) 2018 Edo9300
 *
 *  ndi-savedumper is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
#include <nds.h>
#include<dirent.h>

#include <cstdio>
#include <fat.h>
#include <malloc.h>
#include <list>
#include <ctype.h>
#include <string>
#include <vector>

#include "auxspi.h"
#include "globals.h"

#define DISPLAY_COLUMNS 32

int wait(bool cancel=false){
	while(1) {
		swiWaitForVBlank();
		scanKeys();
		if(keysDown()&KEY_A) return KEY_A;
		if(cancel && (keysDown()&KEY_B)) return KEY_B;
	}
	return 0;
}

void NameUpdate(char gameid[]){
	while(1) {
		swiWaitForVBlank();
		scanKeys();
		if(keysDown()&KEY_A) {sprintf(txt, "%s.sav", gameid); return;}
		if(keysDown()&KEY_X) {sprintf(txt, "save.sav"); return;}
	}
}

PrintConsole upperScreen;
PrintConsole lowerScreen;

void WriteMessage(std::string text, bool clear = false, bool topscreen = false){
	consoleSelect(topscreen ? &upperScreen : &lowerScreen);
	if (clear)
		consoleClear();
	if(text.size()<=DISPLAY_COLUMNS){
		iprintf(text.c_str());
		if(text.size()!=DISPLAY_COLUMNS)
			iprintf("\n");
		return;
	}
	std::vector<std::string> words;
	std::string temp;
	for (int i = 0; i < (int)text.size(); i++){
		if(text[i] == L' '){
			words.push_back(temp);
			temp.clear();
		} else
			temp+=text[i];
	}
	if(temp.size())
		words.push_back(temp);
	std::vector<std::string> rows;
	int column = 0;
	for (auto word : words){
		int chkval=column+(int)word.size();
		if(column)
			chkval++;
		if(chkval<=DISPLAY_COLUMNS){
			if(column){
				iprintf(" ");
				column++;
			}
			iprintf(word.c_str());
			column+=(int)word.size();
		} else {
			if(column!=DISPLAY_COLUMNS)
				iprintf("\n");
			column=(int)word.size();
			iprintf(word.c_str());
		}
	}
	if(column!=DISPLAY_COLUMNS)
		iprintf("\n");
}

void save (auxspi_extra card_type, char gameid[]) {
	sprintf(txt, "Press A to save the savefile as \"%s.sav\".", gameid);
	WriteMessage(txt,true);
	WriteMessage("Press X to save the savefile as \"save.sav\" (use this if garbage text is displayed)");
	NameUpdate(gameid);
	FILE * pFile;
	pFile = fopen (txt,"w+");
	if(pFile!=NULL){
		uint8* buffer;
		WriteMessage("Creating the savefile");
		if(card_type){
			int size = auxspi_save_size_log_2(card_type);
			int size_blocks = 1 << std::max(0, (int8(size) - 18));
			int type = auxspi_save_type(card_type);
			if (size < 16)
				size_blocks = 1;
			else
				size_blocks = 1 << (size - 16);
			u32 LEN = std::min(1 << size, 1 << 16);
			buffer = (u8 *)malloc(LEN*size_blocks);
			auxspi_read_data(0, buffer, LEN*size_blocks, type, card_type);
			fwrite(buffer, 1, LEN*size_blocks, pFile);
		} else {
			int type = cardEepromGetType();
			int size = cardEepromGetSize();
			buffer = (u8 *)malloc(size);
			cardReadEeprom(0, buffer, size, type);
			fwrite(buffer, 1, size, pFile);
		}
		fclose(pFile);
		free(buffer);
		WriteMessage("Savefile created");
	} else {
		WriteMessage("Couldn't create savefile");
	}
	WriteMessage("Press A to continue");
	wait();
	return;
}
void restore (auxspi_extra card_type, char gameid[]) {
	sprintf(txt, "Press A to load %s.sav.", gameid);
	WriteMessage(txt,true);
	WriteMessage("Press X to load \"save.sav\" (use this if garbage text is displayed)");
	NameUpdate(gameid);
	FILE * pFile;
	pFile = fopen (txt,"rb");
	if(pFile==NULL){
		WriteMessage("Savefile not found!");
	} else {
		uint8* buffer;
		if(card_type){
			uint8 size = auxspi_save_size_log_2(card_type);
			int type = auxspi_save_type(card_type);
			if (type == 3) {
				WriteMessage("The savefile in the cartdige has to be cleared, press A to continue, B to cancel",true);
				if(wait(true)==KEY_B)
					return;
				WriteMessage("Deleting the previous savefile");
				auxspi_erase(card_type);
				WriteMessage("Savefile deleted");
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
			WriteMessage("Savefile loaded, press A to write it in the cartdige, B to cancel");
			if(wait(true)==KEY_B){
				fclose(pFile);
				return;
			}
			buffer = (uint8 *)malloc(LEN);
			int step = num_blocks/32;
			for (unsigned int i = 0; i < num_blocks; i++) {
				if (i % step == 0)
					iprintf("#");
				fread(buffer, 1, LEN, pFile);
				auxspi_write_data(i << shift, buffer, LEN, type, card_type);
			}
		} else {
			int type = cardEepromGetType();
			int size = cardEepromGetSize();
			if (type == 3){
				WriteMessage("The savefile in the cartdige has to be cleared, press A to continue, B to cancel",true);
				if(wait(true)==KEY_B)
					return;
				WriteMessage("Deleting the previous savefile");
				cardEepromChipErase();
				WriteMessage("Savefile deleted");				
			}
			WriteMessage("Savefile loaded, press A to write it in the cartdige, B to cancel");
			if(wait(true)==KEY_B){
				fclose(pFile);
				return;
			}
			int blocks=size/32;
			int written = 0;
			buffer = (uint8 *)malloc(blocks);
			for (unsigned int i = 0; i < 32; i++) {
				iprintf("#");
				fread(buffer, 1, blocks, pFile);
				cardWriteEeprom(written, buffer, blocks, type);
				written+=blocks;
			}
		}
		fclose(pFile);
		free(buffer);
		WriteMessage("Savefile successfully written!");
	}
	WriteMessage("Press A to continue");
	wait();
	return;
}

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
		return false;
	}
	memcpy(gameid, nds->gameCode, 4);
	gameid[4] = 0x00;
	memcpy(gamename, nds->gameTitle, 12);
	gamename[12] = 0x00;
	return true;
}

void PrintMenu(char gameid[],char gamename[]){
	WriteMessage(gameid,true);
	WriteMessage(gamename);
	WriteMessage("Press A to dump the save from your cartdige, press B to restore it.");
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
	WriteMessage("Savegame manager by edo9300",true,true);
	
	if(!fatInitDefault()){
		WriteMessage("SD init failed");
		while(1) swiWaitForVBlank();
	}
	
	mkdir("sd:/saves", 0777);
	chdir("sd:/saves");
	
	sysSetCardOwner (BUS_OWNER_ARM9);
	if (REG_SCFG_MC == 0x11) {
		WriteMessage("No cartdige detected!");
		WriteMessage("Please insert a cartdige to continue!");
		WaitCard();
	}
	if(REG_SCFG_MC == 0x10) { 
		disableSlot1();
		for (int i = 0; i < 25; i++) { swiWaitForVBlank(); }
		enableSlot1();
	}
	
	while(!UpdateCardInfo(&nds,&gameid[0],&gamename[0], &card_type)) {
		WriteMessage("Cartdige not read properly!",true);
		WriteMessage("Please reinsert it");
		// Wait until the card is removed, then call the function
		do { swiWaitForVBlank(); } while (REG_SCFG_MC != 0x11);
		WaitCard();
	}
	PrintMenu(gameid, gamename);
	while(1) {
		swiWaitForVBlank();
		if(REG_SCFG_MC == 0x11){
			WriteMessage("The cartdige was removed!",true);
			WriteMessage("Please insert a cartdige to continue!");
			WaitCard();
			while(!UpdateCardInfo(&nds,&gameid[0],&gamename[0], &card_type)) {
				WriteMessage("Cartdige not read properly!",true);
				WriteMessage("Please reinsert it");
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
