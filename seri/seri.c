#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include "serial_reg.h"
#include <linux/ioport.h>
#include <asm/io.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/kfifo.h>
#include "seri.h"



#define UART 0x3f8
#define NUMDEVS 1
#define IRQ 4
#define BUFSIZE 1024
#define FIFOSIZE 16
#define WRITEMODE 1 // 0 usa função seri_write, diferente de 0 usa seri_write2. 1 faz wait no release, 2 faz wait no write, 3 não faz wait (RISCO DE PERDA DE INFORMAÇÃO!)
#define CHANGEMODE 0 // 1 para permitir que o utilizador altere writemode

int seri_open(struct inode * ind,struct file * f);
int seri_release(struct inode * ind,struct file * f);
int seri_ioctl(struct inode *inode, struct file *filp,unsigned int cmd, unsigned long arg);
ssize_t seri_read(struct file *filep, char __user *buff, size_t count, loff_t *offp);
ssize_t seri_write(struct file *filep, const char __user *buff, size_t count, loff_t *offp);
ssize_t seri_write2(struct file *filep, const char __user *buff, size_t count, loff_t *offp);

static irqreturn_t interrupt_handler(int irq, void *dev_id); // Interruption handler

struct sdev
{
	struct cdev cdev;
	int minor;
	int seri_owner;
	int writemode;
	struct semaphore seri_lock;
	spinlock_t readbuf_lock;
	spinlock_t writebuf_lock;
	int seri_using_count;
	wait_queue_head_t readqueue;
	wait_queue_head_t writequeue;
	struct semaphore readlock;
	struct semaphore writelock;
	struct kfifo *readbuf;
	struct kfifo *writebuf;
	unsigned char *writetmpbuf;
};

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = seri_open,
	.release = seri_release,
	.read = seri_read,
	.write = seri_write,
	.llseek = no_llseek,
	.ioctl = seri_ioctl,
};
static int major;
static struct sdev *seri_devs;


static int seri_init(void)
{
	int result,i;
	unsigned char lcr,fifo;
	struct resource *res;
	dev_t dev;
		
	// Criar dispositivo
	if((result = alloc_chrdev_region(&dev,0,NUMDEVS,"seri"))) return result;
	major = MAJOR(dev);
	
	
	seri_devs = (struct sdev *) kmalloc(sizeof(struct sdev)*NUMDEVS,GFP_KERNEL);
	if (!seri_devs) 
	{
		unregister_chrdev_region(dev, NUMDEVS);
		return -ENOMEM;
	}
	
	if(WRITEMODE != 0) fops.write = seri_write2;
	
	for(i = 0;i < NUMDEVS;i++)
	{
		seri_devs[i].minor = MINOR(dev) + i;
		cdev_init(&seri_devs[i].cdev,&fops);
		seri_devs[i].cdev.owner = THIS_MODULE;
		seri_devs[i].cdev.ops = &fops;
		seri_devs[i].readbuf_lock = SPIN_LOCK_UNLOCKED;
		seri_devs[i].writebuf_lock = SPIN_LOCK_UNLOCKED;
		seri_devs[i].readbuf = NULL;
		seri_devs[i].writebuf = NULL;
		seri_devs[i].seri_using_count = 0;
		seri_devs[i].writemode = WRITEMODE;
		init_waitqueue_head(&(seri_devs[i].readqueue));
		init_waitqueue_head(&(seri_devs[i].writequeue));
		init_MUTEX(&(seri_devs[i].seri_lock));
		init_MUTEX(&(seri_devs[i].readlock));
		init_MUTEX(&(seri_devs[i].writelock));
		if( (result = cdev_add(&seri_devs[i].cdev,MKDEV(major,i),1) )) 
		{
			printk(KERN_ALERT "Erro ao criar o dispositivo %d\n",i);
			unregister_chrdev_region(dev, NUMDEVS);
			kfree(seri_devs);			
			return result;
		}
	}
	
	// Iniciar UART
	res = request_region(UART,8,"seri");
	if(res == NULL) 
	{
		printk(KERN_ALERT "ERRO! Recurso indisponível.\n");
		unregister_chrdev_region(dev, NUMDEVS);
		kfree(seri_devs);
		return -EBUSY;
	}
	
	outb(UART_IER_RDI | UART_IER_THRI,UART+UART_IER); // Interrupções
	
	// Iniciar e limpar FIFO
	fifo = UART_FCR_ENABLE_FIFO | UART_FCR_TRIGGER_8 | UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT;
	outb(fifo,UART+UART_FCR);
	
	// Atribuir configurações default (podem ser alteradas pelo utilizador por ioctl)
	lcr = UART_LCR_PARITY | UART_LCR_WLEN8 | UART_LCR_STOP | UART_LCR_EPAR | UART_LCR_DLAB; // 8 bit word, paridade par, 2 stop bits, DLAB = 1
	outb(lcr,UART+UART_LCR);
	outb(0x00,UART+UART_DLM);
	outb(UART_DIV_1200,UART+UART_DLL);
	lcr = lcr & ~UART_LCR_DLAB;
	outb(lcr,UART+UART_LCR); // DLAB = 0;
	
	printk(KERN_ALERT "Modulo SERI instalado com sucesso!\nMajor: %d\n",major);
	return 0;
}

