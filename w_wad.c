//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Handles WAD file header, directory, lump I/O.
//

#include <pspkernel.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"

//#include "config.h"
#include "d_iwad.h"
#include "i_swap.h"
#include "i_system.h"
#include "i_video.h"
#include "m_misc.h"
#include "z_zone.h"

#include "w_wad.h"

#include "doomfeatures.h"

#define printf pspDebugScreenPrintf

#ifdef SHAREWARE
extern int filesize_strife0_wad;
extern int strife0_wad_exists;
#endif

extern int filesize_strife1_wad;
extern int strife1_wad_exists;

extern int filesize_voices_wad;
extern int voices_wad_exists;

extern int disable_voices;

extern boolean devparm;
extern boolean showintro;

extern boolean STRIFE_1_0_REGISTERED;
extern boolean STRIFE_1_X_REGISTERED;

typedef struct
{
    // Should be "IWAD" or "PWAD".
    char		identification[4];		
    int			numlumps;
    int			infotableofs;
} PACKEDATTR wadinfo_t;


typedef struct
{
    int			filepos;
    int			size;
    char		name[8];
} PACKEDATTR filelump_t;

//
// GLOBALS
//

// Location of each lump on disk.

lumpinfo_t *lumpinfo;		
unsigned int numlumps = 0;

// Hash table for fast lookups

static lumpinfo_t **lumphash;

// Hash function used for lump names.

unsigned int W_LumpNameHash(const char *s)
{
    // This is the djb2 string hash function, modded to work on strings
    // that have a maximum length of 8.

    unsigned int result = 5381;
    unsigned int i;

    for (i=0; i < 8 && s[i] != '\0'; ++i)
    {
        result = ((result << 5) ^ result ) ^ toupper(s[i]);
    }

    return result;
}

//
// LUMP BASED ROUTINES.
//

//
// W_AddFile
// All files are optional, but at least one file must be
//  found (PWAD, if all required lumps are present).
// Files with a .wad extension are wadlink files
//  with multiple lumps.
// Other files are single lumps with the base filename
//  for the lump name.

wad_file_t *W_AddFile (char *filename)
{
    wadinfo_t header;
    lumpinfo_t *lump_p;
    unsigned int i;
    wad_file_t *wad_file;
    int length;
    int startlump;
    filelump_t *fileinfo;
    filelump_t *filerover;
    
    // open the file and add to directory

    wad_file = W_OpenFile(filename);
		
    if (wad_file == NULL)
    {
	printf (" couldn't open %s\n", filename);
	return NULL;
    }

    startlump = numlumps;
	
    if (strcasecmp(filename+strlen(filename)-3 , "wad" ) )
    {
	// single lump file

        // fraggle: Swap the filepos and size here.  The WAD directory
        // parsing code expects a little-endian directory, so will swap
        // them back.  Effectively we're constructing a "fake WAD directory"
        // here, as it would appear on disk.

	fileinfo = Z_Malloc(sizeof(filelump_t), PU_STATIC, 0);
	fileinfo->filepos = LONG(0);
	fileinfo->size = LONG(wad_file->length);

        // Name the lump after the base of the filename (without the
        // extension).

	M_ExtractFileBase (filename, fileinfo->name);
	numlumps++;
    }
    else 
    {
	// WAD file
        W_Read(wad_file, 0, &header, sizeof(header));

	if (strncmp(header.identification,"IWAD",4))
	{
	    // Homebrew levels?
	    if (strncmp(header.identification,"PWAD",4))
	    {
		I_Error ("Wad file %s doesn't have IWAD "
			 "or PWAD id\n", filename);
	    }
	    
	    // ???modifiedgame = true;		
	}

	header.numlumps = LONG(header.numlumps);
	header.infotableofs = LONG(header.infotableofs);
	length = header.numlumps*sizeof(filelump_t);
	fileinfo = Z_Malloc(length, PU_STATIC, 0);

        W_Read(wad_file, header.infotableofs, fileinfo, length);
	numlumps += header.numlumps;
    }

    // Fill in lumpinfo
    lumpinfo = realloc(lumpinfo, numlumps * sizeof(lumpinfo_t));

    if (lumpinfo == NULL)
    {
	I_Error ("Couldn't realloc lumpinfo");
    }

    lump_p = &lumpinfo[startlump];
	
    filerover = fileinfo;

    for (i=startlump; i<numlumps; ++i)
    {
	lump_p->wad_file = wad_file;
	lump_p->position = LONG(filerover->filepos);
	lump_p->size = LONG(filerover->size);
        lump_p->cache = NULL;
	strncpy(lump_p->name, filerover->name, 8);

        ++lump_p;
        ++filerover;
    }
	
    Z_Free(fileinfo);

    if (lumphash != NULL)
    {
        Z_Free(lumphash);
        lumphash = NULL;
    }

    return wad_file;
}



