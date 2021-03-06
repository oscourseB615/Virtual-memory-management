﻿#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include "vmm.h"

/* 页表 */
PageTableItem pageTable[ROOT_PAGE_SUM][SEC_PAGE_SUM];
/* 实存空间 */
BYTE actMem[ACTUAL_MEMORY_SIZE];
/* 用文件模拟辅存空间 */
FILE *ptr_auxMem;
/* 物理块使用标识 */
BOOL blockStatus[BLOCK_SUM];
/* 访存请求 */
Ptr_MemoryAccessRequest ptr_memAccReq;
/*response*/
int response=0;
VMM_cmd cmd;
/* 初始化环境 */
void do_init()
{
	int i, j,k;
	int flag;
	srandom(time(NULL));
	for (i = 0; i < ROOT_PAGE_SUM; i++)
	{
		for(k=0,flag=0;k<SEC_PAGE_SUM;k++,flag++)
        {
		pageTable[i][k].pageNum = i;
		pageTable[i][k].filled = FALSE;
		pageTable[i][k].edited = FALSE;
		pageTable[i][k].count = 0;
		if((flag%2)==0){
		pageTable[i][k].processnum=0;
		}
		else{
		pageTable[i][k].processnum=1;
		}
		/* 使用随机数设置该页的保护类型 */
		switch (random() % 7)
		{
			case 0:
			{
				pageTable[i][k].proType = READABLE;
				break;
			}
			case 1:
			{
				pageTable[i][k].proType = WRITABLE;
				break;
			}
			case 2:
			{
				pageTable[i][k].proType = EXECUTABLE;
				break;
			}
			case 3:
			{
				pageTable[i][k].proType = READABLE | WRITABLE;
				break;
			}
			case 4:
			{   
				pageTable[i][k].proType = READABLE | EXECUTABLE;
				break;
			}
			case 5:
			{
				pageTable[i][k].proType = WRITABLE | EXECUTABLE;
				break;
			}
			case 6:
			{
				pageTable[i][k].proType = READABLE | WRITABLE | EXECUTABLE;
				break;
			}
			default:
				break;
		}
		/* 设置该页对应的辅存地址 */
		pageTable[i][k].auxAddr = (i*16+k)* PAGE_SIZE * 2;
	}}
	for (j = 0; j < BLOCK_SUM; j++)
	{
		/* 随机选择一些物理块进行页面装入 */
		if (random() % 2 == 0)
		{
			i=random()%ROOT_PAGE_SUM;
			k=random()%SEC_PAGE_SUM;
			do_page_in(&pageTable[i][k], j);
			pageTable[i][k].blockNum = j;
			pageTable[i][k].filled = TRUE;
			blockStatus[j] = TRUE;
		}
		else
			blockStatus[j] = FALSE;
	}
}

