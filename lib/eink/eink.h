#ifndef __BMP__
#define __BMP__

#include "./EPD_IT8951.h"
#include "../Config/DEV_Config.h"

// 1 bit per pixel, which is 2 grayscale
#define BitsPerPixel_1 1
// 2 bit per pixel, which is 4 grayscale 
#define BitsPerPixel_2 2
// 4 bit per pixel, which is 16 grayscale
#define BitsPerPixel_4 4
// 8 bit per pixel, which is 256 grayscale, but will automatically reduce by hardware to 4bpp, which is 16 grayscale
#define BitsPerPixel_8 8

//For all refresh fram buf except touch panel
extern UBYTE *Refresh_Frame_Buf;

//Only for touch panel
extern UBYTE *Panel_Frame_Buf;
extern UBYTE *Panel_Area_Frame_Buf;

extern bool Four_Byte_Align;

bool eInk_Init(IT8951_Dev_Info *Dev_Info);

UBYTE eInk_BMP(IT8951_Dev_Info *Dev_Info, UBYTE BitsPerPixel);

void eInk_Shutdown();

#endif
