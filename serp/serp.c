#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/spinlock.h>
#include <asm/semaphore.h>
#include "serp.h"
#include "serial_reg.h"

#define UART 0x3f8
#define READWAITMS 15
#define NUMDEVS 1
#define FIFOSIZE 16
#define READTIMEOUT 1



int serp_open(struct inode * ind,struct file * f);
int serp_release(struct inode * ind,struct file * f);
int serp_ioctl(struct inode *inode, struct file *filp,unsigned int cmd, unsigned long arg);
ssize_t serp_read(struct file *filep, char __user *buff, size_t count, loff_t *offp);
ssize_t serp_write(struct file *filep, const char __user *buff, size_t count, loff_t *offp);


struct sdev
{
	struct cdev cdev;
	int minor;
	int serp_owner;
	spinlock_t serp_lock;
	int serp_using_count;
	struct semaphore readlock;
	struct semaphore writelock;
};

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = serp_open,
	.release = serp_release,
	.read = serp_read,
	.write = serp_write,
	.llseek = no_llseek,
	.ioctl = serp_ioctl,
};

static int major;
static struct sdev *serp_devs;

static int serp_init(void)
{
	int result,i;
	unsigned char lcr,fifo;
	struct resource *res;
	dev_t dev;
	

	
	
	// Criar dispositivo
	if((result = alloc_chrdev_region(&dev,0,NUMDEVS,"serp"))) return result;
	major = MAJOR(dev);
	printk(KERN_ALERT "serp:\nMAJOR: %d\n",major);
	
	serp_devs = (struct sdev *) kmalloc(sizeof(struct sdev)*NUMDEVS,GFP_KERNEL);
	if (!serp_devs) 
	{
		unregister_chrdev_region(dev, NUMDEVS);
		return -ENOMEM;
	}
	
	for(i = 0;i < NUMDEVS;i++)
	{
		serp_devs[i].minor = MINOR(dev) + i;
		cdev_init(&serp_devs[i].cdev,&fops);
		serp_devs[i].cdev.owner = THIS_MODULE;
		serp_devs[i].cdev.ops = &fops;
		serp_devs[i].serp_lock = SPIN_LOCK_UNLOCKED;
		serp_devs[i].serp_using_count = 0;
		init_MUTEX(&(serp_devs[i].readlock));
		init_MUTEX(&(serp_devs[i].writelock));
		if( (result = cdev_add(&serp_devs[i].cdev,MKDEV(major,i),1) )) 
		{
			printk(KERN_ALERT "Erro ao criar o dispositivo %d\n",i);
			kfree(serp_devs);
			unregister_chrdev_region(dev, NUMDEVS);
			return result;
		}
	}
	
	// Iniciar UART
	res = request_region(UART,8,"serp");
	if(res == NULL) 
	{
		printk(KERN_ALERT "ERRO! Recurso indisponível.\n");
		kfree(serp_devs);
		unregister_chrdev_region(dev, NUMDEVS);
		return -EBUSY;
	}
	
	outb(0,UART+UART_IER); // Desativar interrupções
	
	// Iniciar FIFO
	fifo = UART_FCR_ENABLE_FIFO;
	outb(fifo,UART+UART_FCR);
	
	// Atribuir configurações default (podem ser alteradas pelo utilizador por ioctl)
	lcr = UART_LCR_PARITY | UART_LCR_WLEN8 | UART_LCR_STOP | UART_LCR_EPAR | UART_LCR_DLAB; // 8 bit word, paridade par, 2 stop bits, DLAB = 1
	outb(lcr,UART+UART_LCR);
	outb(0x00,UART+UART_DLM);
	outb(0x60,UART+UART_DLL); // 115200/1200 = 0x60
	lcr = lcr & ~UART_LCR_DLAB;
	outb(lcr,UART+UART_LCR); // DLAB = 0;
	
	
	//outb('h',UART+UART_TX);	
	
	
	printk(KERN_ALERT "Modulo instalado com sucesso!\n");
	return 0;
}

static void serp_exit(void)
{
	int i;
	release_region(UART,8);
	for(i = 0;i < NUMDEVS;i++)
	{
		cdev_del(&serp_devs[i].cdev);
	}
	kfree(serp_devs);
	unregister_chrdev_region(MKDEV(major,0),NUMDEVS);	
	printk(KERN_ALERT "Modulo removido com sucesso!\n");
}