/* 响应请求 */
void do_response()
{	
	Ptr_PageTableItem ptr_pageTabIt;
	unsigned int rootpageNum,secpagenum, offAddr,secpage;
	unsigned int actAddr;
	int i,j,tmp1,tmp2;
	/* 检查地址是否越界 */
	if (ptr_memAccReq->virAddr < 0 || ptr_memAccReq->virAddr >= VIRTUAL_MEMORY_SIZE)
	{
		do_error(ERROR_OVER_BOUNDARY);
		return;
	}
	/* 计算页号和页内偏移值 */
	secpage=PAGE_SIZE*SEC_PAGE_SUM;
	rootpageNum = ptr_memAccReq->virAddr / secpage;
	secpagenum=ptr_memAccReq->virAddr%secpage/PAGE_SIZE;
	offAddr = ptr_memAccReq->virAddr % PAGE_SIZE;
	printf("一级页号为：%u\t二级页号为%u\t页内偏移为：%u\n", rootpageNum, secpagenum,offAddr);


	/* 获取对应页表项 */

	ptr_pageTabIt = &pageTable[rootpageNum][secpagenum];
	
	tmp1=ptr_pageTabIt->processnum;
	
	tmp2=ptr_memAccReq->processnum;
	
	//printf("tmp1:%d\ttmp2:%d");
	if(tmp1 - tmp2 ==0 )
	{
		
	
	/* 根据特征位决定是否产生缺页中断 */
	if (!ptr_pageTabIt->filled)
	{
		do_page_fault(ptr_pageTabIt);
	}
	for(i=0;i<ROOT_PAGE_SUM;i++){
		for(j=0;j<SEC_PAGE_SUM;j++){
		pageTable[i][j].count>>=1;}}

	actAddr = ptr_pageTabIt->blockNum * PAGE_SIZE + offAddr;
	printf("实地址为：%u\n", actAddr);

	/* 检查页面访问权限并处理访存请求 */
	switch (ptr_memAccReq->reqType)
	{
		case REQUEST_READ: //读请求
		{
		//	ptr_pageTabIt->count++;
			ptr_pageTabIt->count|=0x80000000;
			if (!(ptr_pageTabIt->proType & READABLE)) //页面不可读
			{
				do_error(ERROR_READ_DENY);
				return;
			}
			/* 读取实存中的内容 */
			printf("读操作成功：值为%02X\n", actMem[actAddr]);
			break;
		}
		case REQUEST_WRITE: //写请求
		{
	//		ptr_pageTabIt->count++;
			ptr_pageTabIt->count|=0x80000000;
			if (!(ptr_pageTabIt->proType & WRITABLE)) //页面不可写
			{
				do_error(ERROR_WRITE_DENY);
				return;
			}
			/* 向实存中写入请求的内容 */
			actMem[actAddr] = ptr_memAccReq->value;
			ptr_pageTabIt->edited = TRUE;
			printf("写操作成功\n");
			break;
		}
		case REQUEST_EXECUTE: //执行请求
		{
	//		ptr_pageTabIt->count++;
			ptr_pageTabIt->count|=0x80000000;
			if (!(ptr_pageTabIt->proType & EXECUTABLE)) //页面不可执行
			{
				do_error(ERROR_EXECUTE_DENY);
				return;
			}
			printf("执行成功\n");
			break;
		}
		default: //非法请求类型
		{
			do_error(ERROR_INVALID_REQUEST);
			return;
		}
	}
	}
	else{
		printf("进程号错误\n");
	}
}

/* 处理缺页中断 */
void do_page_fault(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i;
	printf("产生缺页中断，开始进行调页...\n");
	for (i = 0; i < BLOCK_SUM; i++)
	{
		if (!blockStatus[i])
		{
			/* 读辅存内容，写入到实存 */
			do_page_in(ptr_pageTabIt, i);

			/* 更新页表内容 */
			ptr_pageTabIt->blockNum = i;
			ptr_pageTabIt->filled = TRUE;
			ptr_pageTabIt->edited = FALSE;
			ptr_pageTabIt->count = 0;

			blockStatus[i] = TRUE;
			return;
		}
	}
	/* 没有空闲物理块，进行页面替换 */
	do_LRU(ptr_pageTabIt);
}

/* 根据LFU算法进行页面替换 */
void do_LFU(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i,j, min, page_i,page_j;
	printf("没有空闲物理块，开始进行LFU页面替换...\n");
	for (i = 0, min = 0xFFFFFFFF, page_i= 0,page_j=0; i < ROOT_PAGE_SUM; i++)
	{   for(j=0;j<SEC_PAGE_SUM;j++)
		if (pageTable[i][j].count < min)
		{
			min = pageTable[i][j].count;
			page_i= i;
            page_j=j;
		}
	}
	printf("选择第一级页表%u页，二级页表第%u页进行替换\n", page_i,page_j);
	if (pageTable[page_i][page_j].edited)
	{
		/* 页面内容有修改，需要写回至辅存 */
		printf("该页内容有修改，写回至辅存\n");
		do_page_out(&pageTable[page_i][page_j]);
	}
	pageTable[page_i][page_j].filled = FALSE;
	pageTable[page_i][page_j].count = 0;


	/* 读辅存内容，写入到实存 */
	do_page_in(ptr_pageTabIt, pageTable[page_i][page_j].blockNum);

	/* 更新页表内容 */
	ptr_pageTabIt->blockNum = pageTable[page_i][page_j].blockNum;
	ptr_pageTabIt->filled = TRUE;
	ptr_pageTabIt->edited = FALSE;
	ptr_pageTabIt->count = 0;
	printf("页面替换成功\n");
}

