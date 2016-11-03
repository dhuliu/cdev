
## 字符设备驱动

### 1. 驱动设备分类
Linux系统将设备分成三种基本类型：字符设备，块设备，网络设备。
这三种类型的设备分别对应三种模块：字符模块，块模块，网络模块。
字符设备是三类设备中最常见的设备，比如常见的键盘、鼠标、触摸屏等都属于字符设备。
驱动和系统关系：

![enter description here][1]
* 设备文件
linux基本思想：一切皆文件。

	设备文件：从文字上看就是设备的文件，这类文件就是专门给硬件设备服务的文件，而且这类文件都存在/dev目录下，每当我们向内核中加一个设备驱动程序，这时候就会在相应的/dev目录下生成相应的设备文件。总结起来：设备文件就是跟驱动程序对应起来的，是上层应用访问驱动的接口。用户调用open/read/write/seek等对抽象的文件进行操作就可以操作实际硬件设备（或抽象的设备）。
	
![enter description here][2]

其中，第一列 ==c== 代表字符设备。

### 2. 字符设备

#### 2-1 字符设备数据结构
==cdev==用来描述字符设备，定义如下：
```c
struct cdev {
	struct kobject kobj;
	struct module *owner;
	const struct file_operations *ops;
	struct list_head list;
	dev_t dev;
	unsigned int count;
};
```
#### 2-2 设备号
* 定义
dev_t定义了设备号，为32位，前12位用来表示主设备号，后20位表示次设备号。
* 获得设备号
使用下面的宏通过dev_t获取主设备号和次设备号：
	```c
	MAJOR(dev_t dev);
	MINOR(dev_t dev);
	```
	如果已经有主设备号和次设备号，将其转换成dev_t来下，使用下面的宏：
	```c
	MKDEV(int major, int minor);
	```
* 分配释放设备编号
	* 分配
	1). 已知主设备号时，使用下面的函数获得设备号：
	 int register_chrdev_region(dev_t first, unsigned int count, const char *name); 
	其中，first是要分配的设备编号范围的起始值。count是连续设备的编号的个数。
	name是和该设备编号范围关联的设备名称，将出现在/proc/devices和sysfs中。此函数成功返回0，失败返回负的错误码。
	2). 未知主设备号时，使用下面的函数获得设备号：
 int alloc_chrdev_region(dev_t *dev, unsigned int firstminor, unsigned int count, const char *name); 
dev用于输出申请到的设备编号，firstminor要使用的第一个此设备编号
	* 释放
	 当不再使用当前的设备编号时，使用下面的函数释放设备编号：
void unregister_chrdev_region(dev_t dev, unsigned int count);
#### 2-3 file_operations
* 在头文件 **linux/fs.h** 中定义，用来存储驱动内核模块提供的对设备进行各种操作的函数的指针。
定义如下：
	```c
	struct file_operations {
		struct module *owner;
		loff_t (*llseek) (struct file *, loff_t, int);
		ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
		ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
		ssize_t (*aio_read) (struct kiocb *, const struct iovec *, unsigned long, loff_t);
		ssize_t (*aio_write) (struct kiocb *, const struct iovec *, unsigned long, loff_t);
		int (*readdir) (struct file *, void *, filldir_t);
		unsigned int (*poll) (struct file *, struct poll_table_struct *);
		long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
		long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
		int (*mmap) (struct file *, struct vm_area_struct *);
		int (*open) (struct inode *, struct file *);
		int (*flush) (struct file *, fl_owner_t id);
		int (*release) (struct inode *, struct file *);
		int (*fsync) (struct file *, loff_t, loff_t, int datasync);
		int (*aio_fsync) (struct kiocb *, int datasync);
		int (*fasync) (int, struct file *, int);
		int (*lock) (struct file *, int, struct file_lock *);
		ssize_t (*sendpage) (struct file *, struct page *, int, size_t, loff_t *, int);
		unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long, unsigned long, unsigned long);
		int (*check_flags)(int);
		int (*flock) (struct file *, int, struct file_lock *);
		ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, loff_t *, size_t, unsigned int);
		ssize_t (*splice_read)(struct file *, loff_t *, struct pipe_inode_info *, size_t, unsigned int);
		int (*setlease)(struct file *, long, struct file_lock **);
		long (*fallocate)(struct file *file, int mode, loff_t offset,
				  loff_t len);
		int (*show_fdinfo)(struct seq_file *m, struct file *f);
		```
