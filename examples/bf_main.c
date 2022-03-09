#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"

#define CALL_OR_DIE(call)             \
        {                             \
            BF_ErrorCode code = call; \
            if (code != BF_OK)        \
            {                         \
              BF_PrintError(code);    \
              exit(code);             \
            }                         \
        }


int main() 
{
    int fd1;
    BF_Block *block;
    BF_Block_Init(&block);

    CALL_OR_DIE(BF_Init(LRU));
    CALL_OR_DIE(BF_CreateFile("data1.db"))
    CALL_OR_DIE(BF_OpenFile("data1.db", &fd1));

    char* data;
    for (int i = 0; i < 1000; ++i) 
    {
      CALL_OR_DIE(BF_AllocateBlock(fd1, block));

      data = BF_Block_GetData(block);
      memset(data, i % 127, BF_BLOCK_SIZE);
      BF_Block_SetDirty(block);

      CALL_OR_DIE(BF_UnpinBlock(block));
    }

    for (int i = 0; i < 1000; ++i) 
    {
      CALL_OR_DIE(BF_GetBlock(fd1, i, block));

      data = BF_Block_GetData(block);
      printf("block = %d and data = %d\n", i, data[0]);

      CALL_OR_DIE(BF_UnpinBlock(block));
    }

    CALL_OR_DIE(BF_CloseFile(fd1));
    CALL_OR_DIE(BF_Close());



    CALL_OR_DIE(BF_Init(LRU));
    CALL_OR_DIE(BF_OpenFile("data1.db", &fd1));
    int blocks_num;
    CALL_OR_DIE(BF_GetBlockCounter(fd1, &blocks_num));

    for (int i = 0; i < blocks_num; ++i) 
    {
      CALL_OR_DIE(BF_GetBlock(fd1, i, block));

      data = BF_Block_GetData(block);
      printf("block = %d and data = %d\n", i, data[i % BF_BUFFER_SIZE]);

      CALL_OR_DIE(BF_UnpinBlock(block));
    }

    BF_Block_Destroy(&block);
    CALL_OR_DIE(BF_CloseFile(fd1));
    CALL_OR_DIE(BF_Close());

	return 0;
}