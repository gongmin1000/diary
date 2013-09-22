#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include <stdio.h>
struct RegBuf{
	uint32_t _reg_index;
	uint32_t _len;
	uint32_t _reg_buf[254];
};
static unsigned char buf[1024*188];
int main(int argc, char* argv[]){
	int i;
	char* strtemp;
	char* substr;
	uint32_t cmd = 1;
	struct RegBuf buf;
	printf("===================================begin\n");
	if( argc != 5 ){
		printf("缺少参数\n");
		return 0;
	}
	printf("dev path = %s\n",argv[1]);

	if( strcmp(argv[2],"r") == 0 ){
		cmd = 3;
		buf._reg_index = atoi(argv[3]);
		buf._len = atoi(argv[4]);
		if( buf._reg_index > 253){
			printf("寄存器 下标超范围");
			return 0;
		}
		if ( buf._len > 254 ){
			printf("寄存器个数超范围\n");
			return 0;
		}
		printf("reg index = %d\n",buf._reg_index);
		printf("len = %d\n",buf._len);
	}
	else if( strcmp(argv[2],"w") == 0 ){
		cmd = 4;
		buf._reg_index = atoi(argv[3]);
		printf("reg index = %d\n",buf._reg_index);
		if( buf._reg_index > 253){
			printf("寄存器 下标超范围");
			return 0;
		}
		i = 0;
		substr = strtok(argv[4],",");
		while(substr){
			if( i > 253){
				break;
			}
			//printf("%s\n",substr);
			buf._reg_buf[i+buf._reg_index] = strtoul(substr,NULL,16);
			printf("wwwww:%08x\n",buf._reg_buf[i+buf._reg_index]);
			i++;
			substr = strtok(NULL,",");
		
		}
		buf._len = i;
		printf("len = %d\n",buf._len);
	}
	else{
		cmd = 5;
	}
	printf("cmd = %d\n",cmd);
	int fd = open(argv[1],O_RDWR);
	if( fd < 0 ){
		printf("open %s error = %d\n",argv[1],fd);
		return 0;
	}
	//ssize_t ret = write(fd,buf,sizeof(buf));
	//ssize_t ret = read(fd,buf,sizeof(buf));
	//printf("ret = %ld\n",ret);
	long ret = ioctl(fd,cmd,&buf);
	printf("reg_list:\n");
	for(i = 0 ;  i < buf._len ; i++){
		printf("%08x\n",buf._reg_buf[i+buf._reg_index]);
	}
	close(fd);
}
