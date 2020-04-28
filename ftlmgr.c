// 주의사항
// 1. sectormap.h에 정의되어 있는 상수 변수를 우선적으로 사용해야 함
// 2. sectormap.h에 정의되어 있지 않을 경우 본인이 이 파일에서 만들어서 사용하면 됨
// 3. 필요한 data structure가 필요하면 이 파일에서 정의해서 쓰기 바람(sectormap.h에 추가하면 안됨)

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include "sectormap.h"
// 필요한 경우 헤더 파일을 추가하시오.
extern FILE *flashfp;
int dd_read(int ppn, char *pagebuf);
int dd_write(int ppn, char *pagebuf);
int dd_erase(int pbn);

int addTable[1][BLOCKS_PER_DEVICE];//address mapping table
int free_block;
// flash memory를 처음 사용할 때 필요한 초기화 작업, 예를 들면 address mapping table에 대한
// 초기화 등의 작업을 수행한다. 따라서, 첫 번째 ftl_write() 또는 ftl_read()가 호출되기 전에
// file system에 의해 반드시 먼저 호출이 되어야 한다.
//
void ftl_open()
{
    memset(addTable,0xFF,sizeof(addTable));
    for(int i=0;i<DATABLKS_PER_DEVICE;i++){
	addTable[0][i]=i;//lbn index 순서대로 채우기.
    }    
    free_block=DATABLKS_PER_DEVICE;//flashmemory가 full인경우대비해서 freeblock이 마지막에존재.
    addTable[1][free_block]=0xFF;//가장마지막블록을 freeblock pbn로 지정,pbn initialize
	//
	// address mapping table 초기화
	// free block's pbn 초기화
    	// address mapping table에서 lbn 수는 DATABLKS_PER_DEVICE 동일

    //debug
    for(int i=0;i<BLOCKS_PER_DEVICE;i++){
    printf("%d",addTable[0][i]);
    printf("%d",addTable[1][i]);
    }
    return;
}

//
// 이 함수를 호출하기 전에 이미 sectorbuf가 가리키는 곳에 512B의 메모리가 할당되어 있어야 한다.
// 즉, 이 함수에서 메모리를 할당받으면 안된다.
//
void ftl_read(int lsn, char *sectorbuf)
{
    char pagebuf[PAGE_SIZE];
    int ppn;
    int lbn=lsn/PAGES_PER_BLOCK; //quotient
    //lpn=lsn동일.
    int remain=lsn%PAGES_PER_BLOCK; //remainder
    memset(pagebuf,0xFF,PAGE_SIZE);
    ppn=pbn*PAGES_PER_BLOCK + remain;
    dd_read(ppn,pagebuf);
    memcpy(sectorbuf,pagebuf,PAGE_SIZE);

    return;
}


void ftl_write(int lsn, char *sectorbuf)
{


	return;
}

void ftl_print()
{
    printf("lpn ppn\n");
    for(int i=0;i<BLOCKS_PER_DEVICE;i++){
	printf("%d  %d\n",lpn,ppn);
    }
    printf("free block's pbn=%d",addTable[1][free_block]);

	return;
}
