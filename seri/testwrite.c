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
    
	

	char str[] = "Ingredientes:\nPara 4 pessoas\n\n    400 g de bacalhau ;\n    3 colheres de sopa de azeite ;\n    500 g de batatas ;\n    6 ovos ;\n    3 cebolas ;\n    1 dente de alho ;\n    salsa ;\n    sal ;\n    pimenta ;\n    óleo ;\n    azeitonas pretas\n\nConfecção:\n\nDemolha-se o bacalhau como habitualmente, retira-se-lhe a pele e as espinhas e desfia-se com as mãos.\nCortam-se as batatas em palha e as cebolas em rodelas finíssimas. Pica-se o alho.\nFritam-se as batatas em óleo bem quente só até alourarem ligeiramente. Escorrem-se sobre papel absorvente.\nEntretanto, leva-se ao lume um tacho, de fundo espesso, com o azeite, a cebola e o alho e deixa-se refogar lentamente até cozer a cebola. Junta-se, nesta altura, o bacalhau desfiado e mexe-se com uma colher de madeira para que o bacalhau fique bem impregnado de gordura.\nJuntam-se as batatas ao bacalhau e com o tacho sobre o lume deitam-se os ovos ligeiramente batidos e temperados com sal e pimenta.\nMexe-se com um garfo, e logo que os ovos estejam em creme, mas cozidos, retira-se imediatamente o tacho do lume e deita-se o bacalhau num prato ou travessa.\nPolvilha-se com salsa picada e serve-se bem quente, acompanhado com azeitonas pretas.\n";
	write(fd,str,strlen(str));

	
			
    printf("App: closing %d\n", fd);

    if( close(fd) != 0 ) {
	perror("App: on close()");
	exit(1);
    }

    printf("App: exiting\n");

    return 0;
    

}
