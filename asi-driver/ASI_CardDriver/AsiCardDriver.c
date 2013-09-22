//#include <linux/config.h>
#include <linux/version.h>      /*LINUX_VERSION_CODE*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
//#include <linux/device.h>
//#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <asm/atomic.h>

#define DEBUG_TEST     0

#define DCSR           0
#define DDMACR         1
#define WDMATLPA       2
#define WDMATLPS       3
#define WDMATLPC       4
#define WDMATLPP       5
#define RDMATLPP       6
#define RDMATLPA       7
#define RDMATLPS       8
#define RDMATLPC       9
#define WDMAPERF       10
#define RDMAPERF       11
#define RDMASTAT       12
#define NRDCOMP        13
#define RCOMPDSIZW     14
#define DLWSTAT        15
#define DLTRSSTAT      16
#define DMISCCONT      17
#define CHAN_RW        20
#define W_DMA_TLPA_END 21
#define W_DMA_RADD     22
#define W_DMA_STATUS   23
#define W_DMA_WADD     24


struct RegBuf{
	u32 _reg_index;
	u32 _len;
	u32 _reg_buf[254];
};

static struct pci_device_id ids[] = {
	{ PCI_DEVICE(0x10ee, 0x0007), },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, ids);

struct PlayLoad{
	u32 chan_num;
	u8 ts_data[188];
};


#define PLAY_LOAD_SIZE (sizeof(struct PlayLoad))
#define DMATLPS_VALUE 16

static int DevNo = 0;
static struct class*        _class = NULL;

#define ASI_REGISTER_SIZE 4*4
static char driver_name[] = "novel_asi_card";
struct AsiCard{
	int                  No;                        //设备编号
	struct pci_dev*      dev;
	unsigned long        Bar0HwAddr;                // Base register address (Hardware address)
	unsigned long        Bar0Len;                   // Base register address Length
	unsigned long        Bar0VirtAddr;              // 映射到内存的虚拟io地址	
	struct cdev          cdev;
	bool                 IsOpen;
	struct mutex         ops_lock;
	////////////////////////////////////////////////////////////
	//设备超时定时器
	#define TIMER_OUT_TIME 500
	struct timer_list    Timer;
	#define TIMER_COUNT_OUT 
	atomic_t             TimerCount;
	#define DMA_TRANFER_PLAY_LOAD_NUM ((PLAY_LOAD_SIZE/(DMATLPS_VALUE*4))*512)//大于64中断只响应两次
	#define DMA_BUF_SIZE (PLAY_LOAD_SIZE*1024)//小于5只发生中断，不更新写入地址
	struct semaphore     PollSem;
	unsigned long        PollInFlags;
	wait_queue_head_t    PollIn;
	unsigned long        PollOutFlags;
	wait_queue_head_t    PollOut;

	struct semaphore     ReadSem;
	unsigned char*       ReadDmaBuf;                // Pointer to dword aligned DMA buffer.
	dma_addr_t           ReadDmaBufHWAddr;
	//spinlock_t           ReadLock;

	struct semaphore     WriteSem;
	unsigned char*       WriteDmaBuf;               // Pointer to dword aligned DMA buffer.
	dma_addr_t           WriteDmaBufHWAddr;      

};


#define ASI_CARD_MAJOR 0   /* dynamic major by default */
#define ASI_CARD_DEVS  1024    /* asi0 through asi1024 */

static int asi_card_major =   ASI_CARD_MAJOR;
static int asi_card_devs =    ASI_CARD_DEVS;	/* number of bare scullc devices */

u32 asi_card_ReadReg (struct AsiCard *card, u32 dw_offset)
{
        u32 ret = 0;
        ret = readl((void*)(card->Bar0VirtAddr + (4 * dw_offset)));  
        return ret; 
}

void asi_card_WriteReg (struct AsiCard *card,u32 dw_offset, u32 val)
{
        writel(val, (void*)(card->Bar0VirtAddr + (4 * dw_offset)));
}

