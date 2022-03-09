#ifndef COMMON_H
#define COMMON_H

#include "hash_file.h"
#include "sht_file.h"

#define MAX_OPEN_FILES 20
#define member_size(type, member) sizeof(((type *)0)->member)
#define MAX_HASH_TABLE (BF_BLOCK_SIZE / sizeof(int))
#define true 1
#define false 0

typedef struct file{
	int used;
	char* filename;
    int location;
} File;

typedef struct sht_file{
	int used;
	int location;
} Sht_File;

#define HASH_ID "hash_id"
#define REAL_RECORD_SIZE (member_size(Record, id) + member_size(Record, name) + member_size(Record, surname) + member_size(Record, city))

#define SECONDARY_HASH_ID "secondary_hash_id"
#define REAL_SEC_RECORD_SIZE (member_size(SecondaryRecord, index_key) + member_size(SecondaryRecord, tupleId))

void memcpy_to_rec(Record* , char* );
void memcpy_rec(char* , Record );
void print_record(const void *);



#endif