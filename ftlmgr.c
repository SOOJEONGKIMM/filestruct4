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

static int ** addTable;
//[1][BLOCKS_PER_DEVICE];//address mapping table
int free_block;

// flash memory를 처음 사용할 때 필요한 초기화 작업, 예를 들면 address mapping table에 대한
// 초기화 등의 작업을 수행한다. 따라서, 첫 번째 ftl_write() 또는 ftl_read()가 호출되기 전에
// file system에 의해 반드시 먼저 호출이 되어야 한다.
//
void ftl_open()
{
    addTable=(int**)malloc(sizeof(int*)*3);
    addTable[0]=(int*)malloc(sizeof(int)*DATABLKS_PER_DEVICE*PAGES_PER_BLOCK);
    addTable[1]=(int*)malloc(sizeof(int)*DATABLKS_PER_DEVICE*PAGES_PER_BLOCK);
    addTable[2]=(int*)malloc(sizeof(int)*DATABLKS_PER_DEVICE*PAGES_PER_BLOCK);
    
    for(int i=0;i<DATABLKS_PER_DEVICE*PAGES_PER_BLOCK;i++){
	addTable[1][i]=-1;//initialize pbn with 0xFF
	addTable[2][i]=-1;//initialize  with 0xFF
	addTable[0][i]=i;//lbn index 순서대로 채우기.
    }    
    
    free_block=DATABLKS_PER_DEVICE;
     //flashmemory가 full인경우대비해서 freeblock이 마지막에존재.
    //addTable[1][free_block]=DATABLKS_PER_DEVICE;
    //가장마지막블록을 freeblock pbn로 지정
	// address mapping table 초기화
	// free block's pbn 초기화
    	// address mapping table에서 lbn 수는 DATABLKS_PER_DEVICE 동일

    return;
}

//
// 이 함수를 호출하기 전에 이미 sectorbuf가 가리키는 곳에 512B의 메모리가 할당되어 있어야 한다.
// 즉, 이 함수에서 메모리를 할당받으면 안된다.
//
void ftl_read(int lsn, char *sectorbuf)
{
    char pagebuf[PAGE_SIZE];
    int lbn=lsn/PAGES_PER_BLOCK; //quotient
    int psn=addTable[1][lsn];
    //lpn=lsn동일.
    int remain=lsn%PAGES_PER_BLOCK; //remainder
    memset(pagebuf,0xFF,PAGE_SIZE);
    memset(sectorbuf,0xFF,SECTOR_SIZE);
    dd_read(psn,pagebuf);
    strncpy(sectorbuf,pagebuf,SECTOR_SIZE);

    return;
}
 