/* 根据LRU算法进行页面替换 */
void do_LRU(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i,j, min, page_i,page_j;
	printf("没有空闲物理块，开始进行LRU页面替换...\n");
	for (i = 0, min = 0xFFFFFFFF, page_i= 0,page_j=0; i < ROOT_PAGE_SUM; i++)
	{   for(j=0;j<SEC_PAGE_SUM;j++)
		if (pageTable[i][j].count < min)
		{
			min = pageTable[i][j].count;
			page_i= i;
         	        page_j=j;
		}
	}
	printf("选择第一级页表%u页，二级页表第%u页进行替换\n", page_i,page_j);
	if (pageTable[page_i][page_j].edited)
	{
//		页面内容有修改，需要写回至辅存 
		printf("该页内容有修改，写回至辅存\n");
		do_page_out(&pageTable[page_i][page_j]);
	}
	pageTable[page_i][page_j].filled = FALSE;
	pageTable[page_i][page_j].count = 0;


	 //读辅存内容，写入到实存 
	do_page_in(ptr_pageTabIt, pageTable[page_i][page_j].blockNum);

//	 更新页表内容 
	ptr_pageTabIt->blockNum = pageTable[page_i][page_j].blockNum;
	ptr_pageTabIt->filled = TRUE;
	ptr_pageTabIt->edited = FALSE;
	ptr_pageTabIt->count = 0;
	printf("页面替换成功\n");
}

/* 将辅存内容写入实存 */
void do_page_in(Ptr_PageTableItem ptr_pageTabIt, unsigned int blockNum)
{
	unsigned int readNum;
	if (fseek(ptr_auxMem, ptr_pageTabIt->auxAddr, SEEK_SET) < 0)
	{
	#ifdef DEBUG
			printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
	#endif
		do_error(ERROR_FILE_SEEK_FAILED);
		exit(1);
	}
	if ((readNum = fread(actMem + blockNum * PAGE_SIZE,
		sizeof(BYTE), PAGE_SIZE, ptr_auxMem)) < PAGE_SIZE)
	{
	#ifdef DEBUG
			printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
			printf("DEBUG: blockNum=%u\treadNum=%u\n", blockNum, readNum);
			printf("DEGUB: feof=%d\tferror=%d\n", feof(ptr_auxMem), ferror(ptr_auxMem));
	#endif
		do_error(ERROR_FILE_READ_FAILED);
		exit(1);
	}
	printf("调页成功：辅存地址%u-->>物理块%u\n", ptr_pageTabIt->auxAddr, blockNum);
}

/* 将被替换页面的内容写回辅存 */
void do_page_out(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int writeNum;
	if (fseek(ptr_auxMem, ptr_pageTabIt->auxAddr, SEEK_SET) < 0)
	{
		#ifdef DEBUG
				printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt, ftell(ptr_auxMem));
		#endif
		do_error(ERROR_FILE_SEEK_FAILED);
		exit(1);
	}
	if ((writeNum = fwrite(actMem + ptr_pageTabIt->blockNum * PAGE_SIZE,
			sizeof(BYTE), PAGE_SIZE, ptr_auxMem)) < PAGE_SIZE){
		#ifdef DEBUG
			printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
			printf("DEBUG: writeNum=%u\n", writeNum);
			printf("DEGUB: feof=%d\tferror=%d\n", feof(ptr_auxMem), ferror(ptr_auxMem));
		#endif
		do_error(ERROR_FILE_WRITE_FAILED);
		exit(1);
	}
	printf("写回成功：物理块%u-->>辅存地址%03X\n", ptr_pageTabIt->auxAddr, ptr_pageTabIt->blockNum);
}