void InitCard(struct AsiCard *card)
{

	asi_card_WriteReg(card,DCSR, 1);      // Write: DCSR (offset 0) with value of 1 (Reset Device)
	asi_card_WriteReg(card,DCSR, 0);      // Write: DCSR (offset 0) with value of 1 (Reset Device)

	//asi_card_WriteReg(card,CHAN_RW,0x0000000f);   //初始化四个ASI接口为接收数据

	//初始化读dma
	asi_card_WriteReg(card,WDMATLPA, 
			card->ReadDmaBufHWAddr);            // Write:写dma起始地址
	asi_card_WriteReg(card,W_DMA_TLPA_END, 
			(card->ReadDmaBufHWAddr+DMA_BUF_SIZE)); // Write: 写dma结束地址
	asi_card_WriteReg(card,WDMATLPS,DMATLPS_VALUE);            // Write: 写DMA TLP 包长度
	asi_card_WriteReg(card,WDMATLPC, DMA_TRANFER_PLAY_LOAD_NUM);// Write: 写一次读取的DMA TLP 包个数
	
	asi_card_WriteReg(card,W_DMA_RADD,card->ReadDmaBufHWAddr);//写入dma缓冲的pc写入地址 
	asi_card_WriteReg(card,W_DMA_WADD,card->ReadDmaBufHWAddr);//写入dma缓冲的设备读取地址

	
	asi_card_WriteReg(card,WDMATLPP, 0x00000000);   // Write: Write DMA TLP Pattern register with default value (0x0)

	/////////////////////////////////////////////////////////////
	//初始化写dima
	asi_card_WriteReg(card,RDMATLPP, 0xfeedbeef);// Write: Read DMA Expected Data Pattern with default value (feedbeef)
	asi_card_WriteReg(card,RDMATLPA, 
				card->WriteDmaBufHWAddr);//Write: Read DMA TLP Address register with starting address.
	asi_card_WriteReg(card,RDMATLPS, DMATLPS_VALUE);//Write: Read DMA TLP Size register with default value (32dwords)

	asi_card_WriteReg(card,RDMATLPC, DMA_TRANFER_PLAY_LOAD_NUM);// Write: Read DMA TLP Count register with default value (2000)
  	
	//////////////////////////////////////////////
	//debug test
	#if ( DEBUG_TEST == 1 ) 
	card->ReadDmaBufHWAddr = pci_map_single(card->dev,card->ReadDmaBuf,DMA_BUF_SIZE,DMA_FROM_DEVICE);
	if ( dma_mapping_error(card->dev,card->ReadDmaBufHWAddr) ) {
		printk("pci_map_single error\n");
		return ;
	}
	asi_card_WriteReg(card,W_DMA_RADD,card->ReadDmaBufHWAddr);//写入dma缓冲的pc写入地址 
	asi_card_WriteReg(card,W_DMA_WADD,card->ReadDmaBufHWAddr);//写入dma缓冲的设备读取地址
	#endif
	////////////////////////////////////////////// 
	/*ndelay(10);//延时10纳秒
	udelay(10);//延时10微妙
	mdelay(10);//延时10毫秒 */
  
	/*asi_card_WriteReg(card,DCSR, 1);      // Write: DCSR (offset 0) with value of 1 (Reset Device)
	asi_card_WriteReg(card,DCSR, 0);      // Write: DCSR (offset 0) with value of 1 (Reset Device)
	asi_card_WriteReg(card,DDMACR, 0x00000001);          // Write: 开启写内存模式
	asi_card_WriteReg(card,DDMACR, 0x00010000);          // Write: 开启读内存模式*/
	//等待dma写中断发生
	printk("card->ReadDmaBufHWAddr = %llx\n",card->ReadDmaBufHWAddr);
	printk("card->ReadDmaBuf = %p\n",card->ReadDmaBuf);
	printk("W_DMA_RADD = %08x\n",asi_card_ReadReg(card,W_DMA_RADD));//读取dma缓冲的pc读取地址 
	printk("W_DMA_WADD = %08x\n",asi_card_ReadReg(card,W_DMA_WADD));//读取dma缓冲的pc读取地址 
	
  
	
}

void TerminalCard( struct AsiCard *card )
{
	asi_card_WriteReg(card,DDMACR, 0);            // Write: DDMACR (offset 1) with value of 0  (使能DMA控制器)
	asi_card_WriteReg(card,DCSR, 0);      // Write: DCSR (offset 0) with value of 1 (Reset Device)
	
}
void ResetCard(struct AsiCard *card)
{
	TerminalCard(card);
	InitCard(card);
	asi_card_WriteReg(card,0, 1);                // Write: DCSR (offset 0) with value of 1 (Reset Device)
	asi_card_WriteReg(card,0, 0);                // Write: DCSR (offset 0) with value of 0 (Make Active)
	asi_card_WriteReg(card,DDMACR, 0x00000001);          // Write: 开启写内存模式
	//asi_card_WriteReg(card,DDMACR, 0x00010000);          // Write: 开启读内存模式 ,使用会导致W_DMA_RADD 和 W_DMA_WADD 读出为FFFFFFFF
	ndelay(10);//延时10纳秒
	udelay(10);//延时10微妙
	mdelay(10);//延时10毫秒
}


