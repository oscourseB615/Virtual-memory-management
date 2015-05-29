#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include "vmm.h"

/* ҳ�� */
PageTableItem pageTable[ROOT_PAGE_SUM][SEC_PAGE_SUM];
/* ʵ��ռ� */
BYTE actMem[ACTUAL_MEMORY_SIZE];
/* ���ļ�ģ�⸨��ռ� */
FILE *ptr_auxMem;
/* �����ʹ�ñ�ʶ */
BOOL blockStatus[BLOCK_SUM];
/* �ô����� */
Ptr_MemoryAccessRequest ptr_memAccReq;



/* ��ʼ������ */
void do_init()
{
	int i, j,k;
	srandom(time(NULL));
	for (i = 0; i < ROOT_PAGE_SUM; i++)
	{
		for(k=0;k<SEC_PAGE_SUM;k++)
        {
		pageTable[i][k].pageNum = i;
		pageTable[i][k].filled = FALSE;
		pageTable[i][k].edited = FALSE;
		pageTable[i][k].count = 0;
		/* ʹ����������ø�ҳ�ı������� */
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
		/* ���ø�ҳ��Ӧ�ĸ����ַ */
		pageTable[i][k].auxAddr = i * PAGE_SIZE * 2;
	}}
	for (j = 0; j < BLOCK_SUM; j++)
	{
		/* ���ѡ��һЩ��������ҳ��װ�� */
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


/* ��Ӧ���� */
void do_response()
{
	Ptr_PageTableItem ptr_pageTabIt;
	unsigned int rootpageNum,secpagenum, offAddr,secpage;
	unsigned int actAddr;

	/* ����ַ�Ƿ�Խ�� */
	if (ptr_memAccReq->virAddr < 0 || ptr_memAccReq->virAddr >= VIRTUAL_MEMORY_SIZE)
	{
		do_error(ERROR_OVER_BOUNDARY);
		return;
	}

	/* ����ҳ�ź�ҳ��ƫ��ֵ */
	secpage=PAGE_SIZE*SEC_PAGE_SUM;
	rootpageNum = ptr_memAccReq->virAddr / secpage;
	secpagenum=ptr_memAccReq->virAddr%secpage/PAGE_SIZE;
	offAddr = ptr_memAccReq->virAddr % PAGE_SIZE;
	printf("һ��ҳ��Ϊ��%u\t����ҳ��Ϊ%u\tҳ��ƫ��Ϊ��%u\n", rootpageNum, secpagenum,offAddr);

	/* ��ȡ��Ӧҳ���� */
	ptr_pageTabIt = &pageTable[rootpageNum][secpagenum];

	/* ��������λ�����Ƿ����ȱҳ�ж� */
	if (!ptr_pageTabIt->filled)
	{
		do_page_fault(ptr_pageTabIt);
	}

	actAddr = ptr_pageTabIt->blockNum * PAGE_SIZE + offAddr;
	printf("ʵ��ַΪ��%u\n", actAddr);

	/* ���ҳ�����Ȩ�޲�����ô����� */
	switch (ptr_memAccReq->reqType)
	{
		case REQUEST_READ: //������
		{
			ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & READABLE)) //ҳ�治�ɶ�
			{
				do_error(ERROR_READ_DENY);
				return;
			}
			/* ��ȡʵ���е����� */
			printf("�������ɹ���ֵΪ%02X\n", actMem[actAddr]);
			break;
		}
		case REQUEST_WRITE: //д����
		{
			ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & WRITABLE)) //ҳ�治��д
			{
				do_error(ERROR_WRITE_DENY);
				return;
			}
			/* ��ʵ����д����������� */
			actMem[actAddr] = ptr_memAccReq->value;
			ptr_pageTabIt->edited = TRUE;
			printf("д�����ɹ�\n");
			break;
		}
		case REQUEST_EXECUTE: //ִ������
		{
			ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & EXECUTABLE)) //ҳ�治��ִ��
			{
				do_error(ERROR_EXECUTE_DENY);
				return;
			}
			printf("ִ�гɹ�\n");
			break;
		}
		default: //�Ƿ���������
		{
			do_error(ERROR_INVALID_REQUEST);
			return;
		}
	}
}

/* ����ȱҳ�ж� */
void do_page_fault(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i;
	printf("����ȱҳ�жϣ���ʼ���е�ҳ...\n");
	for (i = 0; i < BLOCK_SUM; i++)
	{
		if (!blockStatus[i])
		{
			/* ���������ݣ�д�뵽ʵ�� */
			do_page_in(ptr_pageTabIt, i);

			/* ����ҳ������ */
			ptr_pageTabIt->blockNum = i;
			ptr_pageTabIt->filled = TRUE;
			ptr_pageTabIt->edited = FALSE;
			ptr_pageTabIt->count = 0;

			blockStatus[i] = TRUE;
			return;
		}
	}
	/* û�п�������飬����ҳ���滻 */
	do_LFU(ptr_pageTabIt);
}