static void seri_exit(void)
{
	int i;
	release_region(UART,8);
	for(i = 0;i < NUMDEVS;i++)
	{
		cdev_del(&seri_devs[i].cdev);
	}
	kfree(seri_devs);
	unregister_chrdev_region(MKDEV(major,0),NUMDEVS);	
	printk(KERN_ALERT "Modulo SERI removido com sucesso!\n");
}


int seri_ioctl(struct inode *inode, struct file *filp,unsigned int cmd, unsigned long arg)
{
	unsigned char lcr = inb(UART+UART_LCR);
	int baud;
	struct sdev *dev = filp->private_data;
	unsigned char dlm,dll;
	switch(cmd)
	{
		case SERI_WLEN:
							switch(arg)
							{
								case 5:		lcr = lcr & ~UART_LCR_WLEN8; // Limpa dois bits de WLEN, visto que WLEN8 é 11
											lcr = lcr | UART_LCR_WLEN5;
											outb(lcr,UART+UART_LCR); 
											return 0;
								case 6:		lcr = lcr & ~UART_LCR_WLEN8;
											lcr = lcr | UART_LCR_WLEN6;
											outb(lcr,UART+UART_LCR); 
											return 0;
								case 7:		lcr = lcr & ~UART_LCR_WLEN8;
											lcr = lcr | UART_LCR_WLEN7;
											outb(lcr,UART+UART_LCR); 
											return 0;
								case 8:		lcr = lcr | UART_LCR_WLEN8;
											outb(lcr,UART+UART_LCR); 
											return 0;
								default:	return -EINVAL;
							}
		case SERI_NUM_SB:
							switch(arg)
							{
								case 1: 	lcr = lcr & ~UART_LCR_STOP;
											outb(lcr,UART+UART_LCR); 
											return 0;
								case 2: 	lcr = lcr | UART_LCR_STOP;
											outb(lcr,UART+UART_LCR); 
											return 0;
								default:	return -EINVAL;
							}
		case SERI_PARITY:
							switch(arg)
							{
								case NONEPAR:	lcr = lcr & ~UART_LCR_PARITY; 
												outb(lcr,UART+UART_LCR); 
												return 0;
								case ODDPAR:	lcr = lcr & ~UART_LCR_SPAR & ~UART_LCR_EPAR;
												lcr = lcr | UART_LCR_PARITY;
												outb(lcr,UART+UART_LCR); 
												return 0;
								case EVENPAR:	lcr = lcr & ((~UART_LCR_SPAR) | UART_LCR_EPAR | UART_LCR_PARITY); 
												outb(lcr,UART+UART_LCR); 
												return 0;
								case ONEPAR:	lcr = lcr | UART_LCR_SPAR | UART_LCR_PARITY;
												lcr = lcr & ~UART_LCR_EPAR;
												outb(lcr,UART+UART_LCR); 
												return 0;
								case ZEROPAR:	lcr = lcr | UART_LCR_SPAR | UART_LCR_EPAR | UART_LCR_PARITY;
												outb(lcr,UART+UART_LCR); 
												return 0;
								default:		return -EINVAL;
							}
		case SERI_BAUD:
							if(arg > 65536 - 1) return -EINVAL; // O divisor será no máximo 2^16 - 1, visto que se trate de 2 bytes
							lcr = lcr | UART_LCR_DLAB;
							outb(lcr,UART+UART_LCR); // DLAB = 1;							
							dlm = (unsigned char) (arg >> 0x08);
							dll = (unsigned char) (arg & 0xFF);
							outb(dlm,UART+UART_DLM);
							outb(dll,UART+UART_DLL);
							lcr = lcr & ~UART_LCR_DLAB;
							outb(lcr,UART+UART_LCR); // DLAB = 0;
							return 0;
		case SERI_SBC:
							switch(arg)
							{
								case SET: 	lcr = lcr | UART_LCR_SBC;
											outb(lcr,UART+UART_LCR);
											return 0;
								case UNSET:	lcr = lcr & ~UART_LCR_SBC;
											outb(lcr,UART+UART_LCR);
											return 0;
								default:	return -EINVAL;
							}
		case SERI_GETLCR:	
							return (unsigned char) lcr & ~UART_LCR_DLAB;
		case SERI_GETBAUD:	
							lcr = lcr | UART_LCR_DLAB;
							outb(lcr,UART+UART_LCR); // DLAB = 1;
							baud = inb(UART+UART_DLM);
							baud = baud << 0x08;
							baud = baud | inb(UART+UART_DLL);
							lcr = lcr & ~UART_LCR_DLAB;
							outb(lcr,UART+UART_LCR); // DLAB = 0;
							return baud;
		
		case SERI_WRITEMODE:
					if(CHANGEMODE)
					{
							down(&(dev->seri_lock));
							if (dev->seri_using_count != 1)
							{
								up(&(dev->seri_lock));
								return -EBUSY;
							}
							if(arg != 0)
							{
								if(dev->writemode == 0)
								{
									if( (dev->writebuf = kfifo_alloc(BUFSIZE,GFP_KERNEL,&(dev->writebuf_lock))) == NULL ) 
									{
										up(&(dev->seri_lock));
										printk(KERN_ALERT "Buffer de escrita demasiado grande\n");
										return -ENOMEM;
									}
									if( (dev->writetmpbuf = (unsigned char*) kmalloc( sizeof(unsigned char*) * FIFOSIZE, GFP_KERNEL)) == NULL )
									{
										up(&(dev->seri_lock));
										printk(KERN_ALERT "Buffer temporario de escrita demasiado grande\n");
										return -ENOMEM;
									}
								}
								
								fops.write = seri_write2;
							}
							else
							{
								if(dev->writemode != 0)
								{
									if(dev->writemode == 1)
									{
										if(wait_event_interruptible(dev->writequeue,kfifo_len(dev->writebuf) == 0))
										{
											up(&(dev->seri_lock));
											return -ERESTARTSYS;
										}
									}
									kfree(dev->writetmpbuf);
									kfifo_free(dev->writebuf);
								}
								
								fops.write = seri_write;
							}
							dev->writemode = arg;
							up(&(dev->seri_lock));
							return 0;
						}
						else return -EACCES;
						
							
		default:
							return -ENOTTY;
	}
}

