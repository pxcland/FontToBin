/*	----------------------------------------------------------------
 *	FontToBin.c
 *	Converts a 2 color bitmap image of an ASCII character set
 *	to a width bit x height*128 line long .bin file for use with
 *	$readmemb in Verilog.
 *	Font must have first 64 characters on the top row, and the last 64
 *	on the bottom row.
 *	No spaces between characters.
 *	See example font image.
 
 *	Usage: Drag the .bmp file of the font onto the executable or pass as
 *	the only command line argument.
 *	Program outputs in "font.bin" in the executable's directory.
 *	The first line is the first scanline of ASCII character 0x00, and the last
 *	line is the last scanline of ASCII character 0x80.
 
 *	Characters are not to be wider than 32 pixels.
 
 *	Copyright 2017 Patrick Cland
 *	www.setsunasoft.com
 *	---------------------------------------------------------------- */

#include <stdio.h>
#include <malloc.h>
#include <stdint.h>

FILE* font, *bin;

void toBinary(uint32_t n, int width)
{
	int i;
	/* Get the MSB of n to the MSB of the data type */
	for(i = width-1; i >= 0; i--)
		fprintf(bin, "%d", (n >> i) & 1);
	fprintf(bin, "\n");
}

void swapEndian32(uint32_t* x)
{
	*x = ((*x >> 24) | ((*x << 8) & 0x00FF0000) | ((*x >> 8) & 0x0000FF00) | (*x << 24));
}

void assembleCharacter(uint32_t* data, int ASCII, int charWidth, int charHeight, int dwordsPerLine, uint32_t* character)
{
	int i;
	int dword, offset, onBoundary;
	uint32_t mask;
	uint32_t tmp1, tmp2;
	/* bit of first upper left most pixel of character */
	int bits = ((ASCII % 64) * charWidth) + (ASCII < 64 ? 0 : charHeight * dwordsPerLine * 32);
	dword = bits / 32;
	/* from MSB */
	offset = bits % 32;
	onBoundary = ((offset + charWidth) > 32 ? 1 : 0);
	
	if(!onBoundary)
	{
		for(i = 0; i < charHeight; i++)	
		{
			mask = ((1 << charWidth) - 1);
			character[i] = (data[dword] >> (32 - charWidth - offset)) & mask;
			bits += (dwordsPerLine * 32);
			dword = bits / 32;
			offset = bits % 32;
		}			
	}
	else
	{
		for(i = 0; i < charHeight; i++)
		{
			mask = (1 << (32 - offset)) - 1;
			tmp1 = data[dword] & mask;
			mask = ((1 << (charWidth - (32 - offset))) - 1) << (32 - (charWidth - (32 - offset)));
			tmp2 = data[dword+1] & mask;
			/* Consider a character split across a word boundary */
			/* XXXX XXXX XXX1 1111  |  111X XXXX XXXX XXXX */
			/* We want 1111 1111, tmp1 has 0000 0000 0001 1111, tmp2 has 1110 0000 0000 0000 */
			/* The above does that */
			/* Shift tmp1 left by 3, shift tmp2 right by 13, then OR them together */
			character[i] = (tmp1 << (charWidth - (32 - offset))) | (tmp2 >> (32 - (charWidth - (32 - offset))));
			
			bits += (dwordsPerLine * 32);
			dword = bits / 32;
			offset = bits % 32;
		}
	}
}

int main(int argc, char* argv[])
{
	int pixelDataOffset;
	int imageWidth, imageHeight;
	int charWidth, charHeight;
	int bytesPerLine;
	int dwordsPerLine;
	int i, j;
	uint32_t* charData = 0;
	uint32_t* character = 0;
	if(!(font = fopen(argv[1], "r")))
	{
		fprintf(stderr, "Error opening source font file: %s.\n", argv[1]);
		return 1;
	}
	if(!(bin = fopen("font.bin", "w")))
	{
		fprintf(stderr, "Error creating destination bin file font.bin.\n");
		fclose(font);
		return 1;
	}
	/* Offset to pixel data is at location 0x0A in bitmap */
	fseek(font, 0x0A, SEEK_SET);
	fread(&pixelDataOffset, 4, 1, font); 
	/* Read width and height data */
	fseek(font, 0x0E + 4, SEEK_SET);
	fread(&imageWidth, 4, 1, font);
	fread(&imageHeight, 4, 1, font);
	/* Assuming there are correctly 64 characters on top, this is a simple division to get character width & height */
	charWidth = imageWidth / 64;
	charHeight = imageHeight / 2;
	/* Since there are 64 characters per line, no matter the width of each character, we are always on a dword boundary for the bitmap */
	bytesPerLine = charWidth * 64 / 8;
	dwordsPerLine = bytesPerLine / 4;
	
	if(!(charData = malloc(sizeof(uint32_t) * dwordsPerLine * imageHeight)))
	{
		fprintf(stderr, "Error allocating memory.\n");
		fclose(font);
		fclose(bin);
		return 1;
	}
	fseek(font, pixelDataOffset, SEEK_SET);
	for(i = imageHeight - 1; i >= 0; i--)
	{
		for(j = 0; j < dwordsPerLine; j++)
		{
			fread(&(charData[(i*dwordsPerLine)+j]), sizeof(uint32_t), 1, font);
			/* Since the machine is little endian, reading a sequence of 0x01020304 is stored as 0x04030201, we need to fix it. */
			swapEndian32(&(charData[(i*dwordsPerLine)+j]));
		}
	}
	
	if(!(character = malloc(sizeof(uint32_t) * charHeight)))
	{
		fprintf(stderr, "Error allocating memory.\n");
		fclose(font);
		fclose(bin);
		free(charData);
		return 1;
	}
	
	for(i = 0; i < 128; i++)
	{
		assembleCharacter(charData, i, charWidth, charHeight, dwordsPerLine, character);
		for(j = 0; j < charHeight; j++)
			toBinary(character[j], charWidth);
	}
	
	free(charData);
	free(character);
	fclose(font);
	fclose(bin);
	return 0;
}