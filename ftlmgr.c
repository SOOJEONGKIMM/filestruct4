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
static int cur_psn=0;
typedef struct garbage{//linked list for garbage block list
    int gdata;//ppn(psn)
    struct garbage *next;
}node;

node *head;//=(node*)malloc(sizeof(node));
node *cur_node;

// flash memory를 처음 사용할 때 필요한 초기화 작업, 예를 들면 address mapping table에 대한
// 초기화 등의 작업을 수행한다. 따라서, 첫 번째 ftl_write() 또는 ftl_read()가 호출되기 전에
// file system에 의해 반드시 먼저 호출이 되어야 한다.
//
void ftl_open()
{
    addTable=(int**)malloc(sizeof(int*)*2);
    addTable[0]=(int*)malloc(sizeof(int)*DATABLKS_PER_DEVICE*PAGES_PER_BLOCK);
    addTable[1]=(int*)malloc(sizeof(int)*DATABLKS_PER_DEVICE*PAGES_PER_BLOCK);
    
    for(int i=0;i<DATABLKS_PER_DEVICE*PAGES_PER_BLOCK;i++){
	addTable[1][i]=-1;//initialize pbn with 0xFF
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
    /*if(addTable[1][lsn]==-1){
	fprintf(stderr,"empty block\n");
	return;
    }*/
    //debug
    printf("%dPPPPP\n",psn);
    //lpn=lsn동일.
    int remain=lsn%PAGES_PER_BLOCK; //remainder
    memset(pagebuf,0xFF,PAGE_SIZE);
    //ppn=pbn*PAGES_PER_BLOCK + remain;
    dd_read(psn,pagebuf);
    memcpy(sectorbuf,pagebuf,PAGE_SIZE);

	//debug
    for(int i=0;i<PAGE_SIZE;i++){
	if(pagebuf[i]!=-1)
	    printf("%c",pagebuf[i]);
    }

    return;
}
 

void ftl_write(int lsn, char *sectorbuf)
{
    char sparebuf[SPARE_SIZE];
    char pagebuf[PAGE_SIZE];
    char *blockbuf;
    int lbn=lsn/PAGES_PER_BLOCK;
    int remain=lsn%PAGES_PER_BLOCK;
    int psn=addTable[1][lsn];
    int exist=1;//freeblock인지아닌지 판단위한 플래그//***매핑해서 사실상 필요없을듯..
    int pbn,ppn;

    //dd_read(ppn, pagebuf);

    
    //최초 쓰기 작업 수행인 경우(free block) 
    if(psn==-1){//psn 할당 전
	exist=0;
	//lpn=lbn*PAGES_PER_BLOCK+remain;
	addTable[1][lsn]=cur_psn;
	cur_psn++;
	if(cur_psn==DATABLKS_PER_DEVICE*PAGES_PER_BLOCK)
	    cur_psn=0;
	sparebuf[lsn]=lsn;//lsn(lpn)을 spare에 저장하는이유는 ppn이 lpn에 매핑되어있단걸 표현하기 위한것.
	//인자로 받은 sectorbuf pagebuf에 복사
        memcpy((char*)pagebuf,(char*)sectorbuf,strlen((char*)sectorbuf));
	memcpy((char*)(pagebuf+SECTOR_SIZE),(char*)sparebuf,strlen((char*)sparebuf));
	dd_write(psn,pagebuf);
	//fclose(flashfp);
	return;
    }//ppn 이미 할당되었고 페이지에 이미 데이터 존재한다면 update로 갱신
    else{//finding freeblock...플래그로 표시
	int fppn=0;//freeblock위치사용위해 
	exist=1;
	//ppn=pbn*PAGES_PER_BLOCK+remain;
	
	while(fppn < DATABLKS_PER_DEVICE*PAGES_PER_BLOCK){
	    for(int i=0;i<PAGES_PER_BLOCK;i++){
		dd_read(fppn*PAGES_PER_BLOCK+i,pagebuf);
		exist=0;
		for(int j=0;j<PAGES_PER_BLOCK;j++){
		    if(sparebuf[lsn]!=-1){//이미 매핑된경우
			exist=1;
			break;
		    }
		}
		if(exist==1)
		    break;
	    }
	    //free block찾은 후 기존데이터를 free block에 백업
	    //또한 아직 매핑되지않은 ppn인 경우 
	    if((exist==0)&&(sparebuf[lsn]==-1)){
	    	//dd_read(ppn,pagebuf);//read
		sparebuf[psn]=-1;//old block 처리. 
		node *newnode=(node*)malloc(sizeof(node));
		newnode->gdata=psn;
		if(head==NULL){
		    head=newnode;
		    cur_node=newnode;
		    newnode->next=NULL;
		}
		else{
		    cur_node->next=newnode;
		    cur_node=newnode;
		    newnode->next=NULL;
		}
		dd_write(fppn,pagebuf);//freeblock write
		cur_psn=fppn+1;
		if(cur_psn==DATABLKS_PER_DEVICE*PAGES_PER_BLOCK)
		    cur_psn=0;
	    
		//dd_erase(ppn);//기존데이터 기존자리에서 erase.

		//recopy and erase
		//dd_read(fppn,pagebuf);//freeblock에서 백업데이터 read 
		//lsn(lpn)을 spare에 저장하는이유는 ppn이 lpn에 매핑되어있단걸 표현하기 위한것.
		addTable[1][lsn]=fppn;
		sparebuf[fppn]=lsn;
		//인자로 받은 sectorbuf pagebuf에 복사
		memcpy((char*)pagebuf,(char*)sectorbuf,strlen((char*)sectorbuf));
		memcpy((char*)(pagebuf+SECTOR_SIZE),(char*)sparebuf,strlen((char*)sparebuf));
		//dd_write(fppn,pagebuf);//써야할자리에 write
		//dd_erase(fppn);//freeblock에 했던 백업데이터 erase.
		//fclose(flashfp);
		return;
	    }
	    //free block아니면 계속 찾기
	    else
		fppn++;
	}
	//(while문 다 돌았는데 여전히 못 찾음)
	//flashmemory이 다 차서 freeblock이 없는 경우라 garbage block할당필요
	if((exist==1)&&(psn!=-1)){
	    //garbage node 생성
	    //***    psn=head->gdata;//bring ppn
	    //cur_node=head;
	    //if(head->next==NULL)
		//free(head);
	   // else
		while(cur_node!=NULL && cur_node->next!=NULL)
		    head=cur_node->next;
		if(cur_node!=NULL)
		    head->next=cur_node->next;
		
		
	    free(cur_node);

	    pbn=psn/PAGES_PER_BLOCK;
	    ppn=pbn*PAGES_PER_BLOCK+remain;

	    for(int i=0;i<PAGES_PER_BLOCK;i++){
		if(ppn!=i){
		dd_read(pbn*PAGES_PER_BLOCK+i,pagebuf);
		dd_write(DATABLKS_PER_DEVICE*PAGES_PER_BLOCK+i,pagebuf);
		}
		else{
		sparebuf[DATABLKS_PER_DEVICE*PAGES_PER_BLOCK+i]=lsn;
		//인자로 받은 sectorbuf pagebuf에 복사
		memcpy((char*)pagebuf,(char*)sectorbuf,strlen((char*)sectorbuf));
		memcpy((char*)(pagebuf+SECTOR_SIZE),(char*)sparebuf,strlen((char*)sparebuf));
		}	    
	    }
	    dd_erase(pbn);
	    for(int i=0;i<PAGES_PER_BLOCK;i++){
		dd_read(DATABLKS_PER_DEVICE*PAGES_PER_BLOCK+i,pagebuf);
		dd_write(pbn*PAGES_PER_BLOCK+i,pagebuf);
	    }
	    addTable[1][psn]=lsn;
	}
    }
}

void ftl_print()
{
    printf("lpn ppn\n");
    for(int i=0;i<DATABLKS_PER_DEVICE*PAGES_PER_BLOCK;i++){
	printf("%d  %d\n",i,addTable[1][i]);
    }
    printf("free block's pbn=%d\n",DATABLKS_PER_DEVICE);

	return;
}