/* 错误处理 */
void do_error(ERROR_CODE code)
{
	switch (code)
	{
		case ERROR_READ_DENY:
		{
			printf("访存失败：该地址内容不可读\n");
			break;
		}
		case ERROR_WRITE_DENY:
		{
			printf("访存失败：该地址内容不可写\n");
			break;
		}
		case ERROR_EXECUTE_DENY:
		{
			printf("访存失败：该地址内容不可执行\n");
			break;
		}
		case ERROR_INVALID_REQUEST:
		{
			printf("访存失败：非法访存请求\n");
			break;
		}
		case ERROR_OVER_BOUNDARY:
		{
			printf("访存失败：地址越界\n");
			break;
		}
		case ERROR_FILE_OPEN_FAILED:
		{
			printf("系统错误：打开文件失败\n");
			break;
		}
		case ERROR_FILE_CLOSE_FAILED:
		{
			printf("系统错误：关闭文件失败\n");
			break;
		}
		case ERROR_FILE_SEEK_FAILED:
		{
			printf("系统错误：文件指针定位失败\n");
			break;
		}
		case ERROR_FILE_READ_FAILED:
		{
			printf("系统错误：读取文件失败\n");
			break;
		}
		case ERROR_FILE_WRITE_FAILED:
		{
			printf("系统错误：写入文件失败\n");
			break;
		}
		case ERROR_FIFO_REMOVE_FAILED:
		{
			printf("FIFO移除失败\n");
			break;
		}
		case ERROR_FIFO_MAKE_FAILED:			//FIFO创建失败
		{
			printf("FIFO创建失败\n");
			break;
		}
		case ERROR_FIFO_OPEN_FAILED:			//FIFO打开失败
		{
			printf("FIFO打开失败\n");
			break;
		}
		case ERROR_FIFO_READ_FAILED:			//FIFO读取失败
		{
			printf("FIFO读取失败\n");
			break;
		}
		default:
		{
			printf("未知错误：没有这个错误代码\n");
		}
	}
}

/* 打印页表 */
void do_print_info()
{
	unsigned int i, j, k;
	char str[4];
	printf("一级页号\t二级页号\t块号\t装入\t修改\t保护\t计数\t\t辅存\t进程号\n");
	for (i = 0; i < ROOT_PAGE_SUM; i++)
    {
        for(j=0;j<SEC_PAGE_SUM;j++)
		{
			printf("%u\t\t%u\t\t%u\t%u\t%u\t%s\t%08x\t%u\t%u\n", i,j, pageTable[i][j].blockNum, pageTable[i][j].filled,
				pageTable[i][j].edited, get_proType_str(str, pageTable[i][j].proType),
				pageTable[i][j].count, pageTable[i][j].auxAddr,pageTable[i][j].processnum);
		}
	}	
}

/* 获取页面保护类型字符串 */
char *get_proType_str(char *str, BYTE type)
{
	if (type & READABLE)
		str[0] = 'r';
	else
		str[0] = '-';
	if (type & WRITABLE)
		str[1] = 'w';
	else
		str[1] = '-';
	if (type & EXECUTABLE)
		str[2] = 'x';
	else
		str[2] = '-';
	str[3] = '\0';
	return str;
}