* 主要成员
上层应用程序用户调用open/read/write/seek等对抽象的文件进行操作，就是调用file_operations中的对应的成员函数。
	* open：用来打开设备。
	* read: 用来从设备中读取数据，成功时返回读取的字节数，失败时返回负值。
	* write：用来向设备发送数据，成功时返回写入的字节数。
	* poll： 用来询问设备是否可被阻塞地立即读写。当询问的条件未触发时，用户空间进行select(), poll()系统调用将引起进程阻塞。
	* mmap： 用来将设备内存映射到进程虚拟地址空间中。对应用户空间的 ==mmap()== 。如果驱动未实现此函数，用户调用mmap将返回-ENODEV。
	* compat_ioctl： 被使用在用户空间为32位模式，而内核运行在64位模式时。
	* unlocked_ioctl：用来向设备发送相关控制命令。对应用户空间的 ==ioctl()==  。
	  * ioctl 用户空间原型如下：
	   int ioctl(int fd, unsigned long cmd, ...);
	  * 调用时需要三个参数：设备文件的文件描述符 **fd** ，ioctl控制命令码 **cmd** ，和一个可以被用来传递任何东西的long类型的参数。用户将命令码传递给驱动，驱动根据不同的命令码来进行不同的操作。命令码传递时要保证命令是唯一的，并且和设备一一对应，如果把正确的命令发给错误的设备，或者是把错误的命令发给正确的设备，或者是把错误的命令发给错误的设备都会导致错误。
	  * ioctl控制命令码由4部分组成：type（幻数/ magic number），number(序列号)，direction（数据传送的方向），size(一般为13或者14位)。其中，type 长度为8 位，表示是哪个设备的命令,类型是一个magic number（幻数），ioctl-number.txt给出了推荐以及当前内核中已经使用的“幻数”；序列号表明是设备命令中的第几个；dir表示数据传送的方向，可能的值是 **_IOC_NONE，_IOC_READ，_IOC_WRITE** ；size表示用户数据的大小。
命令控制码组成：
		| type | number | dir  | size |
		| ---  | ------ | ---- | ---- |
		| 8 bit | 8 bit | 2 bit | 13/14 bit |
	  * magic number的选择：取值范围在**0~0xff**之间。对幻数的编号不能**重复定义** 。ioctl-number.txt列举了内核中已经使用过的幻数，选择自己的幻数时要参考**ioctl-number.txt，避免和内核冲突**，如ioctl-number.txt中说明 *‘l'* 的编号已经被占用的范围为00-7F：
'l'     00-3F   linux/tcfs_fs.h 
'l'     40-7F   linux/udf_fs_i.h 

	  * 此外，内核还定义了 ==_IO()== ，==_IOR()== ，==_IOW()== 和 ==_IOWR()==  这 4 个宏来辅助生成命令。

		宏定义代码：
		
		```c
  		 #define _IO(type,nr)          _IOC(_IOC_NONE,(type),(nr),0)
		 #define _IOR(type，nr，size)  _IOC(_IOC_READ,(type),(nr),(_IOC_TYPECHECK(size)))
		 #define _IOW(type,nr,size)    _IOC(_IOC_WRITE,(type),(nr),(_IOC_TYPECHECK(size)))
		 #define _IOWR(type,nr,size)   _IOC(_IOC_READ|_IOC_WRITE,(type),(nr),(_IOC_TYPECHECK(size)))
		 ```
		举例说明：	
		```c
		/* define "0xdf" as magic number */
		#define MY_CMD_MAGIC 0xdf
		#define MYDEV_FIFO_STATUS   _IO(MY_CMD_MAGIC, 1)
		/* mtp cmd */
		#define MTP_SEND_FILE              _IOW('M', 0, struct mtp_file_range)
		```
 