#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
static irqreturn_t  AsiCard_IRQHandler (int irq, void *dev_id, struct pt_regs *regs)
#else
static irqreturn_t  AsiCard_IRQHandler (int irq, void *dev_id)
#endif
{
	unsigned char *k,*ReadAddStartHW,*ReadAddEndHW;
	unsigned char *WriteAddStartHW,*WriteAddEndHW;
	u64 i;
	struct PlayLoad* pak;
	struct AsiCard *card = dev_id;
	//printk("AsiCard_IRQHandler irq = %d\n",irq);
	atomic_inc(&card->TimerCount);
	/*spin_lock(&card->ReadLock);
	spin_unlock(&card->ReadLock);*/

	//////////////////////////////////////////////////
	//debug test
	#if (DEBUG_TEST == 1)
	dma_unmap_single(card->dev,card->ReadDmaBufHWAddr,DMA_BUF_SIZE,DMA_FROM_DEVICE);
	for(i = 0 ; i < DMA_BUF_SIZE ; i++){
		k = card->ReadDmaBuf + i;
		/*if( *k == 0x47 ){
			printk("\nfind ts header\n");
		}*/
		if( (i % 192) == 0 ){
			printk("\n");
		}
		printk("%02x",*k);
	}
	printk("\n");
	asi_card_WriteReg(card,DDMACR, 0x00000000);          // Write: 开启写内存模式
	return IRQ_HANDLED;
	#endif
	//////////////////////////////////////////////////

	/*printk("card->ReadDmaBufHWAddr = %llx\n",(unsigned long long)card->ReadDmaBufHWAddr);
	printk("card->ReadDmaBuf = %p\n",card->ReadDmaBuf);
	ReadAddStartHW = (unsigned char*)(unsigned long long)asi_card_ReadReg(card,W_DMA_RADD);//读取dma缓冲的pc读取地址 
	ReadAddEndHW = (unsigned char*)(unsigned long long)asi_card_ReadReg(card,W_DMA_WADD);//读取dma缓冲的设备写入地址
	printk("ReadAddStartHW = %p\,",ReadAddStartHW);
	printk("ReadAddEndHW = %p\n",ReadAddEndHW);
	pak = (struct PlayLoad*)__va(ReadAddStartHW);
	printk("pak = %p\n",pak);
	printk("chan_num = %d\n",pak->chan_num);
	printk("ts_data[0] = %02x\n",pak->ts_data[0]);
	for(i = 0 ; i < DMA_BUF_SIZE ; i++){
		k = card->ReadDmaBuf + i;
		if( *k == 0x47 ){
			//printk("find ts header\n");
		}
		else{
			//printk("%02x",*k);
		}
	}
	printk("\n");*/
	ReadAddStartHW = (unsigned char*)(unsigned long long)asi_card_ReadReg(card,W_DMA_RADD);//读取dma缓冲的pc读取地址 
	ReadAddEndHW = (unsigned char*)(unsigned long long)asi_card_ReadReg(card,W_DMA_WADD);//读取dma缓冲的设备写入地址
	/*printk("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n");
	printk("ReadDmaBuf = %p,",card->ReadDmaBuf);
	printk("ReadDmaBuf End = %p\n",card->ReadDmaBuf+DMA_BUF_SIZE);
	printk("ReadAddStartHW = %p,",ReadAddStartHW);
	printk("ReadAddEndHW = %p\n",ReadAddEndHW);*/
	if(ReadAddEndHW > ReadAddStartHW){
		i = ReadAddEndHW - ReadAddStartHW;
	}
	else{
		i = ((u64)ReadAddEndHW - (u64)card->ReadDmaBufHWAddr) + 
			( (u64)card->ReadDmaBufHWAddr + DMA_BUF_SIZE - (u64)ReadAddStartHW);
	}
	i = i % PLAY_LOAD_SIZE;
	if( i != 0 ){
		printk("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n");
		printk("ReadDmaBuf = %p,",card->ReadDmaBuf);
		printk("ReadDmaBuf End = %p\n",card->ReadDmaBuf+DMA_BUF_SIZE);
		printk("ReadAddStartHW = %p,",ReadAddStartHW);
		printk("ReadAddEndHW = %p\n",ReadAddEndHW);
		printk("error != %lld\n",i);
	}
	////////////////////////////////////////////////////////////
	//如果发现读非空或者写缓冲非满那么调用以下两个函数
	//如果写缓冲空，调用下面函数
	if ( ReadAddStartHW != ReadAddEndHW ){
		card->PollInFlags = 1;
		wake_up_interruptible(&card->PollIn);
		//asi_card_WriteReg(card,W_DMA_RADD,ReadAddEndHW);
		//printk("Read Enable\n");
	}
	//如果读缓冲有数据调用下面函数
	WriteAddStartHW = asi_card_ReadReg(card,W_DMA_RADD);
	WriteAddEndHW   = asi_card_ReadReg(card,W_DMA_WADD);
	if( WriteAddStartHW != WriteAddEndHW ){
		card->PollOutFlags = 1;
		wake_up_interruptible(&card->PollOut);
		//printk("Write Enable\n");
	}

	return IRQ_HANDLED;
}