int serp_ioctl(struct inode *inode, struct file *filp,unsigned int cmd, unsigned long arg)
{
	unsigned char lcr = inb(UART+UART_LCR);
	int baud;
	unsigned char dlm,dll;
	switch(cmd)
	{
		case SERP_WLEN:
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
		case SERP_NUM_SB:
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
		case SERP_PARITY:
							switch(arg)
							{
								case NONEPAR:	lcr = lcr & ~UART_LCR_PARITY; 
												outb(lcr,UART+UART_LCR); 
												return 0;
								case ODDPAR:	lcr = lcr & ~UART_LCR_SPAR & ~UART_LCR_EPAR;
												lcr = lcr | UART_LCR_PARITY;
												outb(lcr,UART+UART_LCR); 
												return 0;
								case EVENPAR:	lcr = (lcr & ~UART_LCR_SPAR) | UART_LCR_EPAR | UART_LCR_PARITY; 
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
		case SERP_BAUD:
							if(arg > 65536 - 1 || arg == 0) return -EINVAL; // O divisor será no máximo 2^16 - 1, visto que se trata de 2 bytes
							lcr = lcr | UART_LCR_DLAB;
							outb(lcr,UART+UART_LCR); // DLAB = 1;							
							dlm = (unsigned char) (arg >> 0x08);
							dll = (unsigned char) (arg & 0xFF);
							outb(dlm,UART+UART_DLM);
							outb(dll,UART+UART_DLL);
							lcr = lcr & ~UART_LCR_DLAB;
							outb(lcr,UART+UART_LCR); // DLAB = 0;
							return 0;
		case SERP_SBC:
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
		case SERP_GETLCR:	
							return (unsigned char) lcr & ~UART_LCR_DLAB;
		case SERP_GETBAUD:	
							lcr = lcr | UART_LCR_DLAB;
							outb(lcr,UART+UART_LCR); // DLAB = 1;
							baud = inb(UART+UART_DLM);
							baud = baud << 0x08;
							baud = baud | inb(UART+UART_DLL);
							lcr = lcr & ~UART_LCR_DLAB;
							outb(lcr,UART+UART_LCR); // DLAB = 0;
							return baud;
							
		default:
							return -ENOTTY;
	}
}

int serp_open(struct inode * ind,struct file * f)
{
	struct sdev *dev;
	dev = container_of(ind->i_cdev, struct sdev, cdev);
	f->private_data = dev;
	nonseekable_open(ind, f);
	//printk(KERN_ALERT "Quero abrir o ficheiro! pointer: %d\nUID: %d\nEUID: %d\n",dev->minor,current->uid,current->euid);
	
	spin_lock(&(dev->serp_lock));
	if (dev->serp_using_count && (dev->serp_owner != current->uid) && (dev->serp_owner != current->euid) && !capable(CAP_DAC_OVERRIDE))
	{
		spin_unlock(&(dev->serp_lock));
		return -EBUSY;
	}
	
	if (dev->serp_using_count == 0)
		dev->serp_owner = current->uid;
		
	dev->serp_using_count++;
	spin_unlock(&(dev->serp_lock));
	//printk(KERN_ALERT "Abri o ficheiro! pointer: %d\nUID: %d\nEUID: %d\n",dev->minor,current->uid,current->euid);
	
	
	return 0;
}

int serp_release(struct inode * ind,struct file * f)
{
	struct sdev *dev = f->private_data;
	spin_lock(&(dev->serp_lock));
	dev->serp_using_count--; /* nothing else */
	spin_unlock(&(dev->serp_lock));
	//printk(KERN_ALERT "Fechei o ficheiro!\n");
	return 0;
}

ssize_t serp_read(struct file *filep, char __user *buff, size_t count, loff_t *offp)
{
	int lidos=0;
	int wait=0;
	char *kbuf;
	int lsr;
	struct sdev *dev = filep->private_data;
	//printk(KERN_ALERT "VOU ENTRAR NO MALLOC\n");
	
	if(filep->f_flags & O_NONBLOCK)	
	{
		if(down_trylock(&(dev->readlock))) return -EAGAIN;
	}
	else 
	{
		if(down_interruptible(&(dev->readlock))) return -ERESTARTSYS;
	}
	
	if((kbuf = (char*) kmalloc( sizeof(char) * count,GFP_KERNEL)) == NULL) 
	{
		//printk(KERN_ALERT "ERRO NO MALLOC\n");
		up(&(dev->readlock));
		return -ENOMEM;
	}
	//printk(KERN_ALERT "PASSEI O MALLOC\n");
	while(lidos < count)
	{
		lsr = inb(UART+UART_LSR);
		if(!(UART_LSR_DR & lsr))
		{
			if((filep->f_flags & O_NONBLOCK) && lidos == 0)
			{
				kfree(kbuf);
				up(&(dev->readlock));
				return -EAGAIN; // não bloquear em caso de O_NONBLOCK
			}
			else if(lidos > 0 && wait < READTIMEOUT) // faz READTIMEOUT esperas de READWAITMS para garantir que recebe bursts inteiros
			{
				wait++;
			}
			else if(lidos > 0)
			{
				kfree(kbuf);
				up(&(dev->readlock));
				return lidos;
			}
			//printk(KERN_ALERT "READ: Estou aqui\n");
			
			if(msleep_interruptible(READWAITMS)) 
			{
				kfree(kbuf);
				up(&(dev->readlock));
				return lidos; // Caso tenha sido acordado por interrupção
			}
		}
		else if (UART_LSR_OE & lsr) 
		{
			//printk(KERN_ALERT "ERRO READ\n");
			lidos = -EIO;
		}
		else if (UART_LSR_DR & lsr)
		{
			kbuf[lidos] = inb(UART+UART_RX);
			if(copy_to_user(buff+lidos,kbuf+lidos,1))
			{
				kfree(kbuf);
				up(&(dev->readlock));
				return lidos;
			}
			lidos++;
			wait = 0;
		}
	}
	kfree(kbuf);
	up(&(dev->readlock));
	return lidos; // retorna 0 em caso de não ter lido nada
}

ssize_t serp_write(struct file *filep, const char __user *buff, size_t count, loff_t *offp)
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
		if(down_interruptible(&(dev->writelock))) return -ERESTARTSYS;
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
		while( ! ( inb(UART+UART_LSR) & UART_LSR_THRE ) )
		{
			if(filep->f_flags & O_NONBLOCK)
			{
				kfree(kbuf);
				up(&(dev->writelock));
				if(i == 0) return i*FIFOSIZE;
				else return -EAGAIN; // não bloquear em caso de O_NONBLOCK
			}
			schedule(); // Esperar até poder escrever caracter
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
			if(i > 0) return i*FIFOSIZE;
			else return -EAGAIN; // não bloquear em caso de O_NONBLOCK
		}
		schedule(); // Esperar até poder escrever caracter
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


module_init(serp_init);
module_exit(serp_exit);
MODULE_LICENSE("Dual BSD/GPL");
