#include "../lib/eink/eink.h"

#include <math.h>

#include <stdlib.h>     //exit()

int main(int argc, char *argv[])
{
    IT8951_Dev_Info Dev_Info;
    if (!eInk_Init(&Dev_Info)) {
        return -1;
    };

    //Show a bmp file
    //1bp use A2 mode by default, before used it, refresh the screen with WHITE
    eInk_BMP(&Dev_Info, BitsPerPixel_4);

    eInk_Shutdown();
    
    return 0;
}