static ssize_t asi_card_read (struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	struct AsiCard *card = NULL;
	ssize_t ret,ret1,ret2;
	unsigned char	*ReadAddStart ,*ReadAddStartHW;
	unsigned char   *ReadAddEnd ,*ReadAddEndHW;
	struct PlayLoad *s;
	card = file->private_data;

	if( count < PLAY_LOAD_SIZE ){
		return -ERESTARTSYS;
	}
	if ( (count % PLAY_LOAD_SIZE) != 0 )  {
		printk("%s: asi_card_read: Buffer length not dword aligned.\n",driver_name);
		return -ERESTARTSYS;
	}

	if(down_interruptible(&card->ReadSem)){
		return -ERESTARTSYS;
	}

	////////////////////////////////////////////////////////////////
	/*spin_lock_irqsave(&card->ReadLock,flags);
	spin_unlock_irqrestore(&card->ReadLock,flags);*/

	/*printk("read begin =============\n");
	printk("card->ReadDmaBufHWAddr = %llx\n",card->ReadDmaBufHWAddr);
	printk("card->ReadDmaBuf = %p\n",card->ReadDmaBuf);
	printk("ReadDmaBuf End = %p\n",card->ReadDmaBuf+DMA_BUF_SIZE);*/
	ReadAddStartHW = (unsigned char*)(unsigned long long)asi_card_ReadReg(card,W_DMA_RADD);//读取dma缓冲的pc读取地址 
	ReadAddEndHW = (unsigned char*)(unsigned long long)asi_card_ReadReg(card,W_DMA_WADD);//读取dma缓冲的设备写入地址
	/*printk("1ReadAddStartHW = %p,",ReadAddStartHW);
	printk("1ReadAddEndHW = %p\n",ReadAddEndHW);*/
	card->PollInFlags = !( ReadAddStartHW == ReadAddEndHW );
	//printk("card->PollInFlags = %ld\n",card->PollInFlags);
	wait_event_interruptible(card->PollIn,card->PollInFlags);
	ReadAddStartHW = (unsigned char*)(unsigned long long)asi_card_ReadReg(card,W_DMA_RADD);//读取dma缓冲的pc读取地址 
	ReadAddEndHW = (unsigned char*)(unsigned long long)asi_card_ReadReg(card,W_DMA_WADD);//读取dma缓冲的设备写入地址
	/*printk("2ReadAddStartHW = %p,",ReadAddStartHW);
	printk("2ReadAddEndHW = %p\n",ReadAddEndHW);*/
	ReadAddStart = __va(ReadAddStartHW);
	ReadAddEnd = __va(ReadAddEndHW);
	/*printk("3ReadAddStart = %px,",ReadAddStart);
	printk("3ReadAddEnd = %p\n",ReadAddEnd);*/
	/*s = (struct PlayLoad*)ReadAddStart;
	printk("%02x\n",s->chan_num);
	printk("%02x\n",s->ts_data[0]);*/
	ret = 0;
	if( ReadAddEnd >= ReadAddStart ){
		ret = ReadAddEnd - ReadAddStart;
		ret = ret - (ret % PLAY_LOAD_SIZE);
		if( ret < PLAY_LOAD_SIZE ){
			printk("wwwwww : %px,",ReadAddStart);
			printk("%p\n",ReadAddEnd);
			up(&card->ReadSem);
			return 0;
		}
		//printk("ret = %lld\n",(unsigned long long)ret);
		if( ret >= count ){
			ret = count;
		}
		if( copy_to_user(buf,ReadAddStart,ret) ){
			printk("1%s: asi_card_read: Failed copy to user.\n",driver_name);
			up(&card->ReadSem);
			return -ERESTARTSYS;
		}	
		//操作asi卡寄存器，告诉asi卡有数据被读取
		printk("write 1ADDHW = %llx\n",ReadAddStartHW+ret);
		asi_card_WriteReg(card,W_DMA_RADD,ReadAddStartHW+ret);
	}
	else{
		ret1 = card->ReadDmaBuf+DMA_BUF_SIZE - ReadAddStart;
		ret2 = ReadAddEnd - card->ReadDmaBuf;
		ret = ret2 + ret1;
		ret = ret - (ret % PLAY_LOAD_SIZE);
		if( ret < PLAY_LOAD_SIZE ){
			printk("eeeeee : %px,",ReadAddStart);
			printk("%p\n",ReadAddEnd);
			up(&card->ReadSem);
			return 0;
		}
		printk("ret = %lld\n",(unsigned long long)ret);
		if( ret1 >= count ){
			ret = count;
			if( copy_to_user(buf,ReadAddStart,ret ) ){
				printk("2%s: asi_card_read: Failed copy to user.\n",driver_name);
				up(&card->ReadSem);
				return -ERESTARTSYS;
			}
			//操作asi卡寄存器，告诉asi卡有数据被读取
			printk("write 2ADDHW = %llX\n",ReadAddStartHW+ret);
			asi_card_WriteReg(card,W_DMA_RADD,ReadAddStartHW+ret);
		}
		else{
			if( ret >= count ){
				ret = count;
			}
			if( copy_to_user(buf,ReadAddStart,ret1 ) ){
				printk("3%s: asi_card_read: Failed copy to user.\n",driver_name);
				up(&card->ReadSem);
				return -ERESTARTSYS;
			}
			if( copy_to_user(buf+ret1,card->ReadDmaBuf,ret - ret1 ) ){
				printk("4%s: asi_card_read: Failed copy to user.\n",driver_name);
				up(&card->ReadSem);
				return -ERESTARTSYS;
			}
			//操作asi卡寄存器，告诉asi卡有数据被读取
			printk("write 3ADDHW = %llX\n",card->ReadDmaBufHWAddr+(ret - ret1));
			asi_card_WriteReg(card,W_DMA_RADD,card->ReadDmaBufHWAddr+(ret - ret1));
		}	
	}
	ReadAddStartHW = (unsigned char*)(u64)asi_card_ReadReg(card,W_DMA_RADD);//读取dma缓冲的pc读取地址 
	ReadAddEndHW = (unsigned char*)(u64)asi_card_ReadReg(card,W_DMA_WADD);//读取dma缓冲的设备写入地址
	/*printk("4ReadAddStartHW = %p,",ReadAddStartHW);
	printk("4ReadAddEndHW = %p\n",ReadAddEndHW);*/
	
	/////////////////////////////////////////////////////////////////

	up(&card->ReadSem);

	return ret;
}

