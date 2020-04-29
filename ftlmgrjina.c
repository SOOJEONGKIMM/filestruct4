// 주의사항
// 1. blkmap.h에 정의되어 있는 상수 변수를 우선적으로 사용해야 함
// 2. blkmap.h에 정의되어 있지 않을 경우 본인이 만들어서 사용하면 됨

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include "blkmap.h"
// 필요한 경우 헤더 파일을 추가하시오.

extern FILE *devicefp;
int dd_read(int ppn, char *pagebuf);
int dd_write(int ppn, char *pagebuf);
int dd_erase(int pbn);
void printAll();
// flash memory를 처음 사용할 때 필요한 초기화 작업, 예를 들면 address mapping table에 대한
// 초기화 등의 작업을 수행한다. 따라서, 첫 번째 ftl_write() 또는 ftl_read()가 호출되기 전에
// main()함수에서 반드시 먼저 호출이 되어야 한다.

int addr_mapping_table[BLOCKS_PER_DEVICE];
int invaildPBN[BLOCKS_PER_DEVICE];	//physical block이 사용중인지 체크하는 배열
int free_block_idx = BLOCKS_PER_DEVICE - 1;

/*void print(int lpn)	//test 코드
{
	int lbn = (lpn / PAGES_PER_BLOCK);
	int pbn = addr_mapping_table[lbn];
	printf("========lbn: %d, pbn: %d==========\n", lbn, pbn);
	for(int i = 0;i < PAGES_PER_BLOCK;i++){
		char pagebuf[PAGE_SIZE] = {0};
		char spare_area[SPARE_SIZE] = {0};
		dd_read(pbn * PAGES_PER_BLOCK + i, pagebuf);
		memcpy(spare_area, pagebuf + SECTOR_SIZE, SPARE_SIZE);
		if(!strcmp(spare_area, "0"))
			printf("[  NULL  ]\n");
		else
			printf("[   %s   ]\n",pagebuf);
	}
}

void printAll()
{
	for(int i = 0;i < BLOCKS_PER_DEVICE;i++){
		printf("========lbn: %d, pbn: %d==========\n", i, addr_mapping_table[i]);
		for(int j = 0;j < PAGES_PER_BLOCK;j++){
			char pagebuf[PAGE_SIZE] = {0};
			char spare_area[SPARE_SIZE] = {0};
			dd_read(addr_mapping_table[i] * PAGES_PER_BLOCK + j, pagebuf);
			memcpy(spare_area, pagebuf + SECTOR_SIZE, SPARE_SIZE);
			if(!strcmp(spare_area, "0"))
				printf("[  NULL  ]\n");
			else
				printf("[   %s   ]\n",pagebuf);
		}
	}
	printf("----------------------------------\n");
}*/

void ftl_open()
{
	//
	// address mapping table 생성 및 초기화 등을 진행
    // mapping table에서 lbn과 pbn의 수는 blkmap.h에 정의되어 있는 DATABLKS_PER_DEVICE
    // 수와 같아야 하겠지요? 나머지 free block 하나는 overwrite 시에 사용하면 됩니다.
		// pbn 초기화의 경우, 첫 번째 write가 발생하기 전을 가정하므로 예를 들면, -1로 설정을
    // 하고, 그 이후 필요할 때마다 block을 하나씩 할당을 해 주면 됩니다. 어떤 순서대로 할당하는지는
    // 각자 알아서 판단하면 되는데, free block들을 어떻게 유지 관리할 지는 스스로 생각해 보기
    // 바랍니다.

		memset(addr_mapping_table, -1, sizeof(addr_mapping_table));	//-1로 초기화
		memset(invaildPBN, -1, sizeof(invaildPBN));
		invaildPBN[free_block_idx] = 2;	 //free block 용도로 사용
		char spare_area[SPARE_SIZE] = "0";
		//spare_area -1로 초기화
		for(int i = 0; i < BLOCKS_PER_DEVICE; i++){
			for(int j = 0;j < PAGES_PER_BLOCK;j++){
				int ppn = i * PAGES_PER_BLOCK + j;
				fseek(devicefp, ppn * PAGE_SIZE + SECTOR_SIZE, SEEK_SET);
				fwrite(spare_area, SPARE_SIZE, 1, devicefp);
			}
		}
		return;
}

