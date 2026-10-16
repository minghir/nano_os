#include <stdio.h>
//#include "../../src/nano_libc.h"
//#include "../../src/nano_string.h"
#include "m_argv.h"

#include "doomgeneric.h"

pixel_t* DG_ScreenBuffer = NULL;

void M_FindResponseFile(void);
void D_DoomMain (void);


void doomgeneric_Create(int argc, char **argv)
{
	// save arguments
    myargc = argc;
    myargv = argv;

	M_FindResponseFile();
	DG_ScreenBuffer = malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
	DG_Init();
	printf("Doom version:Goaga dadan");
	D_DoomMain ();

}

