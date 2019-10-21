#include "graphics.h"
#include <stdio.h>

#include "font_nftr.h"

std::vector<u8> fontTiles;
std::vector<u8> fontWidths;
std::vector<u16> fontMap;
u16 tileSize, tileWidth, tileHeight;

void initGraphics(void) {
	// Initialize video mode
	videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);

	// initialize all the VRAM banks
	vramSetBankC(VRAM_C_SUB_BG);

	// Init for background
	bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);

	u16 palette[] = {0, 0xFBDE, 0xBDEF, // Light
						0xD294, 0xA529, // Darker
						0xCA5F & 0xFBDE, 0xCA5F & 0xBDEF, // Light Red
						0xCA5F & 0xD294, 0xCA5F & 0xA529}; // Darker Red
	memcpy(BG_PALETTE_SUB, &palette, sizeof(palette));
}

void loadFont(void) {
	// Load font info
	u32 chunkSize = *(u32*)(font_nftr+0x30);
	tileWidth = *(font_nftr+0x34);
	tileHeight = *(font_nftr+0x35);
	tileSize = *(u16*)(font_nftr+0x36);

	// Load character glyphs
	int tileAmount = ((chunkSize-0x10)/tileSize);
	fontTiles = std::vector<u8>(tileSize*tileAmount);
	memcpy(fontTiles.data(), font_nftr+0x3C, tileSize*tileAmount);

	// Fix top rows
	for(int i=0;i<tileAmount;i++) {
		fontTiles[i*tileSize] = 0;
		fontTiles[i*tileSize+1] = 0;
		fontTiles[i*tileSize+2] = 0;
	}

	// Load character widths
	u32 locHDWC = *(u32*)(font_nftr+0x24);
	chunkSize = *(u32*)(font_nftr+locHDWC-4);
	fontWidths = std::vector<u8>(3*tileAmount);
	memcpy(fontWidths.data(), font_nftr+locHDWC+8, 3*tileAmount);

	// Load character maps
	fontMap = std::vector<u16>(tileAmount);
	u32 locPAMC = *(u32*)(font_nftr+0x28);

	while(locPAMC < font_nftr_size) {
		const u8* font = font_nftr+locPAMC;

		u16 firstChar = *(u16*)font;
		font += 2;
		u16 lastChar = *(u16*)font;
		font += 2;
		u32 mapType = *(u32*)font;
		font += 4;
		locPAMC = *(u32*)font;
		font += 4;

		switch(mapType) {
			case 0: {
				u16 firstTile = *(u16*)font;
				for(unsigned i=firstChar;i<=lastChar;i++) {
					fontMap[firstTile+(i-firstChar)] = i;
				}
				break;
			} case 1: {
				for(int i=firstChar;i<=lastChar;i++) {
					u16 tile = *(u16*)font;
					font += 2;
					fontMap[tile] = i;
				}
				break;
			} case 2: {
				u16 groupAmount = *(u16*)font;
				font += 2;
				for(int i=0;i<groupAmount;i++) {
					u16 charNo = *(u16*)font;
					font += 2;
					u16 tileNo = *(u16*)font;
					font += 2;
					fontMap[tileNo] = charNo;
				}
				break;
			}
		}
	}
}

void drawImage(int x, int y, int w, int h, std::vector<u8> &imageBuffer, bool top) {
	u8* dst = (u8*)(top ? BG_GFX : BG_GFX_SUB);
	for(int i=0;i<h;i++) {
		for(int j=0;j<w;j++) {
			if(imageBuffer[(i*w)+j] != 0) { // Do not render palette 0
				dst[(y+i)*256+j+x] = imageBuffer[(i*w)+j];
			}
		}
	}
}

void drawImageScaled(int x, int y, int w, int h, double scaleX, double scaleY, std::vector<u8> &imageBuffer, bool top) {
	if(scaleX == 1 && scaleY == 1)	drawImage(x, y, w, h, imageBuffer, top);
	else {
		u8* dst = (u8*)(top ? BG_GFX : BG_GFX_SUB);
		for(int i=0;i<(h*scaleY);i++) {
			for(int j=0;j<(w*scaleX);j++) {
				if(imageBuffer[(((int)(i/scaleY))*w)+(j/scaleX)]>>15 != 0) { // Do not render transparent pixel
					dst[(y+i)*256+x+j] = imageBuffer[(((int)(i/scaleY))*w)+(j/scaleX)];
				}
			}
		}
	}
}

void drawRectangle(int x, int y, int w, int h, u8 color, bool top) {
	u8* dst = (u8*)(top ? BG_GFX : BG_GFX_SUB);
	for(int i=0;i<h;i++) {
		for(int j=0;j<w;j++) {
			dst[(y+i)*256+j+x] = color;
		}
	}
}

