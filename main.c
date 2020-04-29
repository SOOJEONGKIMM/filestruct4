#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include"sectormap.h"
#include<assert.h>

//제출용은 아니고 테스트용으로 만드는 파일임.

FILE *flashfp;

void ftl_open();
void ftl_write(int lsn, char *sectorbuf);
void ftl_read(int lsn, char *sectorbuf);
void ftl_print();

int main(int argc, char *argv[]){
    char *blockbuf;
    char sectorbuf[SECTOR_SIZE];
    int i;

    if((flashfp=fopen("flashmemory","w+b"))<0){
	fprintf(stderr,"fopen error\n");
	exit(1);
    }
    blockbuf=(char*)malloc(BLOCK_SIZE);
    memset(blockbuf,0xFF,BLOCK_SIZE);

    for(int i=0;i<BLOCKS_PER_DEVICE;i++){
	fwrite(blockbuf,BLOCK_SIZE,1,flashfp);
    }
    free(blockbuf);

//read write전에 항상 호출됨.
    ftl_open();
    ftl_write(3,"aaaaa");
    ftl_read(3,sectorbuf);
    ftl_print();
    ftl_write(4,"b");
    ftl_write(0,"bb");
    //ftl_read(0,sectorbuf);
    ftl_write(2,"c");
    ftl_print();
    printf("\n============\n");
    ftl_write(1,"d");
    ftl_write(7,"ee");
    ftl_write(8,"ddd");
    ftl_print();
    

    fclose(flashfp);

    return 0;


}