/* ����LFU�㷨����ҳ���滻 */
void do_LFU(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i,j, min, page_i,page_j;
	printf("û�п�������飬��ʼ����LFUҳ���滻...\n");
	for (i = 0, min = 0xFFFFFFFF, page_i= 0,page_j=0; i < ROOT_PAGE_SUM; i++)
	{   for(j=0;j<SEC_PAGE_SUM;j++)
		if (pageTable[i][j].count < min)
		{
			min = pageTable[i][j].count;
			page_i= i;
            page_j=j;
		}
	}
	printf("ѡ���һ��ҳ��%uҳ������ҳ���u%ҳ�����滻\n", page_i,page_j);
	if (pageTable[page_i][page_j].edited)
	{
		/* ҳ���������޸ģ���Ҫд�������� */
		printf("��ҳ�������޸ģ�д��������\n");
		do_page_out(&pageTable[page_i][page_j]);
	}
	pageTable[page_i][page_j].filled = FALSE;
	pageTable[page_i][page_j].count = 0;


	/* ���������ݣ�д�뵽ʵ�� */
	do_page_in(ptr_pageTabIt, pageTable[page_i][page_j].blockNum);

	/* ����ҳ������ */
	ptr_pageTabIt->blockNum = pageTable[page_i][page_j].blockNum;
	ptr_pageTabIt->filled = TRUE;
	ptr_pageTabIt->edited = FALSE;
	ptr_pageTabIt->count = 0;
	printf("ҳ���滻�ɹ�\n");
}

/* ����������д��ʵ�� */
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
	printf("��ҳ�ɹ��������ַ%u-->>�����%u\n", ptr_pageTabIt->auxAddr, blockNum);
}

/* �����滻ҳ�������д�ظ��� */
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
		sizeof(BYTE), PAGE_SIZE, ptr_auxMem)) < PAGE_SIZE)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
		printf("DEBUG: writeNum=%u\n", writeNum);
		printf("DEGUB: feof=%d\tferror=%d\n", feof(ptr_auxMem), ferror(ptr_auxMem));
#endif
		do_error(ERROR_FILE_WRITE_FAILED);
		exit(1);
	}
	printf("д�سɹ��������%u-->>�����ַ%03X\n", ptr_pageTabIt->auxAddr, ptr_pageTabIt->blockNum);
}

/* ������ */
void do_error(ERROR_CODE code)
{
	switch (code)
	{
		case ERROR_READ_DENY:
		{
			printf("�ô�ʧ�ܣ��õ�ַ���ݲ��ɶ�\n");
			break;
		}
		case ERROR_WRITE_DENY:
		{
			printf("�ô�ʧ�ܣ��õ�ַ���ݲ���д\n");
			break;
		}
		case ERROR_EXECUTE_DENY:
		{
			printf("�ô�ʧ�ܣ��õ�ַ���ݲ���ִ��\n");
			break;
		}
		case ERROR_INVALID_REQUEST:
		{
			printf("�ô�ʧ�ܣ��Ƿ��ô�����\n");
			break;
		}
		case ERROR_OVER_BOUNDARY:
		{
			printf("�ô�ʧ�ܣ���ַԽ��\n");
			break;
		}
		case ERROR_FILE_OPEN_FAILED:
		{
			printf("ϵͳ���󣺴��ļ�ʧ��\n");
			break;
		}
		case ERROR_FILE_CLOSE_FAILED:
		{
			printf("ϵͳ���󣺹ر��ļ�ʧ��\n");
			break;
		}
		case ERROR_FILE_SEEK_FAILED:
		{
			printf("ϵͳ�����ļ�ָ�붨λʧ��\n");
			break;
		}
		case ERROR_FILE_READ_FAILED:
		{
			printf("ϵͳ���󣺶�ȡ�ļ�ʧ��\n");
			break;
		}
		case ERROR_FILE_WRITE_FAILED:
		{
			printf("ϵͳ����д���ļ�ʧ��\n");
			break;
		}
		default:
		{
			printf("δ֪����û������������\n");
		}
	}
}