//
// file system이 ftl_write()를 호출하면 FTL은 flash memory에서 주어진 lsn과 관련있는
// 최신의 데이터(512B)를 읽어서 sectorbuf가 가리키는 곳에 저장한다.
// 이 함수를 호출하기 전에 이미 sectorbuf가 가리키는 곳에 512B의 메모리가 할당되어 있어야 한다.
// 즉, 이 함수에서 메모리를 할당받으면 안된다.
//
void ftl_read(int lsn, char *sectorbuf)
{
	//** lbn과 offset값구하기
	int lbn = lsn / PAGES_PER_BLOCK, offset = lsn % PAGES_PER_BLOCK;
	if(lbn >= BLOCKS_PER_DEVICE || lbn < 0)	return ;	//lbn값 오류일때 바로 반환

	int pbn = addr_mapping_table[lbn];	//pbn값 구하기
	if(pbn == -1)	return ;	//할당된 pbn이 없던가 사용중이지 않은 lsn이면 return

	char pagebuf[PAGE_SIZE] = {0};
	int ppn = pbn * PAGES_PER_BLOCK + offset;
	dd_read(ppn, pagebuf);
	strncpy(sectorbuf, pagebuf, SECTOR_SIZE);
	return;
}

//
// file system이 ftl_write()를 호출하면 FTL은 flash memory에 sectorbuf가 가리키는 512B
// 데이터를 저장한다. 당연히 flash memory의 어떤 주소에 저장할 것인지는 block mapping 기법을
// 따라야한다.
//
void ftl_write(int lsn, char *sectorbuf)
{
	//** lbn과 offset값구하기
	int lbn = lsn / PAGES_PER_BLOCK, offset = lsn % PAGES_PER_BLOCK;

	if(lbn >= BLOCKS_PER_DEVICE || lbn < 0)	return ;	//lbn값 오류일때 바로 반환

	int* pbn = &addr_mapping_table[lbn];
	if(*pbn == -1){		//free block이라면
		for(int i = 0;i < BLOCKS_PER_DEVICE;i++){
			if(invaildPBN[i] == -1){	//사용가능한 pbn번호를 할당한 뒤 write해준다.
				*pbn = i, invaildPBN[*pbn] = 1;
				int ppn = (*pbn) * PAGES_PER_BLOCK + offset;

				char tempbuf[PAGE_SIZE] = {0};
				char spare_area[SPARE_SIZE] = "1";
				memcpy(tempbuf, sectorbuf, SECTOR_SIZE);
				memcpy(tempbuf+SECTOR_SIZE, spare_area, SPARE_SIZE);
				dd_write(ppn, tempbuf);
				return;
			}
		}
	}

	int ppn = (*pbn) * PAGES_PER_BLOCK + offset;
	char pagebuf[PAGE_SIZE] = {0};
	dd_read(ppn, pagebuf);
	char spare_area[SPARE_SIZE] = {0};
	memcpy(spare_area, pagebuf + SECTOR_SIZE, SPARE_SIZE);	//spare_area에 있는값 가져오기

	if(!strncmp(spare_area, "0", 1)){	//pbn은 이미 할당받은 상태이지만, page는 사용중이지 않을 때
		char tempbuf[PAGE_SIZE] = {0};
		strcpy(spare_area, "1");
		memcpy(tempbuf, sectorbuf, SECTOR_SIZE);
		memcpy(tempbuf+SECTOR_SIZE, spare_area, SPARE_SIZE);
		dd_write(ppn, tempbuf);

		return;
	}

	//overwrite일 경우
	for(int i = 0;i < PAGES_PER_BLOCK;i++){
		char tempbuf[PAGE_SIZE] = {0};	//변경하지 않는 값은 모두 copy
		if(offset == i)	continue;
		dd_read(*pbn * PAGES_PER_BLOCK + i, tempbuf);
		memcpy(spare_area, tempbuf + SECTOR_SIZE, SPARE_SIZE);
		if(!strncmp(spare_area, "0", 1))
			continue;	//아직 사용중이지 않은 위치는 건너뜀
		//	printf("%s %c\n", tempbuf, spare_area[0]);
		dd_write(free_block_idx * PAGES_PER_BLOCK + i, tempbuf);
	}

	ppn = free_block_idx * PAGES_PER_BLOCK + offset;
	char tempbuf[PAGE_SIZE] = {0};
	strcpy(spare_area, "1");
	memcpy(tempbuf, sectorbuf, SECTOR_SIZE);
	memcpy(tempbuf+SECTOR_SIZE, spare_area, SPARE_SIZE);
	dd_write(ppn, tempbuf);

	dd_erase(*pbn);

	strcpy(spare_area, "0");
	for(int i = 0; i < PAGES_PER_BLOCK; i++){
		int ppn = *pbn * PAGES_PER_BLOCK + i;
		fseek(devicefp, ppn * PAGE_SIZE + SECTOR_SIZE, SEEK_SET);
		fwrite(spare_area, SPARE_SIZE, 1, devicefp);
	}

	int temp = *pbn;
	*pbn = free_block_idx;
	free_block_idx = temp;

	invaildPBN[*pbn] = 1;
	addr_mapping_table[lbn] = *pbn;
	invaildPBN[temp] = 2;	//원래의 block free시키기

	return;
}
