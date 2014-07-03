#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include "seri.h"

int main(int argc, char *argv[]) {
    char *spf;
    int fd;
    int i;
    if( argc != 2 ) {
	printf("Usage: %s <special file>\n", argv[0]);
	exit(1);
    }
    spf = argv[1];
    printf("App: opening %s\n", spf);

    fd = open(spf, O_WRONLY);

    if( fd == -1 ) {
	perror("App: on open()");
	exit(1);
    }
    
	ioctl(fd , SERI_WRITEMODE , 4);
	

	char str[] = "AAAAAAA\n";
	ioctl(fd , SERI_WRITEMODE , 0);
	write(fd,str,strlen(str));
	ioctl(fd , SERI_WRITEMODE , 1);
	write(fd,str,strlen(str));
	ioctl(fd , SERI_WRITEMODE , 2);
	write(fd,str,strlen(str));
	ioctl(fd , SERI_WRITEMODE , 3);
	write(fd,str,strlen(str));
			
    printf("App: closing %d\n", fd);

    if( close(fd) != 0 ) {
	perror("App: on close()");
	exit(1);
    }

    printf("App: exiting\n");

    return 0;
    

}
