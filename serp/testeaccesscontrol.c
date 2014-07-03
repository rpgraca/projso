#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/delay.h>
#include <errno.h>
#include "serp.h"
#include <sys/ioctl.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <unistd.h>

#define STRSIZE 10

int main(int argc, char *argv[]) 
{
    char *spf;
    int fd;
    int pid;
    char string[STRSIZE];
	int lidos = 0,a;
	int i;
	
    if( argc != 2 ) {
	printf("Usage: %s <special file>\n", argv[0]);
	exit(1);
    }
    spf = argv[1];
    printf("App: opening %s\n", spf);
    if( fd == -1 ) {
		perror("App: on open()");
		exit(1);
    }
    
	
	
	pid = fork();
	if(pid) 
	{
		sleep(1);
		setuid(1000);
		
	}
	fd = open(spf, O_RDONLY | O_NONBLOCK);
    
    while(1)
    {

	}	
    printf("\n\nForam lidos %d caracteres.\n",lidos);
    

    printf("App: closing %d\n", fd);

    if( close(fd) != 0 ) {
		perror("App: on close()");
		exit(1);
    }

    printf("App: exiting\n");

    return 0;
    

}