void initfile(){
	int i;
	char* key="0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char buffer[VIRTUAL_MEMORY_SIZE*2+1];
	int err;
	ptr_auxMem=fopen(AUXILIARY_MEMORY,"w+");
	for(i=0;i<VIRTUAL_MEMORY_SIZE*2-3;i++)
	{
	    buffer[i]=key[random()%62];
	}
	buffer[VIRTUAL_MEMORY_SIZE*2-3]='y';
	buffer[VIRTUAL_MEMORY_SIZE*2-2]='m';
	buffer[VIRTUAL_MEMORY_SIZE*2-1]='c';
	buffer[VIRTUAL_MEMORY_SIZE*2]='\0';
	//随机生成256位字符串
	fwrite(buffer,sizeof(BYTE),VIRTUAL_MEMORY_SIZE*2,ptr_auxMem);
	/*
	size_t fwrite(const void* buffer, size_t size,size_t count,FILE* stream)
	*/
	printf("系统提示：初始化辅存模拟文件完成\n");
	fclose(ptr_auxMem);
}

void do_print_res(){
	int i=0;
	printf("实存:\n");
	for(i=0;i<ACTUAL_MEMORY_SIZE;i++)
       { printf("地址:  %d\t内容:  %c\t",i,actMem[i]);
         if(i%4==3)
	 printf("\n");}
	printf("\n");
}

void do_print_file(){

    ptr_auxMem = fopen(AUXILIARY_MEMORY, "r+");
    char ch;
	int i=0;
	printf("辅存:\n");
    while((ch=fgetc(ptr_auxMem))!=EOF)
       { printf("地址:  %d\t内容:  %c\t",i,ch);
               i++; 
	if((ch=fgetc(ptr_auxMem))!=EOF)
	{ printf("地址:  %d\t内容:  %c\t",i,ch);
               i++; }
	else break;
	if((ch=fgetc(ptr_auxMem))!=EOF)
 	{printf("地址:  %d\t内容:  %c\t",i,ch);
               i++; }
	else break;
	if((ch=fgetc(ptr_auxMem))!=EOF)
 	{printf("地址:  %d\t内容:  %c\n",i,ch);
               i++; }
	else break;
	}
	printf("\n");
}

int main(int argc, char* argv[])
{
	char c;
	int i,s;
	int pipe_fg;
	int count;
	
	initfile();
	if (!(ptr_auxMem = fopen(AUXILIARY_MEMORY, "r+")))
	{
		do_error(ERROR_FILE_OPEN_FAILED);
		exit(1);
	}

	do_init();
	do_print_info();
	ptr_memAccReq = &cmd.memAccReq;
	//ptr_memAccReq = (Ptr_MemoryAccessRequest) malloc(sizeof(MemoryAccessRequest));

	struct stat statbuf;
	if(stat(FIFO_NAME,&statbuf)==0){
			/* 如果FIFO文件存在,删掉 */
		if(remove(FIFO_NAME)<0){
			do_error(ERROR_FIFO_REMOVE_FAILED);
			exit(1);
		}
	}
	if(mkfifo(FIFO_NAME,0666)<0){
		do_error(ERROR_FIFO_MAKE_FAILED);
		exit(1);
	}
	if((pipe_fg = open(FIFO_NAME,O_RDONLY))<0){
		do_error(ERROR_FIFO_OPEN_FAILED);
		exit(1);
	}

	/* 在循环中模拟访存请求与处理过程 */
	while (TRUE)
	{
		
		bzero(&cmd,sizeof(VMM_cmd));
		if((count = read(pipe_fg, &cmd, sizeof(VMM_cmd)))<0){
			do_error(ERROR_FIFO_READ_FAILED);
			exit(1);
		}
		if(count == 0)
			continue;
		//printf("getreq");
		c = cmd.cmdType;
		if( c == '1' || c == '2'){
			do_response();
		}
		else if(c == 'y' || c == 'Y'){
			do_print_info();
		}
		else if(c == 'z' || c == 'Z'){
			//printf("debug do_print_res");
			do_print_res();
		}
		else if(c == 'w' || c == 'W'){
			do_print_file();
		}
		else if(c == 'x' || c == 'X'){
			break;
		}
	}
	if (fclose(ptr_auxMem) == EOF)
	{
		do_error(ERROR_FILE_CLOSE_FAILED);
		exit(1);
	}
	return (0);
}