static ssize_t asi_card_write (struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	struct AsiCard *card = NULL;
	unsigned char*    WriteAddStart;
	unsigned char*    WriteAddEnd;
	int ret = 0;
	card = file->private_data;

	if ( (count % 4) != 0 )  {
		printk("%s: asi_card_write: Buffer length not dword aligned.\n",driver_name);
		ret = -1;
		return -ERESTARTSYS;
	}

	if(down_interruptible(&card->WriteSem)){
		return -ERESTARTSYS;
	}
	
	WriteAddStart = (unsigned char*)(unsigned long long)asi_card_ReadReg(card,W_DMA_RADD);//写入dma缓冲的pc写入地址 
	WriteAddEnd = (unsigned char*)(unsigned long long)asi_card_ReadReg(card,W_DMA_WADD);//写入dma缓冲的设备读取地址
	card->PollOutFlags = !(WriteAddStart == WriteAddEnd);
	printk("WriteAddStart = %p",WriteAddStart);
	printk("WriteAddEnd = %p",WriteAddEnd);
	printk("flags = %ld\n",card->PollOutFlags);
	wait_event_interruptible(card->PollOut,card->PollOutFlags);
	WriteAddStart = __va(asi_card_ReadReg(card,W_DMA_RADD));//写入dma缓冲的pc写入地址 
	WriteAddEnd = __va(asi_card_ReadReg(card,W_DMA_WADD));//写入dma缓冲的设备读取地址
	// Now it is safe to copy the data from user space.
	if ( copy_from_user(card->WriteDmaBuf, buf, count) )  {
		ret = -1;
		printk("%s: asi_card_write: Failed copy to user.\n",driver_name);
		up(&card->WriteSem);
		return -ERESTARTSYS;
	}
	////////////////////////////////////////////////////////////////
	//操作asi卡寄存器，告诉卡有数据写入


                                       
	////////////////////////////////////////////////////////////////
                                         
	up(&card->WriteSem);

	return (ret);
}

static int asi_card_open (struct inode *inode, struct file *file)
{
	struct AsiCard* card;
	/*  Find the device */
	printk(KERN_WARNING"asi_card_open\n");
	card = container_of(inode->i_cdev, struct AsiCard, cdev);
	mutex_lock(&card->ops_lock);
	
	if( card->IsOpen == true ){
		mutex_unlock(&card->ops_lock);
		printk("open error\n");
		return -EIO;
	}
	card->IsOpen = true;
	file->private_data = card;
	
	ResetCard(card);

	////////////////////////////////////////
	//	
	///////////////////////////////////////
	mutex_unlock(&card->ops_lock);
	return 0;
}

static int asi_card_release (struct inode *inode, struct file *file)
{
	struct AsiCard* card;
	/*  Find the device */
	printk(KERN_WARNING"asi_card_close\n");
	card = container_of(inode->i_cdev, struct AsiCard, cdev);
	mutex_lock(&card->ops_lock);
	
	if( card->IsOpen == false ){
		mutex_unlock(&card->ops_lock);
		printk("close error\n");
		return -EIO;
	}
	card->IsOpen = false;
	file->private_data = NULL;

	///////////////////////////////////////////////////
	TerminalCard(card);

	///////////////////////////////////////////////////
	
	mutex_unlock(&card->ops_lock);
	return 0;
}

unsigned int asi_card_poll(struct file *file, struct poll_table_struct *wait)
{
	unsigned int mask = 0 ;
	struct AsiCard* card;
	card = file->private_data;
	if(down_interruptible(&card->PollSem)){
		return -ERESTARTSYS;
	}
	poll_wait(file,&card->PollIn,wait);
	poll_wait(file,&card->PollOut,wait);
	mask |= POLLIN|POLLRDNORM;//可读取
	mask |= POLLOUT|POLLWRNORM;//可写入
	up(&card->PollSem);
	
	return mask;
}