int seri_open(struct inode * ind,struct file * f)
{
	struct sdev *dev;
	dev = container_of(ind->i_cdev, struct sdev, cdev);
	f->private_data = dev;
	nonseekable_open(ind, f);
	//printk(KERN_ALERT "Quero abrir o ficheiro! pointer: %d\nUID: %d\nEUID: %d\n",dev->minor,current->uid,current->euid);
	
	down(&(dev->seri_lock));
	if (dev->seri_using_count && (dev->seri_owner != current->uid) && (dev->seri_owner != current->euid) && !capable(CAP_DAC_OVERRIDE))
	{
		up(&(dev->seri_lock));
		return -EBUSY;
	}
	
	// Reservar interrupções e alocar kfifo na primeira abertura
	if(dev->seri_using_count == 0)
	{
		if(request_irq(IRQ, interrupt_handler, SA_SHIRQ, "seri", dev))
		{
			printk(KERN_ALERT "Impossivel obter interrupções.\n");
			up(&(dev->seri_lock));
			return -EBUSY;
		}
		if(f->f_mode | FMODE_READ)
		{
			if( (dev->readbuf = kfifo_alloc(BUFSIZE,GFP_KERNEL,&(dev->readbuf_lock))) == NULL ) 
			{
				printk(KERN_ALERT "Buffer de leitura demasiado grande\n");
				up(&(dev->seri_lock));
				free_irq(IRQ, dev);
				return -ENOMEM;
			}
		}
		if((f->f_mode | FMODE_WRITE) && dev->writemode != 0)
		{
			if( (dev->writebuf = kfifo_alloc(BUFSIZE,GFP_KERNEL,&(dev->writebuf_lock))) == NULL ) 
			{
				printk(KERN_ALERT "Buffer de escrita demasiado grande\n");
				up(&(dev->seri_lock));
				free_irq(IRQ, dev);
				if(f->f_mode | FMODE_READ)
					kfifo_free(dev->readbuf);
				return -ENOMEM;
			}
			if( (dev->writetmpbuf = (unsigned char*) kmalloc( sizeof(unsigned char*) * FIFOSIZE, GFP_KERNEL)) == NULL )
			{
				printk(KERN_ALERT "Buffer temporario de escrita demasiado grande\n");
				up(&(dev->seri_lock));
				free_irq(IRQ, dev);
				kfifo_free(dev->writebuf);
				if(f->f_mode | FMODE_READ)
					kfifo_free(dev->readbuf);
				return -ENOMEM;
			}
		}
		//printk(KERN_ALERT "Aloquei tudo!\n");
	}
		
	
	if (dev->seri_using_count == 0)
		dev->seri_owner = current->uid;
		
	dev->seri_using_count++;
	up(&(dev->seri_lock));
	//printk(KERN_ALERT "Abri o ficheiro! pointer: %d\nUID: %d\nEUID: %d\n",dev->minor,current->uid,current->euid);
	
	
	return 0;
}

