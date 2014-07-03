#define SERP_IOC_MAGIC 0xBB
#define SERP_WLEN 		_IOW(SERP_IOC_MAGIC,0,int) // Word Length -> Aceita inteiro entre 5 ou 8
#define SERP_NUM_SB 	_IOW(SERP_IOC_MAGIC,1,int) // Número de Stop Bits, deve ser 1 ou 2
#define SERP_PARITY 	_IOW(SERP_IOC_MAGIC,2,int) // Paridade, deve ser NONEPAR, EVENPAR, ODDPAR, ONEPAR, ZEROPAR
#define SERP_BAUD 		_IOW(SERP_IOC_MAGIC,3,int) // Divisor para a baudrate
#define SERP_SBC		_IOW(SERP_IOC_MAGIC,4,int) // Deve ser SET para ativar, UNSET para desativar
#define SERP_GETLCR 	_IOR(SERP_IOC_MAGIC,5,unsigned char) // Retorna ao utilizador a configuração do LCR
#define SERP_GETBAUD 	_IOR(SERP_IOC_MAGIC,6,int) // Retorna ao utilizador a bauderate

#define SET 1
#define UNSET 0

#define NONEPAR 0
#define ODDPAR 1
#define EVENPAR 2
#define ONEPAR 3
#define ZEROPAR 4


