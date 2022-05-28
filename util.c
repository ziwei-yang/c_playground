#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *load_file(char *fpath) {
	FILE *fp;
	long lSize;
	char *buffer;

	fp = fopen ( fpath , "rb" );
	if( !fp ) perror(fpath),exit(1);

	fseek( fp , 0L , SEEK_END);
	lSize = ftell( fp );
	rewind( fp );

	/* allocate memory for entire content */
	buffer = calloc( 1, lSize+1 );
	if( !buffer ) fclose(fp),fputs("memory alloc fails",stderr),exit(1);

	/* copy the file into the buffer */
	if( 1!=fread( buffer , lSize, 1 , fp) )
		fclose(fp),free(buffer),fputs("entire read fails",stderr),exit(1);

	fclose(fp);
	return buffer;
}