/* �����ô����� */
void do_request()
{
	/* ������������ַ */
	ptr_memAccReq->virAddr = random() % VIRTUAL_MEMORY_SIZE;
	/* ��������������� */
	switch (random() % 3)
	{
		case 0: //������
		{
			ptr_memAccReq->reqType = REQUEST_READ;
			printf("��������\n��ַ��%u\t���ͣ���ȡ\n", ptr_memAccReq->virAddr);
			break;
		}
		case 1: //д����
		{
			ptr_memAccReq->reqType = REQUEST_WRITE;
			/* ���������д���ֵ */
			ptr_memAccReq->value = random() % 0xFFu;
			printf("��������\n��ַ��%u\t���ͣ�д��\tֵ��%02X\n", ptr_memAccReq->virAddr, ptr_memAccReq->value);
			break;
		}
		case 2:
		{
			ptr_memAccReq->reqType = REQUEST_EXECUTE;
			printf("��������\n��ַ��%u\t���ͣ�ִ��\n", ptr_memAccReq->virAddr);
			break;
		}
		default:
			break;
	}
}
void do_request1()
{
	/* ���������ַ */
	int a;
	char b;
	printf("���������ַ...\n");
	scanf("%d",&a);
	ptr_memAccReq->virAddr =a;
	/* ������������ */
	printf("������������,0-��ȡ��1-д�룬2-ִ��...\n");
	scanf("%d",&a);
	switch (a)
	{
		case 0: //������
		{
			ptr_memAccReq->reqType = REQUEST_READ;
			printf("��������\n��ַ��%u\t���ͣ���ȡ\n", ptr_memAccReq->virAddr);
			break;
		}
		case 1: //д����
		{
			ptr_memAccReq->reqType = REQUEST_WRITE;
			/* �����д���ֵ */
			printf("�����д���ֵ ...\n");
			getchar();
			scanf("%c",&b);

			ptr_memAccReq->value =b% 0xFFu;
			printf("��������\n��ַ��%u\t���ͣ�д��\tֵ��%02X\n", ptr_memAccReq->virAddr, ptr_memAccReq->value);
			break;
		}
		case 2:
		{
			ptr_memAccReq->reqType = REQUEST_EXECUTE;
			printf("��������\n��ַ��%u\t���ͣ�ִ��\n", ptr_memAccReq->virAddr);
			break;
		}
		default:
			break;
	}
}
/* ��ӡҳ�� */
void do_print_info()
{
	unsigned int i, j, k;
	char str[4];
	printf("һ��ҳ��\t����ҳ��\t���\tװ��\t�޸�\t����\t����\t����\n");
	for (i = 0; i < ROOT_PAGE_SUM; i++)
    {
        for(j=0;j<SEC_PAGE_SUM;j++)
	{
		printf("%u\t\t%u\t\t%u\t%u\t%u\t%s\t%u\t%u\n", i,j, pageTable[i][j].blockNum, pageTable[i][j].filled,
			pageTable[i][j].edited, get_proType_str(str, pageTable[i][j].proType),
			pageTable[i][j].count, pageTable[i][j].auxAddr);
	}
}}

/* ��ȡҳ�汣�������ַ��� */
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
//�������256λ�ַ���
fwrite(buffer,sizeof(BYTE),VIRTUAL_MEMORY_SIZE*2,ptr_auxMem);
/*
size_t fwrite(const void* buffer, size_t size,size_t count,FILE* stream)
*/
printf("ϵͳ��ʾ����ʼ������ģ���ļ����\n");
fclose(ptr_auxMem);
}
void do_print_res(){
    printf("%s ",actMem);
}
void do_print_file(){

    ptr_auxMem = fopen(AUXILIARY_MEMORY, "r+");
    char ch;
    while((ch=fgetc(ptr_auxMem))!=EOF)
        putchar(ch);
printf("\n");

}

int main(int argc, char* argv[])
{
	char c;
	int i,s;
	initfile();
	if (!(ptr_auxMem = fopen(AUXILIARY_MEMORY, "r+")))
	{
		do_error(ERROR_FILE_OPEN_FAILED);
		exit(1);
	}

	do_init();
	do_print_info();
	ptr_memAccReq = (Ptr_MemoryAccessRequest) malloc(sizeof(MemoryAccessRequest));
	/* ��ѭ����ģ��ô������봦����� */
	while (TRUE)
	{
		printf("�Ƿ��ֶ�����request����-1����-��������...\n");
		scanf("%d",&s);
		if((s==1))
           {
            do_request1();
           }
        else
		    do_request();
		do_response();
		c=getchar();
		printf("��Y��ӡҳ����z����ӡʵ�����ݣ���W��ӡ�������ݣ�������������ӡ...\n");
		if ((c = getchar()) == 'y' || c == 'Y')
			do_print_info();
        else if(c=='z'||c=='Z')
            do_print_res();
        else if(c=='w'||c=='W')
            do_print_file();
		while (c != '\n')
			c = getchar();
		printf("��X�˳����򣬰�����������...\n");
		if ((c = getchar()) == 'x' || c == 'X')
			break;
		while (c != '\n')
			c = getchar();
		//sleep(5000);
	}

	if (fclose(ptr_auxMem) == EOF)
	{
		do_error(ERROR_FILE_CLOSE_FAILED);
		exit(1);
	}
	return (0);
}