//
// W_NumLumps
//
int W_NumLumps (void)
{
    return numlumps;
}



//
// W_CheckNumForName
// Returns -1 if name not found.
//

int W_CheckNumForName (char* name)
{
    lumpinfo_t *lump_p;
    int i;

    // Do we have a hash table yet?

    if (lumphash != NULL)
    {
        int hash;
        
        // We do! Excellent.

        hash = W_LumpNameHash(name) % numlumps;
        
        for (lump_p = lumphash[hash]; lump_p != NULL; lump_p = lump_p->next)
        {
            if (!strncasecmp(lump_p->name, name, 8))
            {
                return lump_p - lumpinfo;
            }
        }
    } 
    else
    {
        // We don't have a hash table generate yet. Linear search :-(
        // 
        // scan backwards so patch lump files take precedence

        for (i=numlumps-1; i >= 0; --i)
        {
            if (!strncasecmp(lumpinfo[i].name, name, 8))
            {
                return i;
            }
        }
    }

    // TFB. Not found.

    return -1;
}

#pragma GCC diagnostic push			// FOR PSP: NAME check on CHAR causes compiler warning
#pragma GCC diagnostic ignored "-Waddress"	// FOR PSP: NAME check on CHAR causes compiler warning

//
// W_GetNumForName
// Calls W_CheckNumForName, but bombs out if not found.
//
int W_GetNumForName (char* name)
{
    int	i;

    i = W_CheckNumForName (name);
    
    if (i < 0)
    {
//        I_Error ("W_GetNumForName: %s not found!", name); 		// DISABLED HERE FOR PSP
	if(name == "SERIAL")
	{
	    devparm = true;
	    showintro = false;

	    printf(" ");
	    printf("\n");
	    printf(" ================================================================== ");
    	    printf("                          !!! WARNING !!!                           ");
	    printf("\n");
	    printf("                 THE IWAD FILE THAT YOU'RE TRYING                   ");
	    printf("            TO USE IS NOT A KNOWN STRIFE IWAD FILE !!!              ");
	    printf("\n");
	    printf("            PLEASE GET A KNOWN PROPER STRIFE IWAD FILE              ");
	    printf("\n");
	    printf("                THE GAME WILL QUIT IN 10 SECONDS...                  ");
	    printf(" ================================================================== ");

	    sceKernelDelayThread(10000*1000);
	    I_QuitSerialFail();
	}
	else
	    I_Error ("W_GetNumForName: %s not found!", name);		// FOR PSP: MOVED CHECK HERE

    }
 
    return i;
}

#pragma GCC diagnostic pop

//
// W_LumpLength
// Returns the buffer size needed to load the given lump.
//
int W_LumpLength (unsigned int lump)
{
    if (lump >= numlumps)
    {
	I_Error ("W_LumpLength: %i >= numlumps", lump);
    }

    return lumpinfo[lump].size;
}



//
// W_ReadLump
// Loads the lump into the given buffer,
//  which must be >= W_LumpLength().
//
void W_ReadLump(unsigned int lump, void *dest)
{
    int c;
    lumpinfo_t *l;
	
    if (lump >= numlumps)
    {
	I_Error ("W_ReadLump: %i >= numlumps", lump);
    }

    l = lumpinfo+lump;
	
    I_BeginRead ();
	
    c = W_Read(l->wad_file, l->position, dest, l->size);

    if (c < l->size)
    {
	I_Error ("W_ReadLump: only read %i of %i on lump %i",
		 c, l->size, lump);	
    }

    I_EndRead ();
}