static long asi_card_compat_ioctl (struct file *file, unsigned int cmd, unsigned long arg)
{
	struct AsiCard* card;
	u32 i;
	struct RegBuf kregbuf;
	struct RegBuf *uregbuf = (struct RegBuf*)arg;
	card = file->private_data;
	printk("ioctl\n");
	if ( copy_from_user(&kregbuf, uregbuf, sizeof(struct RegBuf)) )  {
		printk("RegBuf Failed copy to user.\n");
		return -ERESTARTSYS;
	}
	printk("cmd = %d\n",cmd);
	printk("reg index = %d\n",kregbuf._reg_index);
	printk("reg len = %d\n",kregbuf._len);
	printk("reglist :\n");

	switch(cmd){
	case 3:	
		break;
	case 4:
		for(i = 0 ; i < kregbuf._len ;i++){
			asi_card_WriteReg(card,i+kregbuf._reg_index,
					cpu_to_le32(kregbuf._reg_buf[i+kregbuf._reg_index]));
			printk("w %08x\n",cpu_to_le32(kregbuf._reg_buf[i+kregbuf._reg_index]));
		}
		break;
	case 5:
		printk("asi_card_WriteReg\n");
		asi_card_WriteReg(card,DDMACR, 0x00000001);
		return 0;
		break;
	default:
		break;
	}
	for(i = 0 ; i < kregbuf._len ;i++){
		kregbuf._reg_buf[i+kregbuf._reg_index] = asi_card_ReadReg(card,i+kregbuf._reg_index);
		printk("%08x\n",le32_to_cpu(kregbuf._reg_buf[i+kregbuf._reg_index]));
	}
	copy_to_user(uregbuf,&kregbuf,sizeof(struct RegBuf));
	/*asi_card_WriteReg(card,2,0xffffffff);
	printk("reg = %08x\n",asi_card_ReadReg(card,2));*/
	return 0;
}

static struct file_operations AsiCard_fops = {
	owner:        THIS_MODULE,
	read :        asi_card_read,
	write:        asi_card_write,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36))
	ioctl: asi_card_compat_ioctl,
#else
	compat_ioctl: asi_card_compat_ioctl,
	unlocked_ioctl:asi_card_compat_ioctl,
#endif
	poll:         asi_card_poll,
	open:         asi_card_open,
	release:      asi_card_release,
};


static unsigned char skel_get_revision(struct pci_dev *dev)
{
	u8 revision;

	pci_read_config_byte(dev, PCI_REVISION_ID, &revision);
	return revision;
}


void TimerOut(unsigned long data){
	struct AsiCard* card = (struct AsiCard*)data;
	int count = atomic_read(&card->TimerCount);

	/*printk("TimerOut\n");
	if( count < TIMER_COUNT_OUT ){
		InitiatorReset(card);
		InitCard(card);
	}*/

	atomic_set(&card->TimerCount,0);
	card->Timer.expires += TIMER_OUT_TIME;
	add_timer(&card->Timer);
}