int seri_release(struct inode * ind,struct file * f)
{
	struct sdev *dev = f->private_data;
	
	down(&(dev->seri_lock));
	// Libertar kfifo e interrupções quando o ultimo processo libertar o dispositivo
	if(dev->seri_using_count == 1)
	{
		if(f->f_mode | FMODE_READ)
			kfifo_free(dev->readbuf);
		if((f->f_mode | FMODE_WRITE) && dev->writemode != 0)
		{
			if(dev->writemode == 1)
			{
				if(wait_event_interruptible(dev->writequeue,kfifo_len(dev->writebuf) == 0))
				{
					return -ERESTARTSYS;
				}
			}
			kfree(dev->writetmpbuf);
			kfifo_free(dev->writebuf);
		}
		free_irq(IRQ, dev);
	}
	dev->seri_using_count--; /* nothing else */
	up(&(dev->seri_lock));
	//printk(KERN_ALERT "Fechei o ficheiro!\n");
	return 0;
}

ssize_t seri_read(struct file *filep, char __user *buff, size_t count, loff_t *offp)
{
	int recebidos;
	int naocopiados = 0;
	struct sdev *dev = filep->private_data;
	char *tmpbuf;
	
	if(filep->f_flags & O_NONBLOCK)	
	{
		if(down_trylock(&(dev->readlock))) return -EAGAIN;
	}
	else 
	{
		if(down_interruptible(&(dev->readlock))) return -ERESTARTSYS;
	}
	
	tmpbuf = (char *) kmalloc(sizeof(char) * count, GFP_KERNEL);
	if(tmpbuf == NULL)
	{
		up(&(dev->readlock));
		return -ENOMEM;
	}
	
	//printk(KERN_ALERT "Quero ler.\n");
	if(kfifo_len(dev->readbuf) == 0 && (filep->f_flags & O_NONBLOCK))
	{
		kfree(tmpbuf);
		up(&(dev->readlock));
		return -EAGAIN;
	}
	else if(wait_event_interruptible(dev->readqueue,kfifo_len(dev->readbuf) > 0))
	{
		kfree(tmpbuf);
		up(&(dev->readlock));
		return -ERESTARTSYS;
	}
	//printk(KERN_ALERT "Acordei para ler.\n");
	recebidos = kfifo_get(dev->readbuf,tmpbuf,count);
	if(recebidos > 0) naocopiados = copy_to_user(buff,tmpbuf,recebidos);
	
	kfree(tmpbuf);
	up(&(dev->readlock));
	return recebidos - naocopiados;
	
}