//
// W_CacheLumpNum
//
// Load a lump into memory and return a pointer to a buffer containing
// the lump data.
//
// 'tag' is the type of zone memory buffer to allocate for the lump
// (usually PU_STATIC or PU_CACHE).  If the lump is loaded as 
// PU_STATIC, it should be released back using W_ReleaseLumpNum
// when no longer needed (do not use Z_ChangeTag).
//

void *W_CacheLumpNum(int lumpnum, int tag)
{
    byte *result;
    lumpinfo_t *lump;

    if ((unsigned)lumpnum >= numlumps)
    {
	I_Error ("W_CacheLumpNum: %i >= numlumps", lumpnum);
    }

    lump = &lumpinfo[lumpnum];

    // Get the pointer to return.  If the lump is in a memory-mapped
    // file, we can just return a pointer to within the memory-mapped
    // region.  If the lump is in an ordinary file, we may already
    // have it cached; otherwise, load it into memory.

    if (lump->wad_file->mapped != NULL)
    {
        // Memory mapped file, return from the mmapped region.

        result = lump->wad_file->mapped + lump->position;
    }
    else if (lump->cache != NULL)
    {
        // Already cached, so just switch the zone tag.

        result = lump->cache;
        Z_ChangeTag(lump->cache, tag);
    }
    else
    {
        // Not yet loaded, so load it now

        lump->cache = Z_Malloc(W_LumpLength(lumpnum), tag, &lump->cache);
	W_ReadLump (lumpnum, lump->cache);
        result = lump->cache;
    }
	
    return result;
}



//
// W_CacheLumpName
//
void *W_CacheLumpName(char *name, int tag)
{
    return W_CacheLumpNum(W_GetNumForName(name), tag);
}

// 
// Release a lump back to the cache, so that it can be reused later 
// without having to read from disk again, or alternatively, discarded
// if we run out of memory.
//
// Back in Vanilla Doom, this was just done using Z_ChangeTag 
// directly, but now that we have WAD mmap, things are a bit more
// complicated ...
//

void W_ReleaseLumpNum(int lumpnum)
{
    lumpinfo_t *lump;

    if ((unsigned)lumpnum >= numlumps)
    {
	I_Error ("W_ReleaseLumpNum: %i >= numlumps", lumpnum);
    }

    lump = &lumpinfo[lumpnum];

    if (lump->wad_file->mapped != NULL)
    {
        // Memory-mapped file, so nothing needs to be done here.
    }
    else
    {
        Z_ChangeTag(lump->cache, PU_CACHE);
    }
}

void W_ReleaseLumpName(char *name)
{
    W_ReleaseLumpNum(W_GetNumForName(name));
}

#if 0

//
// W_Profile
//
int		info[2500][10];
int		profilecount;

void W_Profile (void)
{
    int		i;
    memblock_t*	block;
    void*	ptr;
    char	ch;
    FILE*	f;
    int		j;
    char	name[9];
	
	
    for (i=0 ; i<numlumps ; i++)
    {	
	ptr = lumpinfo[i].cache;
	if (!ptr)
	{
	    ch = ' ';
	    continue;
	}
	else
	{
	    block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
	    if (block->tag < PU_PURGELEVEL)
		ch = 'S';
	    else
		ch = 'P';
	}
	info[i][profilecount] = ch;
    }
    profilecount++;
	
    f = fopen ("waddump.txt","w");
    name[8] = 0;

    for (i=0 ; i<numlumps ; i++)
    {
	memcpy (name,lumpinfo[i].name,8);

	for (j=0 ; j<8 ; j++)
	    if (!name[j])
		break;

	for ( ; j<8 ; j++)
	    name[j] = ' ';

	fprintf (f,"%s ",name);

	for (j=0 ; j<profilecount ; j++)
	    fprintf (f,"    %c",info[i][j]);

	fprintf (f,"\n");
    }
    fclose (f);
}


#endif

// Generate a hash table for fast lookups