std::u16string UTF8toUTF16(const std::string &src) {
	std::u16string ret;
	for(size_t i = 0; i < src.size(); i++) {
		u16 codepoint = 0xFFFD;
		int iMod = 0;
		if(src[i] & 0x80 && src[i] & 0x40 && src[i] & 0x20 && !(src[i] & 0x10) && i + 2 < src.size()) {
			codepoint = src[i] & 0x0F;
			codepoint = codepoint << 6 | (src[i + 1] & 0x3F);
			codepoint = codepoint << 6 | (src[i + 2] & 0x3F);
			iMod = 2;
		} else if(src[i] & 0x80 && src[i] & 0x40 && !(src[i] & 0x20) && i + 1 < src.size()) {
			codepoint = src[i] & 0x1F;
			codepoint = codepoint << 6 | (src[i + 1] & 0x3F);
			iMod = 1;
		} else if(!(src[i] & 0x80)) {
			codepoint = src[i];
		}

		ret.push_back((char16_t)codepoint);
		i += iMod;
	}
	return ret;
}

int getCharIndex(char16_t c) {
	int spriteIndex = 0;
	int left = 0;
	int mid = 0;
	int right = fontMap.size();

	while(left <= right) {
		mid = left + ((right - left) / 2);
		if(fontMap[mid] == c) {
			spriteIndex = mid;
			break;
		}

		if(fontMap[mid] < c) {
			left = mid + 1;
		} else {
			right = mid - 1;
		}
	}
	return spriteIndex;
}

void printTextCenteredMaxW(std::string text, double w, double scaleY, int palette, int xOffset, int yPos, bool top, bool invert) { printText(UTF8toUTF16(text), std::min(1.0, w/getTextWidth(text)), scaleY, palette, ((256-getTextWidthMaxW(text, w))/2)+xOffset, yPos, top, invert); }
void printTextCenteredMaxW(std::u16string text, double w, double scaleY, int palette, int xOffset, int yPos, bool top, bool invert) { printText(text, std::min(1.0, w/getTextWidth(text)), scaleY, palette, ((256-getTextWidthMaxW(text, w))/2)+xOffset, yPos, top, invert); }
void printTextCentered(std::string text, double scaleX, double scaleY, int palette, int xOffset, int yPos, bool top, bool invert) { printText(UTF8toUTF16(text), scaleX, scaleY, palette, ((256-getTextWidth(text))/2)+xOffset, yPos, top, invert); }
void printTextCentered(std::u16string text, double scaleX, double scaleY, int palette, int xOffset, int yPos, bool top, bool invert) { printText(text, scaleX, scaleY, palette, ((256-getTextWidth(text))/2)+xOffset, yPos, top, invert); }
void printTextMaxW(std::string text, double w, double scaleY, int palette, int xPos, int yPos, bool top, bool invert) { printText(UTF8toUTF16(text), std::min(1.0, w/getTextWidth(text)), scaleY, palette, xPos, yPos, top, invert); }
void printTextMaxW(std::u16string text, double w,  double scaleY, int palette, int xPos, int yPos, bool top, bool invert) { printText(text, std::min(1.0, w/getTextWidth(text)), scaleY, palette, xPos, yPos, top, invert); }
void printText(std::string text, double scaleX, double scaleY, int palette, int xPos, int yPos, bool top, bool invert) { printText(UTF8toUTF16(text), scaleX, scaleY, palette, xPos, yPos, top, invert); }
void printText(std::u16string text, double scaleX, double scaleY, int palette, int xPos, int yPos, bool top, bool invert) {
	int x=xPos;
	for(unsigned c=0;c<text.size();c++) {
		if(text[c] == 0x00BB) { // » makes a new line currently, may change this
			x = xPos;
			yPos += tileHeight;
			continue;
		}

		int t = getCharIndex(text[c]);
		std::vector<u8> image;
		for(int i=0;i<tileSize;i++) {
			image.push_back(fontTiles[i+(t*tileSize)]>>6 & 3);
			image.push_back(fontTiles[i+(t*tileSize)]>>4 & 3);
			image.push_back(fontTiles[i+(t*tileSize)]>>2 & 3);
			image.push_back(fontTiles[i+(t*tileSize)]    & 3);
		}

		if(palette) {
			for(unsigned int i=0;i<image.size();i++) {
				if(image[i] != 0) {
					if(palette) {
						image[i] += palette*2;
					}
				}
			}
		}

		x += fontWidths[t*3];
		if(x > 256) {
			x = xPos+fontWidths[t*3];
			yPos += tileHeight;
		}
		drawImageScaled(x, yPos, tileWidth, tileHeight, scaleX, scaleY, image, top);
		x += fontWidths[(t*3)+1]*scaleX;
	}
}

int getTextWidthMaxW(std::string text, int w) { return std::min(w, getTextWidth(UTF8toUTF16(text))); }
int getTextWidthMaxW(std::u16string text, int w) { return std::min(w, getTextWidth(text)); }
int getTextWidth(std::string text) { return getTextWidth(UTF8toUTF16(text)); }
int getTextWidth(std::u16string text) {
	int textWidth = 0;
	for(unsigned c=0;c<text.size();c++) {
		textWidth += fontWidths[(getCharIndex(text[c])*3)+2];
	}
	return textWidth;
}