static int probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	struct AsiCard* card = NULL;
	u8 irq = 0;
	int result = 0;
	printk("probe begin\n");

	printk("%p\n",dev);
	card = kmalloc(sizeof(struct AsiCard), GFP_KERNEL);
	if (dev == NULL) {
		printk("Out of memory");
		return -ENODEV;
	}
	memset(card,0,sizeof(struct AsiCard));
	card->No = DevNo++;
	card->dev = dev;
	pci_set_drvdata(dev,card);	


	if (skel_get_revision(dev) == 0x42)
		goto probe_error;
	///////////////////////////////////
	// Do probing type stuff here.  
	// Like calling request_region();
	///////////////////////////////////
	pci_enable_device(dev);
	//为设备启用总线控制 
	pci_set_master(card->dev);

	////////////////////////////////////////////////////////////////////
	//获得io寄存器	
	// Get Base Address of registers from pci structure. Should come from pci_dev
	// structure, but that element seems to be missing on the development system.
	//测试设备内存是否支持内存映射模式
	if (!(pci_resource_flags(card->dev, 0) & IORESOURCE_MEM)) {
		printk(KERN_ERR "foo: Incorrect BAR configuration.\n");
		goto probe_error;
	}

  	card->Bar0HwAddr = pci_resource_start (card->dev, 0);

	if (0 > card->Bar0HwAddr) {
		printk(KERN_WARNING"No.%d: Init: Base Address not set.\n", card->No);
		goto probe_error;
	} 

	// Print Base Address to kernel log
	printk(KERN_INFO"No.%d: Init: Base hw val %X\n", card->No, (unsigned int)card->Bar0HwAddr);

	// Get the Base Address Length
	card->Bar0Len = pci_resource_len (card->dev, 0);
	printk(KERN_INFO"Bar0Len = %ld\n",card->Bar0Len);


	////////////////////////////////////////////////////////////////////
	//请求iomem
	// Check the memory region to see if it is in use
	if (0 > check_mem_region(card->Bar0HwAddr, ASI_REGISTER_SIZE)) {
		printk(KERN_WARNING"No.%d: Init: Memory in use.\n", card->No);
		goto probe_error;
	}

	// Try to gain exclusive control of memory for demo hardware.
  	request_mem_region(card->Bar0HwAddr, ASI_REGISTER_SIZE, "ASI_iomem0");

  	printk(KERN_INFO"No.%d: Init: Initialize Hardware Done..\n",card->No);

	///////////////////////////////////////////////////////////////////
	//映射io寄存器到内存
	// Remap the I/O register block so that it can be safely accessed.
	// I/O register block starts at gBaseHdwr and is 32 bytes long.
	// It is cast to char because that is the way Linus does it.
	// Reference "/usr/src/Linux-2.4/Documentation/IO-mapping.txt".
	card->Bar0VirtAddr = (unsigned long)ioremap(card->Bar0HwAddr, card->Bar0Len);
	if (!card->Bar0VirtAddr) {
		printk(KERN_WARNING"No.%d: Init: Could not remap memory.\n", card->No);
		goto probe_error;
	} 
	// Print out the aquired virtual base addresss
	printk(KERN_INFO"No.%d: Init: Virt HW address %X\n", card->No, (unsigned int)card->Bar0VirtAddr);

	//////////////////////////////////////////////////////////////////
	//初始化硬件
	//////////////////////////////////////////////////////////////////////
	if (!pci_set_dma_mask(card->dev, DMA_BIT_MASK(32))){
		//privdata->dma_using_dac = 1;
		printk("set dma mask 32\n");
	}
	else if (!pci_set_dma_mask(card->dev, DMA_BIT_MASK(64))){
		//privdata->dma_using_dac = 0;
		printk("set dma mask 64\n");
	}
	else {
		printk(KERN_ERR "foo: Failed to set DMA mask.\n");
		goto probe_error;
	}
	/*if (!pci_set_consistent_dma_mask(card->dev, DMA_BIT_MASK(32))){
		//privdata->dma_using_dac = 1;
		printk("set dma mask 32\n");
	}
	else if (!pci_set_consistent_dma_mask(card->dev, DMA_BIT_MASK(64))){
		//privdata->dma_using_dac = 0;
		printk("set dma mask 64\n");
	}
	else {
		printk(KERN_ERR "foo: Failed to set DMA mask.\n");
		goto probe_error;
	}*/
	//////////////////////////////////////////////////////////////////////
	//分配dmabufer
	//////////////////////////////////////////////////////////////////////
	mutex_init(&card->ops_lock);

	sema_init(&card->PollSem,1);
	init_waitqueue_head(&card->PollIn);
	init_waitqueue_head(&card->PollOut);

	sema_init(&card->ReadSem,1);
	sema_init(&card->WriteSem,1);
	card->ReadDmaBuf = pci_alloc_consistent(card->dev, DMA_BUF_SIZE, &card->ReadDmaBufHWAddr);
	if (NULL == card->ReadDmaBuf ) {
		printk(KERN_CRIT"%s: Init: Unable to allocate gBuffer.\n",driver_name);
		goto probe_error;
	}
	// Print Read buffer size and address to kernel log
	printk(KERN_INFO"%s: Read Buffer Allocation: %llX->%llX\n", driver_name, 
			(unsigned long long)card->ReadDmaBuf, (unsigned long long)card->ReadDmaBufHWAddr);

	card->WriteDmaBuf = pci_alloc_consistent(card->dev, DMA_BUF_SIZE, &card->WriteDmaBufHWAddr);
	if (NULL == card->WriteDmaBuf) {
		printk(KERN_CRIT"%s: Init: Unable to allocate gBuffer.\n",driver_name);
		goto probe_alloc_writedmabuf_error;
	}
	// Print Write buffer size and address to kernel log  
	printk(KERN_INFO"%s: Write Buffer Allocation: %llX->%llX\n", driver_name, 
		(unsigned long long)card->WriteDmaBuf, (unsigned long long)card->WriteDmaBufHWAddr);




	////////////////////////////////////////////////////////////////////////////////////////
	// 从pci的0x3c(60)寄存器（中断线寄存器）读取中断号，这个中断号在电脑上电时有bios固件分配（详细看pci规范）
	result = pci_read_config_byte(card->dev,PCI_INTERRUPT_PIN,&irq);
	printk(KERN_INFO"PCI_INTERRUPT_PIN = %d\n", irq);

	printk(KERN_INFO"pci_dev.irq = %d\n", dev->irq);
	result = pci_read_config_byte(card->dev,PCI_INTERRUPT_LINE,&irq);
	printk(KERN_INFO"PCI_INTERRUPT_LINE = %d\n", irq);
	if( result ){
		//一般不应该出错，这里可以不用错误处理直接读取
		printk(KERN_INFO"No.%d: Init: Irq no can not find %X\n", card->No, (unsigned int)result);
		goto probe_enable_msi_error;
	}	
	// Request IRQ from OS.
	// In past architectures, the SHARED and SAMPLE_RANDOM flags were called: SA_SHIRQ and SA_SAMPLE_RANDOM
	// respectively.  In older Fedora core installations, the request arguments may need to be reverted back.
	// SA_SHIRQ | SA_SAMPLE_RANDOM
	//2.6.36后的你内核不再支持SA_SAMPLE_RANDOM选项
	// Disable global interrupts 


	// Set up a single MSI interrupt 
	//如果启用 MSI中断模式那么要先调用pci_enable_msi再调用request_irq注册终端函数
	if (pci_enable_msi(card->dev)) {
		printk(KERN_ERR "foo: Failed to enable MSI.\n");
		goto probe_enable_msi_error;
	}

	printk(KERN_INFO"No%d: ISR Setup..\n", card->No);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
	if (0 > request_irq(card->dev->irq, &AsiCard_IRQHandler, SA_INTERRUPT|SA_SHIRQ, driver_name, card)) {
#else
	if (0 > request_irq(card->dev->irq, &AsiCard_IRQHandler, IRQF_SHARED, driver_name, card)) {
#endif
		printk(KERN_WARNING"No.%d: Init: Unable to allocate IRQ",card->No);//IRQF_DISABLE
		goto probe_irq_request_error;
  	}
	printk(KERN_INFO"No%d: ISR Setup.....................................\n", card->No);



	////////////////////////////////////////////////////////////////////
	//建立硬件设备文件
	cdev_init(&card->cdev, &AsiCard_fops);
	card->cdev.owner = THIS_MODULE;
	card->cdev.ops = &AsiCard_fops;
	card->cdev.dev = MKDEV(asi_card_major, card->No);
	result = cdev_add(&card->cdev, MKDEV(asi_card_major, card->No) ,1);
	if (result < 0){
		printk(KERN_WARNING "Error %d adding asi cdev", result);
		goto probe_cdev_add_error;
	}
	
	//调用udevd动态建立设备文件（/dev下）
	device_create(_class,NULL, MKDEV(asi_card_major, card->No),NULL,"asi%d",card->No);

	
	//InitCard(card);

	////////////////////////////////////////////////////////////////////
	//初始化设备超时定时器
	atomic_set(&card->TimerCount,0);
	init_timer(&card->Timer);
	card->Timer.expires = jiffies + TIMER_OUT_TIME;
	card->Timer.data = (unsigned long)card;
	card->Timer.function = TimerOut;
	add_timer(&card->Timer);	
	printk(KERN_INFO "asd insmod complete\n");
	
	return 0;
probe_cdev_add_error:
	free_irq(card->dev->irq,card->dev);
probe_irq_request_error:
	pci_disable_msi(card->dev);
probe_enable_msi_error:
	pci_free_consistent(card->dev, DMA_BUF_SIZE,card->WriteDmaBuf , card->WriteDmaBufHWAddr);
probe_alloc_writedmabuf_error:
	pci_free_consistent(card->dev, DMA_BUF_SIZE, card->ReadDmaBuf, card->ReadDmaBufHWAddr);
probe_error:
	iounmap((void*)card->Bar0VirtAddr);
	
	release_mem_region(card->Bar0HwAddr, ASI_REGISTER_SIZE);
	
	pci_resource_end (card->dev, 0);


	pci_clear_master(card->dev);	
	pci_disable_device(card->dev);


	return -ENODEV;	
}