void W_GenerateHashTable(void)
{
    unsigned int i;

    // Free the old hash table, if there is one

    if (lumphash != NULL)
    {
        Z_Free(lumphash);
    }

    // Generate hash table
    if (numlumps > 0)
    {
        lumphash = Z_Malloc(sizeof(lumpinfo_t *) * numlumps, PU_STATIC, NULL);
        memset(lumphash, 0, sizeof(lumpinfo_t *) * numlumps);

        for (i=0; i<numlumps; ++i)
        {
            unsigned int hash;

            hash = W_LumpNameHash(lumpinfo[i].name) % numlumps;

            // Hook into the hash table

            lumpinfo[i].next = lumphash[hash];
            lumphash[hash] = &lumpinfo[i];
        }
    }

    // All done!
}

// Lump names that are unique to particular game types. This lets us check
// the user is not trying to play with the wrong executable, eg.
// chocolate-doom -iwad hexen.wad.
static const struct
{
    GameMission_t mission;
    char *lumpname;
} unique_lumps[] = {
    { doom,    "POSSA1" },
    { heretic, "IMPXA1" },
    { hexen,   "ETTNA1" },
    { strife,  "AGRDA1" },
};
/*
void W_CheckCorrectIWAD(GameMission_t mission)
{
    int i;
    int lumpnum;

    for (i = 0; i < arrlen(unique_lumps); ++i)
    {
        if (mission != unique_lumps[i].mission)
        {
            lumpnum = W_CheckNumForName(unique_lumps[i].lumpname);

            if (lumpnum >= 0)
            {
                I_Error("\nYou are trying to use a %s IWAD file with "
                        "the %s%s binary.\nThis isn't going to work.\n"
                        "You probably want to use the %s%s binary.",
                        D_SuggestGameName(unique_lumps[i].mission,
                                          indetermined),
                        "chocolate-",
                        D_GameMissionString(mission),
                        "chocolate-",
                        D_GameMissionString(unique_lumps[i].mission));
            }
        }
    }
}
*/


void W_CheckSize(void)		// FOR THE PSP SOURCE PORT (TAKEN FROM PSPHEXEN BUT HEAVILY MODIFIED)
{
#ifdef SHAREWARE
    FILE *file_strife0_wad;
    file_strife0_wad = fopen("strife0.wad", "r");

    if(file_strife0_wad == NULL)
    {
    	strife0_wad_exists = 0;
	goto try_next;
    }
    else
    {
    	fseek(file_strife0_wad, 0, 2);			// file pointer at the end of file
    	strife0_wad_exists = 1;
    	filesize_strife0_wad = ftell(file_strife0_wad);	// take position of file pointer size variable
    	fclose(file_strife0_wad);
//	goto end;
    }

    try_next:
    {
    }
#endif
    FILE *file_strife1_wad;
    file_strife1_wad = fopen("strife1.wad", "r");

    if(file_strife1_wad == NULL)
    {
/*
	printf("\n STRIFE IWAD file is missing.\n");
	printf("\n");
	printf(" Please copy your STRIFE IWAD file into the game's directory.\n");
	printf("\n");
	printf("\n Quitting now...\n");
	sceKernelDelayThread(5000*1000);
	I_QuitSerialFail();
*/
    	strife1_wad_exists = 0;
    }
    else
    {
    	fseek(file_strife1_wad, 0, 2);			// file pointer at the end of file
    	strife1_wad_exists = 1;
        filesize_strife1_wad = ftell(file_strife1_wad);	// take position of file pointer size variable
        fclose(file_strife1_wad);
    }
/*
    end:
    {
    }
*/
//    if(disable_voices == 0 && (STRIFE_1_0_REGISTERED || STRIFE_1_X_REGISTERED))
    {
    	FILE *file_voices_wad;
    	file_voices_wad = fopen("voices.wad", "r");
    	if (file_voices_wad == NULL)
	{
//	    printf(" Unable to open VOICES.WAD");
    	    voices_wad_exists = 0;
	}
    	else
	{
	    fseek(file_voices_wad, 0, 2);		// file pointer at the end of file
    	    voices_wad_exists = 1;
	    filesize_voices_wad = ftell(file_voices_wad);	// take position of file pointer size variable
	    fclose(file_voices_wad);
	}
    }
}

