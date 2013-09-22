#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#include <stdint.h>

#include <stdio.h>
struct PlayLoad{
	uint32_t chan_num;
	uint8_t ts_data[188];
};
struct PlayLoad buf[4096*sizeof(struct PlayLoad)];
int main(int argc, char* argv[]){
	int i = 0;
	printf("===================================begin\n");
	printf("dev path = %s\n",argv[1]);
	int fd = open(argv[1],O_RDWR);
	if( fd < 0 ){
		printf("open %s error = %d\n",argv[1],fd);
		return 0;
	}
	
	printf("test.ts path = %s\n",argv[2]);
	int fdw = open(argv[2],O_CREAT|O_RDWR);
	if( fdw < 0 ){
		printf("open %s error = %d\n",argv[2],fdw);
		return 0;
	}
	//ssize_t ret = write(fd,buf,sizeof(buf));
	usleep(1000000);
	ssize_t retw = 0;
	do{
		//continue;
		ssize_t ret = read(fd,buf,sizeof(buf));
		printf("%02x\n",buf[0].chan_num);
		printf("%02x\n",buf[0].ts_data[0]);
		if( buf[0].ts_data[0] != 0x47 ){
			break;
		}
		//printf( "%d" , sizeof(struct PlayLoad) );
		printf("ret = %ld\n",ret);
		if( (ret %192 ) != 0 ){
			break;
		}
		if( ret == 0 ){
			while(1){
			}
			break;
		}
		for(i = 0 ; (i*sizeof(struct PlayLoad)) < ret ; i++ ){
			//printf("%02x",buf[i].chan_num);
			retw += write(fdw,buf[i].ts_data,188);
		}
		printf("retw = %ld\n",retw);
		//break;
	}while(1);
	close(fd);
	close(fdw);
}