static void remove(struct pci_dev *dev)
{
	/* clean up any allocated resources and stuff here.
	 * like call release_region();
	 */
	struct AsiCard* card = NULL;
	printk("remove begin\n");

	card = pci_get_drvdata(dev);
	
	del_timer_sync(&card->Timer);

	TerminalCard( card );

	device_destroy(_class,MKDEV(asi_card_major, card->No));

	cdev_del(&card->cdev);


	free_irq(card->dev->irq,card);
	
	pci_disable_msi(card->dev);

	pci_free_consistent(card->dev, DMA_BUF_SIZE,card->WriteDmaBuf , card->WriteDmaBufHWAddr);
	pci_free_consistent(card->dev, DMA_BUF_SIZE, card->ReadDmaBuf, card->ReadDmaBufHWAddr);

	iounmap((void*)card->Bar0VirtAddr);

	release_mem_region(card->Bar0HwAddr, ASI_REGISTER_SIZE);

	pci_resource_end (card->dev, 0);

	pci_clear_master(card->dev);
	pci_disable_device(card->dev);
	/////////////////////////////
	kfree(card);
	
}

static struct pci_driver pci_driver = {
	.name = "NOVEL_ASI_CARD",
	.id_table = ids,
	.probe = probe,
	.remove = remove,
};

static int __init NovelAsiCard_init(void)
{
	///////////////////////////////////////
	int result = 0;
	dev_t dev_id = MKDEV(asi_card_major, 0);
	printk(KERN_NOTICE"Hello asi card \n");	
	/*
	 * Register your major, and accept a dynamic number.
	 * 分配设备号
	 */
	if (asi_card_major)
		result = register_chrdev_region(dev_id, asi_card_devs, "asi");
	else {
		result = alloc_chrdev_region(&dev_id, 0, asi_card_devs, "asi");
		asi_card_major = MAJOR(dev_id);
	}
	if (result < 0)
		return result;
	//建立/sysfs文件系统下相关文件
	_class = class_create(THIS_MODULE,driver_name);
	if( _class == NULL ){
		printk(KERN_WARNING"class_create error\n");
		unregister_chrdev_region(MKDEV(asi_card_major, 0), asi_card_devs);
		return 0;
	}
	//////////////////////////////////////////////////////////////
	return pci_register_driver(&pci_driver);
}

static void __exit NovelAsiCard_exit(void)
{
	pci_unregister_driver(&pci_driver);
	
	class_destroy(_class);

	unregister_chrdev_region(MKDEV(asi_card_major, 0), asi_card_devs);
}

module_init(NovelAsiCard_init);
module_exit(NovelAsiCard_exit);


MODULE_AUTHOR("DONGRUI, Novel SuperTv, Inc.");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Driver for the ASI PCI-PCIE Cards");

//./drivers/tty/serial/msm_serial_hs.c 1524行里典型的dma操作代码 
