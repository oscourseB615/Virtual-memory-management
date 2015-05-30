#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include <time.h>
#include <stdio.h>
#include "vmm.h"

VMM_cmd cmd;
Ptr_MemoryAccessRequest ptr_memAccReq = &cmd.memAccReq;
int main(int argc,char *argv[])
{
	srandom(time(NULL));
	int pipe_fd;
	char c;
	
	while(1){
		bzero(&cmd,sizeof(VMM_cmd));
		printf("按1手动出入请求，按2自动生成请求，按Y/y打印页表，按Z/z键打印实存内容，按W/w打印辅存内容，按X/x键退出\n");
		while((c = getchar()) == '\n')
			;
		cmd.cmdType = c;
		putchar(c);
		putchar('\n');
		if(c == '1' || c == '2'){
			do_request(c - '0');
		}
		if((pipe_fd = open(FIFO_NAME,O_WRONLY))<0){
			printf("Open FIFO fail!\n");
			exit(-1);
		}
		if(write(pipe_fd, &cmd, sizeof(VMM_cmd))<0){
			printf("Write FIFO fail\n");
			exit(-1);
		}
		close(pipe_fd);
	}
	
	return 0;
}

/*根据输入产生请求*/
void do_request(int req_kind)
{
	
	//int req_kind;	/*请求产生类型*/
	int req_Type;	/*请求类型*/
	char b;			/*如果为写请求，待写入的值*/

	if(req_kind == 1){
		/* 输入请求地址 */
		printf("输入请求地址...\n");
		scanf("%d",&ptr_memAccReq->virAddr);
		/* 输入请求类型 */
		printf("输入请求类型,0: 读取\t1: 写入\t2:执行\n");
		scanf("%d",&req_Type);
	}
	else{
		ptr_memAccReq->virAddr = random() % VIRTUAL_MEMORY_SIZE;
		req_Type = random() % 3;
		//getchar();
	}
	
	switch (req_Type)
	{
		case 0: //读请求
		{
			ptr_memAccReq->reqType = REQUEST_READ;
			printf("产生请求：\n地址：%u\t类型：读取\n", ptr_memAccReq->virAddr);
			break;
		}
		case 1: //写请求
		{
			ptr_memAccReq->reqType = REQUEST_WRITE;
			/* 输入待写入的值 */
			if(req_kind == 1){
				printf("输入待写入的值 ...\n");
				getchar();
				scanf("%c",&b);
				//getchar();
			}
			else{
				b = random();
			}
			ptr_memAccReq->value =b % 0xFFu;
			printf("产生请求：\n地址：%u\t类型：写入\t值：%02X\n", ptr_memAccReq->virAddr, ptr_memAccReq->value);
			break;
		}
		case 2:
		{
			ptr_memAccReq->reqType = REQUEST_EXECUTE;
			printf("产生请求：\n地址：%u\t类型：执行\n", ptr_memAccReq->virAddr);
			break;
		}
		default:
			break;
	}
}