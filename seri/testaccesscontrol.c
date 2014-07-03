#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
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

    if( fd == -1 ) {
	perror("App: on open()");
	exit(1);
    }
    
	
	
	if(fork())
	{
		char str[] = "\n\n\nINICIO DA STRING\n\n\nIngredientes:\nPara 4 pessoas\n\n    400 g de bacalhau ;\n    3 colheres de sopa de azeite ;\n    500 g de batatas ;\n    6 ovos ;\n    3 cebolas ;\n    1 dente de alho ;\n    salsa ;\n    sal ;\n    pimenta ;\n    óleo ;\n    azeitonas pretas\n\nConfecção:\n\nDemolha-se o bacalhau como habitualmente, retira-se-lhe a pele e as espinhas e desfia-se com as mãos.\nCortam-se as batatas em palha e as cebolas em rodelas finíssimas. Pica-se o alho.\nFritam-se as batatas em óleo bem quente só até alourarem ligeiramente. Escorrem-se sobre papel absorvente.\nEntretanto, leva-se ao lume um tacho, de fundo espesso, com o azeite, a cebola e o alho e deixa-se refogar lentamente até cozer a cebola. Junta-se, nesta altura, o bacalhau desfiado e mexe-se com uma colher de madeira para que o bacalhau fique bem impregnado de gordura.\nJuntam-se as batatas ao bacalhau e com o tacho sobre o lume deitam-se os ovos ligeiramente batidos e temperados com sal e pimenta.\nMexe-se com um garfo, e logo que os ovos estejam em creme, mas cozidos, retira-se imediatamente o tacho do lume e deita-se o bacalhau num prato ou travessa.\nPolvilha-se com salsa picada e serve-se bem quente, acompanhado com azeitonas pretas.\n";
		char str2[] = "\na\n";
		fd = open(spf, O_WRONLY);
		sleep(1);
		//ioctl(fd , SERI_WRITEMODE , 0);
		for(i=0;i<2;i++)
		{
			write(fd,str2,strlen(str2));
			write(fd,str,strlen(str));
			str2[1]++;
		}
	}
	else
	{
		char str[] = "\n\n\nINICIO DA STRING\n\n\nSerial UART types\nSerial communication on PC compatibles started with the 8250 UART in the IBM XT. In the years after, new family members were introduced like the 8250A and 8250B revisions and the 16450. The last one was first implemented in the AT. The higher bus speed in this computer could not be reached by the 8250 series. The differences between these first UART series were rather minor. The most important property changed with each new release was the maximum allowed speed at the processor bus side.\nThe 16450 was capable of handling a communication speed of 38.4 kbs without problems. The demand for higher speeds led to the development of newer series which would be able to release the main processor from some of its tasks. The main problem with the original series was the need to perform a software action for each single byte to transmit or receive. To overcome this problem, the 16550 was released which contained two on-board FIFO buffers, each capable of storing 16 bytes. One buffer for incomming, and one buffer for outgoing bytes.\n\nA marvellous idea, but it didn't work out that way. The 16550 chip contained a firmware bug which made it impossible to use the buffers. The 16550A which appeared soon after was the first UART which was able to use its FIFO buffers. This made it possible to increase maximum reliable communication speeds to 115.2 kbs. This speed was necessary to use effectively modems with on-board compression. A further enhancment introduced with the 16550 was the ablity to use DMA, direct memory access for the data transfer. Two pins were redefined for this purpose. DMA transfer is not used with most applications. Only special serial I/O boards with a high number of ports contain sometimes the necessary extra circuitry to make this feature work.\nThe 16550A is the most common UART at this moment. Newer versions are under development, including the 16650 which contains two 32 byte FIFO's and on board support for software flow control. Texas Instruments is developing the 16750 which contains 64 byte FIFO's. \n";
		char str2[] = "\nA\n";
		fd = open(spf, O_WRONLY);
		//ioctl(fd , SERI_WRITEMODE , 1);
		for(i=0;i<2;i++)
		{
			write(fd,str2,strlen(str2));
			write(fd,str,strlen(str));
			str2[1]++;
		}
	}
	
			
    printf("App: closing %d\n", fd);

    if( close(fd) != 0 ) {
	perror("App: on close()");
	exit(1);
    }

    printf("App: exiting\n");

    return 0;
    

}