ssize_t seri_write(struct file *filep, const char __user *buff, size_t count, loff_t *offp)
{
	struct sdev *dev = filep->private_data;
	int notcopied;
	int i,j;
	int numblocos;
	int sizelast;
	char *kbuf;
	
	if(filep->f_flags & O_NONBLOCK)	
	{
		if(down_trylock(&(dev->writelock))) return -EAGAIN;
	}
	else 
	{
		if(down_interruptible(&(dev->writelock))) return -EINTR;
	}
		
	if((kbuf = (char*) kmalloc( sizeof(char) * FIFOSIZE,GFP_KERNEL)) == NULL)
	{
		up(&(dev->writelock));
		return -ENOMEM;
	}
	
	numblocos = count/FIFOSIZE;
	sizelast = count%FIFOSIZE;
	
	for(i = 0; i < numblocos; i++)
	{
		if( ! ( inb(UART+UART_LSR) & UART_LSR_THRE ) )
		{
			if(filep->f_flags & O_NONBLOCK)
			{
				kfree(kbuf);
				up(&(dev->writelock));
				return -EAGAIN; // não bloquear em caso de O_NONBLOCK
			}
			//printk(KERN_ALERT "WRITE: Estou aqui\n");
			wait_event_interruptible(dev->writequeue,( inb(UART+UART_LSR) & UART_LSR_THRE ) != 0); // Esperar até poder escrever caracter
		}
		notcopied = copy_from_user(kbuf,buff+i*FIFOSIZE,FIFOSIZE);
		for(j = 0;j < FIFOSIZE-notcopied;j++)
		{
			outb(kbuf[j],UART+UART_TX);
		}
	}
	
	while( ! ( inb(UART+UART_LSR) & UART_LSR_THRE ) )
	{
		if(filep->f_flags & O_NONBLOCK)
		{
			kfree(kbuf);
			up(&(dev->writelock));
			return -EAGAIN; // não bloquear em caso de O_NONBLOCK
		}
		//printk(KERN_ALERT "WRITE: Estou aqui\n");
		wait_event_interruptible(dev->writequeue,( inb(UART+UART_LSR) & UART_LSR_THRE ) != 0);  // Esperar até poder escrever caracter
	}
	notcopied = copy_from_user(kbuf,buff+i*FIFOSIZE,sizelast);
	for(j = 0;j < sizelast-notcopied;j++)
	{
		outb(kbuf[j],UART+UART_TX);
	}
	
	kfree(kbuf);
	up(&(dev->writelock));
	return i;
}

