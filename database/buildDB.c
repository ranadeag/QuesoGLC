/* This file builds the database used by QuesoGLC to access to unicode datas */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gdbm.h>

int main(int argc, char** argv)
{
    FILE *uniFile = NULL;
    char buffer[256];
    char *end = NULL;
    int code = 0;
    GDBM_FILE unicod1;
    GDBM_FILE unicod2;
    datum key , content;
    
    if (argc < 2) {
	printf("\tUse of buildDB :\tbuildDB <file>\n");
	printf("\twhere <file> is a Unicode Data file in ASCII format\n");
	return -1;
    }
    
    /* Opens the Unicode Data file */
    uniFile = fopen(argv[1], "r");
    if (!uniFile) {
	printf("failed to open '%s'\n", argv[1]);
	return -1;
    }
    
    /* Opens the first database which converts code into Unicode names */
    unicod1 = gdbm_open("unicode1.db", 0, GDBM_WRCREAT, 0666, NULL);
    if (!unicod1) {
	printf("failed to open 'unicode1.db'\n");
	return -1;
    }

    /* Opens the second database which converts Unicode names into code*/
    unicod2 = gdbm_open("unicode2.db", 0, GDBM_WRCREAT, 0666, NULL);
    if (!unicod2) {
	printf("failed to open 'unicode2.db'\n");
	return -1;
    }

    /* Initializes datums */
    key.dsize = sizeof(int);
    key.dptr = (char *)&code;
    content.dptr = &buffer[5];
    
    while(!feof(uniFile)) {
	fgets(buffer, 256, uniFile);
    
	/* Skip the beginning comments */
	if (buffer[0] == '#')
	    continue;
	
	/* Split buffer into two null-terminated strings and read the hexa
	   code in the first 4 columns */
	buffer[4] = 0;
	code = strtol(buffer, NULL, 16);
    
	/* Find the end of the next field and replace it with a NULL char */
	end = strstr((const char*)&buffer[5], ";");
	if (end)
	    *end = 0;
	/* We can now read the Unicode character name */
	content.dsize = strlen(&buffer[5]) + 1;
	
	/* Stores key=code content=name into the first database
		  key=name content=code into the second one */
	gdbm_store(unicod1, key, content, GDBM_REPLACE);
	gdbm_store(unicod2, content, key, GDBM_REPLACE);
    }    

    /* Closes the databases */
    gdbm_close(unicod1);
    gdbm_close(unicod2);
    
    /* Closes the Unicode Data file */
    fclose(uniFile);
    
    return 0;
}