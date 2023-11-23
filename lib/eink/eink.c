#include "eink.h"

#include <time.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include <signal.h>     //signal()

#include "./EPD_IT8951.h"
#include "../GUI/GUI_Paint.h"
#include "../GUI/GUI_BMPfile.h"
#include "../Utils/Debug.h"
#include "../Config/DEV_Config.h"

#define IMG_FILE "./pic/img.bmp"

UBYTE *Refresh_Frame_Buf = NULL;

UBYTE *Panel_Frame_Buf = NULL;
UBYTE *Panel_Area_Frame_Buf = NULL;

bool Four_Byte_Align = false;

UWORD VCOM = 2510;

extern IT8951_Dev_Info Dev_Info;
UWORD Panel_Width;
UWORD Panel_Height;
int epd_mode = 0;	//0: no rotate, no mirror
					//1: no rotate, horizontal mirror, for 10.3inch
					//2: no totate, horizontal mirror, for 5.17inch
					//3: no rotate, no mirror, isColor, for 6inch color
					
extern UBYTE isColor;
/******************************************************************************
function: Change direction of display, Called after Paint_NewImage()
parameter:
    mode: display mode
******************************************************************************/
static void Epd_Mode(int mode)
{
	if(mode == 3) {
		Paint_SetRotate(ROTATE_0);
		Paint_SetMirroring(MIRROR_NONE);
		isColor = 1;
	}else if(mode == 2) {
		Paint_SetRotate(ROTATE_0);
		Paint_SetMirroring(MIRROR_HORIZONTAL);
	}else if(mode == 1) {
		Paint_SetRotate(ROTATE_0);
		Paint_SetMirroring(MIRROR_HORIZONTAL);
	}else {
		Paint_SetRotate(ROTATE_0);
		Paint_SetMirroring(MIRROR_NONE);
	}
}

void  Handler(int signo){
    Debug("\r\nHandler:exit\r\n");
    if(Refresh_Frame_Buf != NULL){
        free(Refresh_Frame_Buf);
        Debug("free Refresh_Frame_Buf\r\n");
        Refresh_Frame_Buf = NULL;
    }
    if(Panel_Frame_Buf != NULL){
        free(Panel_Frame_Buf);
        Debug("free Panel_Frame_Buf\r\n");
        Panel_Frame_Buf = NULL;
    }
    if(Panel_Area_Frame_Buf != NULL){
        free(Panel_Area_Frame_Buf);
        Debug("free Panel_Area_Frame_Buf\r\n");
        Panel_Area_Frame_Buf = NULL;
    }
    if(bmp_src_buf != NULL){
        free(bmp_src_buf);
        Debug("free bmp_src_buf\r\n");
        bmp_src_buf = NULL;
    }
    if(bmp_dst_buf != NULL){
        free(bmp_dst_buf);
        Debug("free bmp_dst_buf\r\n");
        bmp_dst_buf = NULL;
    }
	if(Dev_Info.Panel_W != 0){
		Debug("Going to sleep\r\n");
		EPD_IT8951_Sleep();
	}
    DEV_Module_Exit();
    exit(0);
}

bool eInk_Init(IT8951_Dev_Info *Dev_Info) {
    //Exception handling:ctrl + c
    signal(SIGINT, Handler);

    //Init the BCM2835 Device
    if(DEV_Module_Init()!=0){
        return false;
    }

    Debug("VCOM value:%d\r\n", VCOM);
    Debug("Display mode:%d\r\n", epd_mode);
    printf("initing EPD");
    *Dev_Info = EPD_IT8951_Init(VCOM);

    char* LUT_Version = (char*)Dev_Info->LUT_Version;
    if( strcmp(LUT_Version, "M841_TFA2812A2") != 0 ){
        Debug("Wrong LUT Version");
    }
    Debug("LUT: %s", LUT_Version);
    //10.3inch e-Paper HAT(1872,1404)
    A2_Mode = 6;
    Debug("A2 Mode:%d\r\n", A2_Mode);

    UDOUBLE Init_Target_Memory_Addr = Dev_Info->Memory_Addr_L | (Dev_Info->Memory_Addr_H << 16);
	EPD_IT8951_Clear_Refresh(*Dev_Info, Init_Target_Memory_Addr, INIT_Mode);
    return true;
}

UBYTE eInk_BMP(IT8951_Dev_Info *Dev_Info, UBYTE BitsPerPixel){
    UWORD WIDTH;
    //get some important info from Dev_Info structure
    UDOUBLE Init_Target_Memory_Addr = Dev_Info->Memory_Addr_L | (Dev_Info->Memory_Addr_H << 16);

    if(Four_Byte_Align == true){
        WIDTH  = Dev_Info->Panel_W - (Dev_Info->Panel_W % 32);
    }else{
        WIDTH = Dev_Info->Panel_W;
    }
    UWORD HEIGHT = Dev_Info->Panel_H;

    UDOUBLE Imagesize;

    Imagesize = ((WIDTH * BitsPerPixel % 8 == 0)? (WIDTH * BitsPerPixel / 8 ): (WIDTH * BitsPerPixel / 8 + 1)) * HEIGHT;
    if((Refresh_Frame_Buf = (UBYTE *)malloc(Imagesize)) == NULL) {
        Debug("Failed to apply for black memory...\r\n");
        return -1;
    }

    Paint_NewImage(Refresh_Frame_Buf, WIDTH, HEIGHT, 0, BLACK);
    Paint_SelectImage(Refresh_Frame_Buf);
	Epd_Mode(epd_mode);
    Paint_SetBitsPerPixel(BitsPerPixel);
    Paint_Clear(WHITE);

    GUI_ReadBmp(IMG_FILE, 0, 0);
    EPD_IT8951_4bp_Refresh(Refresh_Frame_Buf, 0, 0, WIDTH,  HEIGHT, false, Init_Target_Memory_Addr,false);

    if(Refresh_Frame_Buf != NULL){
        free(Refresh_Frame_Buf);
        Refresh_Frame_Buf = NULL;
    }

    return 0;
}

void eInk_Shutdown() {
    //EPD_IT8951_Standby();
    EPD_IT8951_Sleep();

    //In case RPI is transmitting image in no hold mode, which requires at most 10s
    DEV_Delay_ms(5000);

    DEV_Module_Exit();
}
