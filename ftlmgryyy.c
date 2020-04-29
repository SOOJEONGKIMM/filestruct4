#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include "blkmap.h"

extern FILE *devicefp; // main.c의 FILE 구조체 포인터 devicefp를 사용
static int use;
static int ** bt; // address mapping table
int free_block; // 자유 블록 변수

int dd_read(int ppn,char * pagebuf);

int dd_write(int ppn,char * pagebuf);

int dd_erase(int pbn);

void ftl_open(void)
{
	int i;
	
	bt = (int **)malloc(sizeof(int *) * 2); // lbn과 pbn
	bt[0] = (int *)malloc(sizeof(int) * BLOCKS_PER_DEVICE); // lbn
	bt[1] = (int *)malloc(sizeof(int) * BLOCKS_PER_DEVICE); // pbn

	for(i = 0; i < BLOCKS_PER_DEVICE; i++)
	{
		bt[0][i] = i; // lbn은 각 블록 index로 초기화
		bt[1][i] = -1;
	}
	use = 0;
	free_block = DATABLKS_PER_DEVICE;
	bt[1][free_block] = DATABLKS_PER_DEVICE;
	return;
}

void ftl_read(int lsn, char *sectorbuf)
{
	int ppn;
	int lbn = lsn / PAGES_PER_BLOCK; // quotient
	int page = lsn % PAGES_PER_BLOCK; // remainder
	char pagebuf[PAGE_SIZE];

	if(bt[1][lbn] == -1)
	{
		fprintf(stderr,"requested block is empty...\n");
		exit(1);
	}
	ppn = bt[1][lbn] * PAGES_PER_BLOCK + page;
	memset((char *)pagebuf,0,PAGE_SIZE);
	memset((char *)sectorbuf,0,SECTOR_SIZE);
	dd_read(ppn,pagebuf);
	strncpy(sectorbuf,pagebuf,SECTOR_SIZE);
	return;
}

void ftl_write(int lsn, char *sectorbuf)
{
	int i,j;
	int ppn,fpn;
	int lbn = lsn / PAGES_PER_BLOCK; // quotient
	int page = lsn % PAGES_PER_BLOCK; // remainder
	char pagebuf[PAGE_SIZE];
	char spare[SPARE_SIZE];

	memset((char *)pagebuf,0,PAGE_SIZE);
	memset((char *)spare,0,SPARE_SIZE);

	if(bt[1][lbn] == -1) // 비어있다면
	{
		ppn = use * PAGES_PER_BLOCK + page;
		memcpy((char *)pagebuf,(char *)sectorbuf,SECTOR_SIZE);
		spare[0] = '1'; spare[1] = '\0';
		memcpy((char *)(pagebuf+SECTOR_SIZE),(char *)spare,SPARE_SIZE);
		dd_write(ppn,pagebuf);
		bt[1][lbn] = use++;
	}
	else // 비어있지 않다면
	{
		ppn = bt[1][lbn] * PAGES_PER_BLOCK + page;
		dd_read(ppn,pagebuf);
		memcpy((char *)pagebuf,(char *)sectorbuf,SECTOR_SIZE);
		memcpy((char *)spare,(char *)(pagebuf+SECTOR_SIZE),SPARE_SIZE);
		if(!strcmp(spare,"1"))
		{
			fpn = bt[1][free_block] * PAGES_PER_BLOCK;
			i = fpn;
			j = ppn - page;
			while(i < fpn + PAGES_PER_BLOCK) // 해당 페이지 빼고 전부 옮기기
			{
				char temp[PAGE_SIZE];
				memset((char *)spare,0,SPARE_SIZE);
				memset((char *)temp,0,PAGE_SIZE);
				dd_read(j,temp);
				memcpy((char *)spare,(char *)(temp+SECTOR_SIZE),SPARE_SIZE);

				if(i == fpn + page)
				{
					memcpy((char *)temp,(char *)sectorbuf,SECTOR_SIZE);
					spare[0] = '1'; spare[1] = 0;
					memcpy((char *)(temp+SECTOR_SIZE),(char *)spare,SPARE_SIZE);
					dd_write(i,temp);
					i++; j++;
					continue;
				}
				if(!strcmp(spare,"1"))
				{
					dd_write(i,temp);
				}
				i++; j++;
			}
			int swap = bt[1][lbn];
			bt[1][lbn] = bt[1][free_block];
			bt[1][free_block] = swap;
			dd_erase(swap);
		}
		else
		{
			spare[0] = '1'; spare[1] = 0;
			memcpy((char *)(pagebuf+SECTOR_SIZE),(char *)spare,SPARE_SIZE);
			dd_write(ppn,pagebuf);
		}
	}
	return;
}
