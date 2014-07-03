#define SERI_IOC_MAGIC 0xBB
#define SERI_WLEN 		_IOW(SERI_IOC_MAGIC,0,int) // Word Length -> Aceita inteiro entre 5 ou 8
#define SERI_NUM_SB 	_IOW(SERI_IOC_MAGIC,1,int) // Número de Stop Bits, deve ser 1 ou 2
#define SERI_PARITY 	_IOW(SERI_IOC_MAGIC,2,int) // Paridade, deve ser NONEPAR, EVENPAR, ODDPAR, ONEPAR, ZEROPAR
#define SERI_BAUD 		_IOW(SERI_IOC_MAGIC,3,int) // Divisor para a baudrate
#define SERI_SBC		_IOW(SERI_IOC_MAGIC,4,int) // Deve ser SET para ativar, UNSET para desativar
#define SERI_GETLCR 	_IOR(SERI_IOC_MAGIC,5,unsigned char) // Retorna ao utilizador a configuração do LCR
#define SERI_GETBAUD 	_IOR(SERI_IOC_MAGIC,6,int) // Retorna ao utilizador a bauderate
#define SERI_WRITEMODE 	_IOW(SERI_IOC_MAGIC,7,int) // Muda função utilizada para escrita

#define SET 1
#define UNSET 0

#define NONEPAR 0
#define ODDPAR 1
#define EVENPAR 2
#define ONEPAR 3
#define ZEROPAR 4