void ftl_write(int lsn, char *sectorbuf)
{
    char sparebuf[SPARE_SIZE];
    char pagebuf[PAGE_SIZE];
    char *blockbuf;
    int lbn=lsn/PAGES_PER_BLOCK;
    int remain=lsn%PAGES_PER_BLOCK;
    int exist=1;//freeblock인지아닌지 판단위한 플래그
    int pbn,ppn,psn;
    printf("\nFTL_WRITE() CALLED\n");

    memset(pagebuf,0xFF,PAGE_SIZE);
    memset(sparebuf,0xFF,SPARE_SIZE);
    
    //최초 쓰기 작업 수행인 경우(free block) 
    if(addTable[2][lsn]==-1){//psn 할당 전
	exist=0;
	memset(sparebuf,0xFF,SPARE_SIZE);
	sprintf(sparebuf,"%d",lsn);
	//인자로 받은 sectorbuf pagebuf에 복사
        memcpy((char*)pagebuf,(char*)sectorbuf,strlen((char*)sectorbuf));
	memset(sparebuf+8,0xFF,SPARE_SIZE-8);
	memcpy((char*)(pagebuf+SECTOR_SIZE),(char*)sparebuf,SPARE_SIZE);
	dd_write(lsn,pagebuf);

	addTable[1][lsn]=lsn;
	addTable[2][lsn]=1;
	printf("FIRST WRITE IN FREEBLOCK\n");
	return;
    }//ppn 이미 할당되었고 페이지에 이미 데이터 존재한다면 update로 갱신
    else if(addTable[1][lsn]!=-1||addTable[2][lsn]==1){//finding freeblock...플래그로 표시
	printf("FINDING FREEBLOCK...\n");
	int fppn=0;//freeblock위치사용위해 
	//spare에 적혀있는 맵핑번호 비교확인 위해 
	char tmppage[PAGE_SIZE];
	exist=0;
	while(fppn < DATABLKS_PER_DEVICE*PAGES_PER_BLOCK){
	    if(addTable[2][fppn]!=-1)
		exist=1;
	    if(addTable[2][fppn]==-1)
		exist=0;
	    //free block찾은 후 기존데이터를 free block에 백업
	    //또한 아직 매핑되지않은 ppn인 경우 
	    if(exist==0){
		printf("FOUND FREEBLOCK AND WRITES\n");
		printf("\nFPPN:%d\n ",fppn);//debug
		//int fpsn;
		//int fpbn=fppn/PAGES_PER_BLOCK;
		
		psn = addTable[1][lsn];
		addTable[2][psn]=0; // invlaid page
		addTable[2][fppn]=1;
		printf("\nDEBUG:%d lsn is invalid,so [2][%d]=1 \n",lsn,fppn);
		addTable[1][lsn]=fppn;
		//remain=lsn%PAGES_PER_BLOCK;
		//fpsn=fppn*PAGES_PER_BLOCK+remain;
		sparebuf[fppn]=lsn;
		memset(sparebuf,0xFF,SPARE_SIZE);
		sprintf(sparebuf,"%d",fppn);//***근데 이 경우 스페어에 lsn쓰는건지 잘모르겠음
		memset((char*)pagebuf,0XFF,PAGE_SIZE);
		//인자로 받은 sectorbuf pagebuf에 복사
		memcpy((char*)pagebuf,(char*)sectorbuf,strlen((char*)sectorbuf));
		memset((char*)sparebuf+8,0XFF,SPARE_SIZE-8);
		memcpy((char*)(pagebuf+SECTOR_SIZE),(char*)sparebuf,strlen((char*)sparebuf));
		dd_write(fppn,pagebuf);//써야할자리에 write

		return;
	    }
	    //free block아니면 계속 찾기
	    else
		fppn++;
	}
	//(while문 다 돌았는데 여전히 못 찾음)
	//flashmemory이 다 차서 freeblock이 없는 경우라 garbage block할당필요
	if((exist==1)&&(addTable[1][lsn]!=-1)){
	    printf("ENTERED GARBAGE\n");
	    pbn=psn/PAGES_PER_BLOCK;
	    ppn=pbn*PAGES_PER_BLOCK+remain;
	    for(int j=0;j<DATABLKS_PER_DEVICE;j++){
	    
		for(int i=0;i<PAGES_PER_BLOCK;i++){
		    //copy valid page
		    if(addTable[2][j*PAGES_PER_BLOCK+i] ==1){
			dd_read(j*PAGES_PER_BLOCK+i,pagebuf);
			dd_write(DATABLKS_PER_DEVICE*PAGES_PER_BLOCK+i,pagebuf);
		    }
		}

		dd_erase(j);
		for(int i=0;i<PAGES_PER_BLOCK;i++){
		    //recopy valid page
		    if(addTable[2][j*PAGES_PER_BLOCK+i] ==1){
			dd_read(DATABLKS_PER_DEVICE*PAGES_PER_BLOCK+i,pagebuf);
			dd_write(j*PAGES_PER_BLOCK+i,pagebuf);
		    }
		    if(addTable[2][j*PAGES_PER_BLOCK+i]==0)
			addTable[2][j*PAGES_PER_BLOCK+i]=-1;
		}

	    }
	    int full=1;
	    for(int i=0;i<PAGES_PER_BLOCK*DATABLKS_PER_DEVICE;i++){
		if(addTable[2][i]==-1){
		    psn = i;
		    full=0;
		    break;
		}
	    }
	    if(full==0){
		addTable[1][psn]=lsn;
		addTable[2][lsn]=1;
		sprintf(sparebuf,"%d",lsn);
		memset((char*)pagebuf,0xFF,PAGE_SIZE);
		//인자로 받은 sectorbuf pagebuf에 복사
		memcpy((char*)pagebuf,(char*)sectorbuf,strlen((char*)sectorbuf));
		memset((char*)sparebuf+8,0XFF,SPARE_SIZE-8);
		memcpy((char*)(pagebuf+SECTOR_SIZE),(char*)sparebuf,strlen((char*)sparebuf));
		    
		dd_write(psn,pagebuf);
	    }
	    else{
		printf("WARNING: MEMORY IS FULL!");
		return;
	    }
	}
    }
}

void ftl_print()
{
    printf("lpn ppn\n");
    for(int i=0;i<DATABLKS_PER_DEVICE*PAGES_PER_BLOCK;i++){
	printf("%d  %d  %d\n",i,addTable[1][i],addTable[2][i]);
    }
    printf("free block's pbn=%d\n",DATABLKS_PER_DEVICE);

	return;
}