ssize_t seri_write2(struct file *filep, const char __user *buff, size_t count, loff_t *offp)
{
	struct sdev *dev = filep->private_data;
	int copiados;
	int i;
	int numblocos;
	int sizelast;
	char *kbuf;
	int escritos=0;
	
	if(filep->f_flags & O_NONBLOCK)	
	{
		if(down_trylock(&(dev->writelock))) return -EAGAIN;
	}
	else 
	{
		if(down_interruptible(&(dev->writelock))) return -ERESTARTSYS;
	}
		
	if((kbuf = (char*) kmalloc( sizeof(char) * count,GFP_KERNEL)) == NULL)
	{
		up(&(dev->writelock));
		return -ENOMEM;
	}
	
	copiados = count - copy_from_user(kbuf,buff,count);
	
	numblocos = copiados/(BUFSIZE+1);
	sizelast = copiados%(BUFSIZE+1);
	for(i = 0; i < numblocos; i++)
	{
		if(wait_event_interruptible(dev->writequeue,BUFSIZE-kfifo_len(dev->writebuf) >= BUFSIZE))
		{
			kfree(kbuf);
			up(&(dev->writelock));
			return -ERESTARTSYS;
		}
		escritos += kfifo_put(dev->writebuf,kbuf+1+i*(BUFSIZE+1),BUFSIZE);
		outb(kbuf[i*(BUFSIZE+1)],UART+UART_TX);	
	}
	if(wait_event_interruptible(dev->writequeue,BUFSIZE-kfifo_len(dev->writebuf) >= sizelast))
	{
			kfree(kbuf);
			up(&(dev->writelock));
			return -ERESTARTSYS;
	}
	if(kfifo_len(dev->writebuf) == 0)
	{
		escritos += kfifo_put(dev->writebuf,kbuf+1+i*(BUFSIZE+1),sizelast-1) + 1;
		outb(kbuf[i*(BUFSIZE+1)],UART+UART_TX);
	}
	else
	{
		escritos += kfifo_put(dev->writebuf,kbuf+i*(BUFSIZE+1),sizelast);
	}
	
	if(dev->writemode == 2) // No modo 2, espera que todos os dados sejam enviados antes de retornar ao utilizador
	{
		if(wait_event_interruptible(dev->writequeue,kfifo_len(dev->writebuf) == 0))
		{
			kfree(kbuf);
			up(&(dev->writelock));
			return -ERESTARTSYS;
		}
	}
	
	kfree(kbuf);
	up(&(dev->writelock));
	return escritos;
}

static irqreturn_t interrupt_handler(int irq, void *dev_id)
{
	struct sdev *dev = (struct sdev*) dev_id;
	unsigned char received;
	unsigned char iirstatus;
	int j;
	irqreturn_t ret = IRQ_NONE;
	iirstatus = inb(UART+UART_IIR);
	if(iirstatus & UART_IIR_THRI)
	{
		if(dev->writemode != 0 && dev->writebuf != NULL)
		{
			received = kfifo_get(dev->writebuf,dev->writetmpbuf,FIFOSIZE);
			wake_up_interruptible(&(dev->writequeue));
			for(j=0;j<received;j++)
			{	
				outb(dev->writetmpbuf[j],UART+UART_TX);
			}
		}
		else if(dev->writebuf != NULL)
		{
			wake_up_interruptible(&(dev->writequeue));
		}
		ret = IRQ_HANDLED;
	}
	if((iirstatus & UART_IIR_RDI)  && dev->readbuf != NULL)
	{
		while(inb(UART+UART_LSR) & UART_LSR_DR)
		{
			received = inb(UART+UART_RX);
			kfifo_put(dev->readbuf,&received,1);
		}
		wake_up_interruptible(&(dev->readqueue));
		ret = IRQ_HANDLED;
	}
	return ret;
}


module_init(seri_init);
module_exit(seri_exit);
MODULE_LICENSE("Dual BSD/GPL");
