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
    if( argc != 2 ) {
	printf("Usage: %s <special file>\n", argv[0]);
	exit(1);
    }
    spf = argv[1];
    printf("App: opening %s\n", spf);
    
	char string[STRSIZE];
	int lidos = 0,a;
	int i;

	fd = open(spf, O_RDONLY | O_NONBLOCK);
    



    if( fd == -1 ) {
		perror("App: on open()");
		exit(1);
    }


    printf("No inicio, o LCR é %x e o divisor da baudrate é %d\n",ioctl(fd,SERP_GETLCR),ioctl(fd,SERP_GETBAUD));
    for(i=4;i<10;i++)
    {
		if(ioctl(fd,SERP_WLEN,i)) printf("Invalido\n");
		printf("Teste WLEN: #%d, o LCR é %x e o divisor da baudrate é %d\n",i,ioctl(fd,SERP_GETLCR),ioctl(fd,SERP_GETBAUD));
    }
   
    for(i=0;i<4;i++)
    {
		if(ioctl(fd,SERP_NUM_SB,i)) printf("Invalido\n");
		printf("Teste SB: #%d, o LCR é %x e o divisor da baudrate é %d\n",i,ioctl(fd,SERP_GETLCR),ioctl(fd,SERP_GETBAUD));
	}
	for(i = -1;i<6;i++)
	{
		if(ioctl(fd,SERP_PARITY,i)) printf("Invalido\n");
		printf("Teste PARITY: #%d, o LCR é %x e o divisor da baudrate é %d\n",i,ioctl(fd,SERP_GETLCR),ioctl(fd,SERP_GETBAUD));
	} 
	for(i = -1;i<2;i++)
	{
		if(ioctl(fd,SERP_SBC,i)) printf("Invalido\n");
		printf("Teste SBC: #%d, o LCR é %x e o divisor da baudrate é %d\n",i,ioctl(fd,SERP_GETLCR),ioctl(fd,SERP_GETBAUD));
	}



	ioctl(fd,SERP_BAUD,96);
	printf("Teste BAUD o LCR é %x e o divisor da baudrate é %d\n",ioctl(fd,SERP_GETLCR),ioctl(fd,SERP_GETBAUD));
	ioctl(fd,SERP_BAUD,84);
	printf("Teste BAUD o LCR é %x e o divisor da baudrate é %d\n",ioctl(fd,SERP_GETLCR),ioctl(fd,SERP_GETBAUD));
	ioctl(fd,SERP_BAUD,17);
	printf("Teste BAUD o LCR é %x e o divisor da baudrate é %d\n",ioctl(fd,SERP_GETLCR),ioctl(fd,SERP_GETBAUD));
	ioctl(fd,SERP_BAUD,0xFFFF);
	printf("Teste BAUD o LCR é %x e o divisor da baudrate é %d\n",ioctl(fd,SERP_GETLCR),ioctl(fd,SERP_GETBAUD));
	


    printf("App: closing %d\n", fd);

    if( close(fd) != 0 ) {
		perror("App: on close()");
		exit(1);
    }

    printf("App: exiting\n");

    return 0;
    

}