#### 2-4 cdev 相关操作
* 初始化
 有两种方式：==cdev_init()== , ==cdev_alloc()==
* 添加
使用 ==cdev_add()== 函数将cdev添加到系统。
* 删除
使用 ==cdev_del()== 函数将cdev从系统中删除。
相关函数具体如下：
	```c
	void cdev_init(struct cdev *, const struct file_operations *);
	
	struct cdev *cdev_alloc(void);
	
	int cdev_add(struct cdev *, dev_t, unsigned);
	
	void cdev_del(struct cdev *);
	```
### 3 字符设备驱动
字符驱动程序中完成的主要工作是初始化、添加和删除cdev结构体，申请和释放设备号；
填充file_operation结构体中操作函数，并实现其中的read()、write()、ioctl()等重要函数。
#### 3-1 模块加载和卸载

1. 模块介绍
	 * 如果把内核中所有需要的功能都编译到内核，会导致内核很大；
 	* 如果需要新增或者删除某些功能，需要重新编译内核。
所以linux提供模块机制来使得编译出的内核并不需要包含所有功能，而在这些功能需要被使用的时候，动态加载加载相应代码到内核中。
    * **lsmod** 命令可以获得系统中加载了的所有模块以及模块间的依赖关系。
    
    ![enter description here][3]

2. 模块组成
    * 加载：  **insmod/modprobe**  (module_init)
    * 卸载： **rmmod**   (module_exit)
    * 模块声明、描述    (MODULE_DESCRIPTION / MODULE_AUTHOR /MODULE_LICENSE /MODULE_ALIAS/MODULE_VERSION/、MODULE_DEVICE_TABLE)
    * 模块参数  module_param（参数名，参数类型，读写权限)，参数可以通过sys/modules/xxxx/paramters来查看
    * 导出符号   EXPORT_SYMBOL /EXPORT_SYMBOL_GPL 导出的符号可以被其他模块使用

    * 使用 **modinfo \<模块名\>** 命令可以获得模块的信息，包括模块作者、模块的说明、模块所支持的参数等。

![enter description here][4]

3. 模块加载
	* 使用 ==alloc_chrdev_region()== 或 ==register_chrdev_region()== 添加一个主设备号；
	* 用 ==cdev_init()== 初始化cdev结构并建立cdev和file operation之间的连接；
	* 使用 ==cdev_add()== 把cdev结构体与新添加的主设备号关联起来
4. 模块卸载
	* 使用 ==unregister_chrdev_region()== 释放设备号 
	* 使用 ==cdev_del()== 注销设备
#### 3-2 实现 file_operations 结构体中的成员函数
定义一个 file_operations 的实例，并将具体设备驱动的函数赋值给 file_operations 的成员，模板如下：
```c
static const struct file_operations xxx_operations = {
	.open		       = xxx_open,
	.write		       = xxx_write,
	.read		       = xxx_read,
	.unlocked_ioctl    = xxx_ioctl,
	.release	       = xxx_release,
	...
};
```
#### 3-3 字符设备驱动结构和字符设备驱动关系
![字符设备][5]
### 4. 驱动使用举例  

```c
int fd;
char buf[100];
fd =open(“/dev/xxx”, O_RDONLY, 0);
if (-1 == fd)
    return ERROR;
 
read(fd, buf, 100);
close(fd);
```

![enter description here][6]


  [1]: ./images/1477890923896.jpg "设备驱动与系统关系"
  [2]: ./images/1477891042325.jpg "/dev下的部分设备文件"
  [3]: ./images/1477968300734.jpg "lsmod "
  [4]: ./images/1477917763476.jpg "modinfo usbhid.ko"
  [5]: ./images/1477741493127.jpg "字符设备驱动结构"
  [6]: ./images/1477883363321.jpg "驱动设备调用流程"
