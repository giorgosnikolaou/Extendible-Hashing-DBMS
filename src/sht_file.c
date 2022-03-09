#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>
#include "common.h"
#include "bf.h"
#include "sht_file.h"

#include <stdarg.h>


#define CALL_BF(call)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {         \
    BF_PrintError(code);    \
    return HT_ERROR;        \
  }                         \
}

#define CALL_HT(call)     				\
		{                           	\
			HT_ErrorCode code = call;	\
			if (code != HT_OK) {      	\
			fprintf(stderr, "Error\n");	\
			exit(code);             	\
			}                         	\
		}



Sht_File open_files2[MAX_OPEN_FILES];
int sht_records_per_block = -1;
int sht_number_of_indexes = -1;

extern File open_files[MAX_OPEN_FILES];
extern int records_per_block ;
extern int number_of_indexes;


static void memcpy_secondary_rec(char* data, SecondaryRecord rec) {
	int offset = 0;

	for (int i = 0, len = strlen(rec.index_key); i < member_size(SecondaryRecord, index_key); i++, offset++)
		if (i < len)
			data[offset] = rec.index_key[i];
		else
			data[offset] = '\0';
	memcpy(data + offset, &rec.tupleId, sizeof(int));
	
}


static void memcpy_to_secondary_rec(SecondaryRecord* rec, char* data) {
	int offset = 0;

	for (int i = 0; i < member_size(SecondaryRecord, index_key); i++, offset++) 
		rec->index_key[i] = data[offset];
	
	memcpy(&rec->tupleId, data + offset, sizeof(rec->tupleId));
}

//based on dan bernstein's djb 2 a hash function known to work great for strings
static uint32_t string_hash(char* item, int bits) {

    uint32_t hash = 5381;

	for (char* str = item; *str != '\0'; str++)
		hash = ((hash << 5) + hash) + *str; 

	return hash >> (sizeof(hash) * 8 - bits);

}

//returns the index of the file given by bf 
//primary location takes the index of the file in open_files
static int find_primary(const char* secondary , int *primary_location) {
	for (int i = 0 ; i < MAX_OPEN_FILES; i++) {
	
	if (open_files[i].used == true && !strcmp(open_files[i].filename , secondary )) {
		*primary_location = i;
		return open_files[i].location;
		} 
	}
	return -1;
}

//returns the attribute name a secondary index is created on
//gets the find_primary returned values on primary and primary location
static char* get_primary_info(char *data , int *primary , int *primary_location) {
	int off =  sizeof(SECONDARY_HASH_ID) + sizeof(int) + sizeof(int) , count = 0 ;
	char *temp = data + off;
	while (*temp++ !='\0' ) count++;
	char *attribute_name = malloc((count + 1) *sizeof(char));
	for (int i =0; i < count +1 ; i++) {
		attribute_name[i] = data[off + i];
	}
	int off1 = off + count + 1;
	*primary = find_primary(data + off1 , primary_location);
	return attribute_name;
}

//function to derefrence tupleId and get the primary file's record
static HT_ErrorCode get_primary_recrord(int tupleId , int indexDesc , Record *record , int records_offset) {
	BF_Block* block;
	BF_Block_Init(&block);
	int block_to_find = tupleId / records_per_block;
	CALL_BF(BF_GetBlock(indexDesc, block_to_find , block))
	char* data = BF_Block_GetData(block);
	CALL_BF(BF_UnpinBlock(block));
	int position = tupleId %  records_per_block;
	memcpy_to_rec(record, data + records_offset + position*REAL_RECORD_SIZE );
	BF_Block_Destroy(&block);
	return HT_OK;
}

//helper function for tests
HT_ErrorCode print_sec_file(int indexDesc)
{
	BF_Block* block;
	BF_Block_Init(&block);

	int gd = 0;
	CALL_BF(BF_GetBlock(open_files2[indexDesc].location, 0, block));
	
	char* data = BF_Block_GetData(block);
	memcpy(&gd, data + sizeof(SECONDARY_HASH_ID), sizeof(int));
	CALL_BF(BF_UnpinBlock(block));

	int num_blocks = 0;
	CALL_BF(BF_GetBlockCounter(open_files2[indexDesc].location, &num_blocks));

	// Print general information about the file structure
	printf("glopal depth: %d\n", gd);
	printf("hash indexs: %d\n", 1 << gd);
	printf("hash blocks: %ld\n\n", ((1 << gd ) - 1) / MAX_HASH_TABLE + 1);

	int count = 0;

	// Print the hash table, ie where each index is pointing to
	for (int i = 0; i < ((1 << gd ) - 1) / MAX_HASH_TABLE + 1; i++){
		
		CALL_BF(BF_GetBlock(open_files2[indexDesc].location, i + 1, block));
		char* data = BF_Block_GetData(block);
		uint32_t hash[MAX_HASH_TABLE];
		memcpy(hash, data, sizeof(hash));

		printf("|");

		for (int j = 0; j < MAX_HASH_TABLE; j++) {
			printf("%3d|", hash[j]);
			if (++count % 32 == 0)
				printf("\n%s", !(count == (((1 << gd ) - 1) / MAX_HASH_TABLE + 1) * MAX_HASH_TABLE) ? "|" : "");
		}
		
		printf("\t");
		
		CALL_BF(BF_UnpinBlock(block));
	}

	printf("\n");

	// For each bucket print its local depth, number of records in the bucket, prefix and the bytes that make the bitmap for the records
	// Then print the record, or FREE if there isnt a record in that place 
	for (int i = ((1 << gd ) - 1) / MAX_HASH_TABLE + 1 + 1; i < num_blocks; i++){
		
		CALL_BF(BF_GetBlock(open_files2[indexDesc].location, i, block));
		char* data = BF_Block_GetData(block);

		int ld = 0;
		memcpy(&ld, data, sizeof(int));

		int ins = 0;
		memcpy(&ins, data + sizeof(int), sizeof(int));

		uint8_t indexes[sht_number_of_indexes];
		memcpy(indexes, data + 2 * sizeof(int), sizeof(uint8_t) * sht_number_of_indexes);

		uint32_t prefix = 0;
		memcpy(&prefix, data + 2 * sizeof(int) + sizeof(indexes), sizeof(uint32_t));
		printf("%3d: %2d %d %3d, ", i, ld, ins, prefix);

		for (int k = 0; k < sht_number_of_indexes; k++)
			printf("%3d ", indexes[k]);

		printf("\n\t\t\t");

		for (int k = 0; k < sht_number_of_indexes; k++){
			for (int j = 0; j < 8 && (k * 8 + j) < sht_records_per_block; j++){
				if (((indexes[k] >> (7-j)) & 1) == 0){
					printf("FREE\n\t\t\t");
					continue;
				}

				SecondaryRecord rec;
				memcpy_to_secondary_rec(&rec, data + 2 * sizeof(int) + sizeof(indexes) + sizeof(uint32_t) + (k * 8 + j) * REAL_SEC_RECORD_SIZE);
				printf("%2s - %2d\n\t\t\t", rec.index_key, rec.tupleId);
			}
		}
		
		printf("\n");

		CALL_BF(BF_UnpinBlock(block));
	}

	BF_Block_Destroy(&block);
	printf("\n");
	
	return HT_OK;
}



HT_ErrorCode SHT_Init() {
  //insert code here
	
	for (size_t i = 0; i < MAX_OPEN_FILES; i++)
		open_files2[i].used = false;

	// Only do it the first time init is called
	if (sht_records_per_block < 0){
		// block size - header size
		size_t remaining = BF_BLOCK_SIZE - 2 * sizeof(int) - sizeof(int);

		// first evaluation of the # of buckets
		sht_records_per_block = remaining / REAL_SEC_RECORD_SIZE;

		// Bytes that we would need to store the indexes with the above # of buckets
		int indexes = sht_records_per_block / 8 + (sht_records_per_block % 8 != 0);

		// Bytes required for the buckets
		int buckets_size = sht_records_per_block * REAL_SEC_RECORD_SIZE;

		// Available bytes
		int av_bytes = remaining - buckets_size;

		// If available bytes are not enough substruct a record and update the values
		sht_records_per_block -= av_bytes < indexes;
		sht_number_of_indexes = indexes;
	}
	return HT_OK;
}

static HT_ErrorCode mass_insert(const char* source, const char* target, char *attrName){
	BF_Block* block;
	BF_Block_Init(&block);
	int primary_location;
	int primary = find_primary(source , &primary_location);
	if (primary == -1) return HT_ERROR;
	
	int secondary;
	SHT_OpenSecondaryIndex(target, &secondary);

	int primary_blocks_num ; 
	CALL_BF(BF_GetBlockCounter(primary, &primary_blocks_num));

 	CALL_BF(BF_GetBlock(primary, 0, block));
	char* primary_data  = BF_Block_GetData(block);

	int global_depth;
	memcpy(&global_depth, primary_data + sizeof(HASH_ID), sizeof(int));
	CALL_BF(BF_UnpinBlock(block));


	// Size is 2^depth
	int size = 1 << global_depth; 
	// Calculate the number of directory blocks that exist in the file
	int number_of_dir_blocks = (size / MAX_HASH_TABLE) > 1 ? size / MAX_HASH_TABLE : 1;
	int ident;
	int size_to_copy;

 	if (!strcmp(attrName, "surname")) { 
		ident = member_size(Record, id) + member_size(Record, name);
		size_to_copy = member_size(Record, surname);
	}
	else if (!strcmp(attrName , "city")) {
		ident = member_size(Record, id) + member_size(Record, name) + member_size(Record, surname); 
		size_to_copy = member_size(Record , city);
	}  
	int records_offset = 2 * sizeof(int) + sizeof(uint8_t) * number_of_indexes  + sizeof(uint32_t);
	for (int i = number_of_dir_blocks + 1 ; i < primary_blocks_num ; i++) {

		CALL_BF(BF_GetBlock(primary , i , block));

		primary_data = BF_Block_GetData(block);

		uint8_t indexes[number_of_indexes];
		memcpy(indexes, primary_data + 2 * sizeof(int), sizeof(uint8_t) * number_of_indexes);

		for (int k = 0; k < number_of_indexes; k++) {

			// check_bit starts from 10000000 and will help to check if a bit of the index is set or not
			uint8_t check_bit = 0x80;
			int record_index = 0;
			
			// Loop until copy index contains only zeros, thus there are no more records to print
			while (indexes[k]) {

				// If this bit is set we found a record
				if (indexes[k] & check_bit) {

					SecondaryRecord record;
					int position = records_offset + (k * 8 + record_index) * REAL_RECORD_SIZE + ident;  					
					memcpy(record.index_key , primary_data + position , size_to_copy);
						
					// We currently are on block: i
					// and record number of that data block: k*8 + record_index
					// and that's how we create the tuple id
					record.tupleId = i * records_per_block + (k * 8 + record_index);
					SHT_SecondaryInsertEntry(secondary, record);

					// Unset the bit in the index copy
					indexes[k] &= ~check_bit;
				}

				// Increase the record_index by 1 each time to calculate the offset properly
				// and get ready to check the next bit
				record_index++;
				check_bit >>= 1; 	
			}
		}

		CALL_BF(BF_UnpinBlock(block));

	}

	BF_Block_Destroy(&block);
	SHT_CloseSecondaryIndex(secondary);

	return HT_OK;
}

HT_ErrorCode SHT_CreateSecondaryIndex(const char *sfileName, char *attrName, int attrLength, int depth, const char *fileName ) {

	if (strcmp(attrName , "city") && strcmp(attrName , "surname")) {
		printf("Invalid attribute name %s\n" , attrName);
		return HT_ERROR;
	}
	CALL_BF(BF_CreateFile(sfileName));

	char* data;
	int indexDesc, empty = 0;

	// number of buckets will be 2^depth
	int number_of_buckets = 1 << depth;

	// Calculate the directory blocks that need to be allocated to host these buckets
	int blocks_to_allocate = number_of_buckets > MAX_HASH_TABLE ? number_of_buckets / MAX_HASH_TABLE : 1;
	
	// if we need more than 1 dir blocks every dir block will host MAX_HASH_TABLE buckets
	if (blocks_to_allocate > 1)
		number_of_buckets = MAX_HASH_TABLE;


	CALL_BF(BF_OpenFile(sfileName, &indexDesc));

	BF_Block* block;
	BF_Block_Init(&block);

	// Write the secondary hash identifier, the global depth, the attribute length and attribute name to the first block of the file
	CALL_BF(BF_AllocateBlock(indexDesc, block));
	data = BF_Block_GetData(block);
	memcpy(data, SECONDARY_HASH_ID , sizeof(SECONDARY_HASH_ID));
	memcpy(data + sizeof(SECONDARY_HASH_ID), &depth, sizeof(int));
	memcpy(data + sizeof(SECONDARY_HASH_ID) + sizeof(int) , &attrLength , sizeof(int));
	memcpy(data + sizeof(SECONDARY_HASH_ID) + sizeof(int) + sizeof(int) , attrName , strlen(attrName) +1);
	memcpy(data + sizeof(SECONDARY_HASH_ID) + sizeof(int) + sizeof(int) + strlen(attrName) +1 , fileName , strlen(fileName)+1);

	BF_Block_SetDirty(block);
	CALL_BF(BF_UnpinBlock(block));

	BF_Block* bucket;
	int hash_table[number_of_buckets];
	uint8_t index[sht_number_of_indexes];

	
	for (int i = 0; i < sht_number_of_indexes; i++)
		index[i] = 0x00;


	// Allocate every block needed to store the initial dictionary
	for (int i = 0; i < blocks_to_allocate; i++) {
		CALL_BF(BF_AllocateBlock(indexDesc, block)); 
		CALL_BF(BF_UnpinBlock(block));
	}


	BF_Block_Init(&bucket);

	int blocks;
	CALL_BF(BF_GetBlockCounter(indexDesc, &blocks));
	
	for (int j = 0; j < blocks_to_allocate; j++) {

		CALL_BF(BF_GetBlock(indexDesc, j + 1, block));
		data = BF_Block_GetData(block);

		for (int i = 0; i < number_of_buckets ; i++) {
			// Allocate a block that will store records
			CALL_BF(BF_AllocateBlock(indexDesc, bucket));

			// Write the local depth, amount of records, index array and prefix to the block (initialization)
			char* bucket_data = BF_Block_GetData(bucket);
			memcpy(bucket_data, &depth, sizeof(int));
			memcpy(bucket_data + sizeof(int), &empty, sizeof(int)); 
			memcpy(bucket_data + 2 * sizeof(int), &index, sizeof(index));

			int pointed_prefix = j * MAX_HASH_TABLE + i;
			memcpy(bucket_data + 2 * sizeof(int) + sizeof(index), &pointed_prefix, sizeof(int));
			
			// Store the location of the block to the array that will be written on the directory block
			hash_table[i] = ((i + blocks) + j * MAX_HASH_TABLE); 
			BF_Block_SetDirty(bucket);
			CALL_BF(BF_UnpinBlock(bucket));
		}
		
		memcpy(data, hash_table, sizeof(hash_table));

		BF_Block_SetDirty(block);
		CALL_BF(BF_UnpinBlock(block));
	}


	BF_Block_Destroy(&block);
	BF_Block_Destroy(&bucket);
	CALL_BF(BF_CloseFile(indexDesc));

	// Initialize the values with values from a primary hashing file
	mass_insert(fileName, sfileName, attrName);

  return HT_OK;
}

HT_ErrorCode SHT_OpenSecondaryIndex(const char *sfileName, int *indexDesc  ) {
  
  	int loc;
	CALL_BF(BF_OpenFile(sfileName, &loc));
	

	// Assert the file given is a hash file this module made
	BF_Block* block;
	BF_Block_Init(&block);

	CALL_BF(BF_GetBlock(loc, 0, block));

	char* data = BF_Block_GetData(block);

	// Copy the bytes that should make the HASH_ID into a char array so we can use strcmp to check if it is correct
	// We do this this way in order to have a more general approach 
	// i.e. to be able to change only the HASH_ID when we want a new hash identifier
	size_t length = strlen(SECONDARY_HASH_ID);
	char str[length + 1];
	str[length] = '\0';

	for (size_t i = 0; i < length; i++)
		str[i] = data[i];


	// We don't need to set the block as dirty since we didn't alter its content
	CALL_BF(BF_UnpinBlock(block));
	BF_Block_Destroy(&block);

	if (strcmp(str, SECONDARY_HASH_ID)) {
		CALL_BF(BF_CloseFile(loc));
		return HT_ERROR;
	}

	// Find first avalaible index to "store" the open file
	for (size_t i = 0; i < MAX_OPEN_FILES; i++) {
		if (!open_files2[i].used) {
			// Cell is used
			open_files2[i].used = true;

			// Store the location returned from the bf module to the struct
			open_files2[i].location = loc;

			// "Return" the appropriate index of the open_files array
			*indexDesc = i;

			return HT_OK;		
		}
	}

	// Didn't find an empty spot, return error
	CALL_BF(BF_CloseFile(loc));
	return HT_ERROR;
	
}

HT_ErrorCode SHT_CloseSecondaryIndex(int indexDesc) {
  
	assert(indexDesc >= 0 && indexDesc < 20);

	// If there is a file open in the corresponding array index
	if (open_files2[indexDesc].used) {

		// Close the file
		CALL_BF(BF_CloseFile(open_files2[indexDesc].location));

		// Cell no longer used
		open_files2[indexDesc].used = false;

		return HT_OK;			
	}

	return HT_ERROR;
}


static int* find_prefixes(int prefix, int prefix_bits, int bits) {
	// Arguments are: Stored prefix, local depth, global depth
	// Every buddy bucket has the same "local depth" MSBs
	// Therefore by having these bits (prefix), the local depth (prefix_bits) and the global depth (bits)
	// we can find the range of the indexes of the directory that point to the bucket
	// Example prefix is 9 (001001 since we need 6 bits we include two 0s at the front) local depth is 6 and global depth is 8
	// All the indexes for which their MSBs are 001001XX point to the same bucket
	// Therefore the first index is when all X are 0 and last index is when all X are 1
	// That can be easily calculated by a simple shift for the first number
	// and by adding to it a number of (global depth - local depth) bits where each bit is 1 for the last number 
	// This number is created by substracting 1 from 2 raised to the power of (global depth - local depth + 1)

	int* array = malloc(sizeof(int) * 4);
	int max = (0xFFFFFFFF & (1 << (bits - prefix_bits))) - 1;

	array[0] = prefix << (bits - prefix_bits);
	array[3] = array[0] | max;
	array[1] = (array[0] + array[3]) / 2;
	array[2] = array[1] + 1;

	return array;
}

static HT_ErrorCode expand_dir(int indexDesc, int global_depth) {

	// Calculate current and new index count, directory blocks needed
	size_t indexes = 1 << global_depth; 
 	size_t index_blocks = (indexes / MAX_HASH_TABLE > 1 )? indexes / MAX_HASH_TABLE : 1;

	size_t new_indexes = indexes << 1; 
	size_t new_index_blocks = (new_indexes / MAX_HASH_TABLE > 1 ) ? new_indexes / MAX_HASH_TABLE : 1;
	
	int size = (new_index_blocks == 1) ? new_indexes : MAX_HASH_TABLE;
	uint32_t hashtable[size];


	// Expansion of directory => global depth increases by 1
	global_depth++;

	BF_Block* block;
	BF_Block_Init(&block);

	CALL_BF(BF_GetBlock(indexDesc, 0, block));
	char* data = BF_Block_GetData(block);
	memcpy(data + sizeof(SECONDARY_HASH_ID), &global_depth, sizeof(int));
	BF_Block_SetDirty(block);
	CALL_BF(BF_UnpinBlock(block));

	CALL_BF(BF_GetBlock(indexDesc, 1, block));
	data = BF_Block_GetData(block);


	BF_Block* bucket;
	BF_Block_Init(&bucket);

	BF_Block* bucket2;
	BF_Block_Init(&bucket2);
	
	// For every new directory block needed move one data block to the end of the file
	// This is done to have all directory blocks continuously and thus have a faster hash find
	// Since expansion of the directory is an uncommon action this does not affect the performance of the module much relative to the increased hash find performance
	for (size_t i = 0, end = new_index_blocks - index_blocks; i < end; i++) {
		// Allocate new block to copy the data block to
		CALL_BF(BF_GetBlock(indexDesc, i + index_blocks + 1, bucket));
		char* bucket_data = BF_Block_GetData(bucket);
		
		CALL_BF(BF_AllocateBlock(indexDesc, bucket2));
		char* bucket_data2 = BF_Block_GetData(bucket2);
		
		// Copy data block to the new block
		memcpy(bucket_data2, bucket_data, BF_BLOCK_SIZE);
		
		BF_Block_SetDirty(bucket2);
		CALL_BF(BF_UnpinBlock(bucket2));

		// Clear the current directory block (probably not needed)
	 	memset(bucket_data, 0, BF_BLOCK_SIZE);


		BF_Block_SetDirty(bucket);
		CALL_BF(BF_UnpinBlock(bucket));
	}

	int num_blocks = 0;
	CALL_BF(BF_GetBlockCounter(indexDesc, &num_blocks));

	int last_dir = -1;
	char* bucket_data2 = NULL;
	int wrote = 0;

	// For every data block (bucket) calculate the indexes of the directory that should be pointing to it 
	// and update the directory accordingly
	for (size_t i = new_index_blocks + 1, end = num_blocks, offset = 2 * sizeof(int) + sizeof(uint8_t) * sht_number_of_indexes; i < end; i++) {
		CALL_BF(BF_GetBlock(indexDesc, i, bucket));
		char* bucket_data = BF_Block_GetData(bucket);
		
		int depth = 0;
		memcpy(&depth, bucket_data, sizeof(int));

		int prefix = 0;
		memcpy(&prefix, bucket_data + offset, sizeof(int));

		int* buckets = find_prefixes(prefix, depth, global_depth);

		for (int j = buckets[0], end = buckets[3]; j <= end; j++) {
			int dir = j / MAX_HASH_TABLE ;

			// If we find a new directory block write the old one to the file and get the data for the new one
			// "bucket_data2 != NULL" asserts we don't do it the first time we enter this loop since we don't
			// have an "old" directory block yet
			if (dir != last_dir) {
				last_dir = dir;

				if (bucket_data2 != NULL) {
					memcpy(bucket_data2, hashtable, sizeof(hashtable));
					BF_Block_SetDirty(bucket2);
					CALL_BF(BF_UnpinBlock(bucket2));
				}

				CALL_BF(BF_GetBlock(indexDesc, dir + 1, bucket2));
				bucket_data2 = BF_Block_GetData(bucket2);
				memcpy(hashtable, bucket_data2, sizeof(hashtable));
			}

			wrote = dir != last_dir;
			hashtable[j % MAX_HASH_TABLE] = i;
		}

		free(buckets);
		
		CALL_BF(BF_UnpinBlock(bucket));
	} 

	// If we didn't write a directory block to the file on the last iteration of the loop we need to write it here
	if (!wrote) {
		memcpy(bucket_data2, hashtable, sizeof(hashtable));
		BF_Block_SetDirty(bucket2);
	}
		
	CALL_BF(BF_UnpinBlock(bucket));
		
	CALL_BF(BF_UnpinBlock(bucket2));

	BF_Block_Destroy(&block);
	BF_Block_Destroy(&bucket);
	BF_Block_Destroy(&bucket2);

	return HT_OK;
}

static HT_ErrorCode split_bucket(int indexDesc, int bucket_num, int global_depth) {
	
	// Split a data block
	// New block goes to the end of the file to improve performance 
	int size_of_bucket_header = 2 * sizeof(int) + sht_number_of_indexes * sizeof(uint8_t) + sizeof(int);


	BF_Block* bucket;
	BF_Block_Init(&bucket);

	CALL_BF(BF_GetBlock(indexDesc, bucket_num, bucket));
	char* bucket_data = BF_Block_GetData(bucket);
	
	int depth = 0;
	memcpy(&depth, bucket_data, sizeof(int));

	int prefix = 0;
	memcpy(&prefix, bucket_data + 2 * sizeof(int) + sizeof(uint8_t) * sht_number_of_indexes, sizeof(int));

	int* buckets = find_prefixes(prefix, depth, global_depth);

	BF_Block* new_bucket;
	BF_Block_Init(&new_bucket);


	CALL_BF(BF_AllocateBlock(indexDesc, new_bucket));
	char* new_data = BF_Block_GetData(new_bucket);

	// For every record on bucket if its hash is in [buckets[0], buckets[1]] it stays there
	// else goes to new bucket and we just remove the 1 bit from index

	uint8_t indexes[sht_number_of_indexes];
	memcpy(indexes, bucket_data + 2 * sizeof(int), sizeof(indexes));

	uint8_t new_indexes[sht_number_of_indexes];
	memcpy(new_indexes, new_data + 2 * sizeof(int), sizeof(new_indexes));

	int inserted = 0;

	for (int i = 0; i < sht_number_of_indexes; i++) {
		// For all bits check if any is 1 => the bytes of the record corresponding there are used 
		for (int j = 7; j >= 0 && (i * 8 + 7 - j) < sht_records_per_block; j--) {
			
			if ( ((indexes[i] >> j) & 1 ) == 1) {
			
				SecondaryRecord record;
				memcpy_to_secondary_rec(&record, bucket_data + size_of_bucket_header + (i * 8 + 7 - j) * REAL_SEC_RECORD_SIZE);
				
				
				uint32_t hash = string_hash(record.index_key, global_depth);

				// If its hash corresponds to the new bucket put it there
				// We don't remove the record from the old block, we just set its index to 0 to improve performance
				if (hash >= buckets[2]) {
					
					// Update the index char to account for the lost entry
					indexes[i] &= ~(1 << j);

					// Write the new index char over the old one 
					memcpy(new_data + 2 * sizeof(int), indexes, sizeof(indexes));

					// Write every field of the record to the file on the appropriate position of the new block
					memcpy_secondary_rec(new_data + size_of_bucket_header + inserted * REAL_SEC_RECORD_SIZE, record);
					
					inserted++;

				}
			}
		}
	}
	
	int number_of_records = 0;
	memcpy(&number_of_records, bucket_data + sizeof(int), sizeof(int));

	// Write the new number of records stored over the original block 
	number_of_records -= inserted;
	memcpy(bucket_data + sizeof(int), &number_of_records, sizeof(int));
	memcpy(bucket_data + 2 * sizeof(int), indexes, sizeof(indexes));

	// Write # of records stored to the new block 
	memcpy(new_data + sizeof(int), &inserted, sizeof(int));
	
	// Update the index char to include the new entries
	for (size_t i = 0; i < sht_number_of_indexes && inserted > 0; i++) {
		for (int j = 7; j >= 0 && inserted > 0; j--, inserted--)
			new_indexes[i] |= (1 << j);
	}

	// Write the new index bytes to the new block
	memcpy(new_data + 2 * sizeof(int), new_indexes, sizeof(new_indexes));

	// Local depth is one more than the original's bucket local depth for both blocks
	depth++;
	memcpy(bucket_data, &depth, sizeof(int));
	memcpy(new_data, &depth, sizeof(int));

	// New prefixes are: old prefix with 0 added to the end	for the old block
	//					 old prefix with 1 added to the end for the new block
	int new_prefix = (prefix << 1) | 1;
	prefix <<= 1;
	memcpy(bucket_data + 2 * sizeof(int) + sizeof(uint8_t) * sht_number_of_indexes, &prefix, sizeof(int));
	memcpy(new_data + 2 * sizeof(int) + sizeof(uint8_t) * sht_number_of_indexes, &new_prefix, sizeof(int));


	BF_Block_SetDirty(bucket);
	BF_Block_SetDirty(new_bucket);
		
	CALL_BF(BF_UnpinBlock(bucket));
	CALL_BF(BF_UnpinBlock(new_bucket));


	// Update indexes

	int last_dir = -1;
	char* bucket_data2 = NULL;
	int wrote = 0;
	int num_blocks = 0;
	CALL_BF(BF_GetBlockCounter(indexDesc, &num_blocks));
		
	uint32_t hashtable[MAX_HASH_TABLE];

	// Same concept as on expand but this time we don't have to update them for all the buckets 
	// but just for the ones for the new block
	for (int bucket = buckets[2], end = buckets[3]; bucket <= end; bucket++) {
		int dir = bucket / MAX_HASH_TABLE;
		
		if (dir != last_dir) {
			last_dir = dir;

			if (bucket_data2 != NULL) {
				memcpy(bucket_data2, hashtable, sizeof(hashtable));
				BF_Block_SetDirty(new_bucket);
		
				CALL_BF(BF_UnpinBlock(new_bucket));
			}
			
			CALL_BF(BF_GetBlock(indexDesc, dir + 1, new_bucket));
			bucket_data2 = BF_Block_GetData(new_bucket);
			memcpy(hashtable, bucket_data2, sizeof(hashtable));
		}

		wrote = dir != last_dir;
		hashtable[bucket % MAX_HASH_TABLE] = num_blocks - 1;
	}
	
	if (!wrote) {
		memcpy(bucket_data2, hashtable, sizeof(hashtable));
		BF_Block_SetDirty(new_bucket);
		CALL_BF(BF_UnpinBlock(new_bucket));
	}

		
	BF_Block_Destroy(&bucket);
	BF_Block_Destroy(&new_bucket);

	free(buckets);

	return HT_OK;
}


// indexDesc is the index on the open_files2 array
HT_ErrorCode SHT_SecondaryInsertEntry (int indexDesc, SecondaryRecord record  ) {

	// If file is not opened or index is out of range, return error
	if (indexDesc >= 0 && indexDesc <= MAX_OPEN_FILES && !open_files2[indexDesc].used)
		return HT_ERROR;

	BF_Block* block;
	char* data ;
	int depth ;

	BF_Block_Init(&block);

	int bf_index = open_files2[indexDesc].location;
	
	CALL_BF(BF_GetBlock(bf_index, 0, block));
	data = BF_Block_GetData(block);
	memcpy(&depth, data + sizeof(SECONDARY_HASH_ID), sizeof(int));
	CALL_BF(BF_UnpinBlock(block));

	uint32_t hash = string_hash(record.index_key, depth);
	

	int number_of_records, local_depth, i, j;
	int block_to_write;
	int block_to_get = hash / MAX_HASH_TABLE + 1; 

	int num_blocks = 0;
	CALL_BF(BF_GetBlockCounter(bf_index, &num_blocks));
	CALL_BF(BF_GetBlock(bf_index, block_to_get, block));
	data = BF_Block_GetData(block);
	memcpy(&block_to_write, data + ( (hash % MAX_HASH_TABLE) * sizeof(int)), sizeof(int));
	CALL_BF(BF_UnpinBlock(block));

	CALL_BF(BF_GetBlock(bf_index, block_to_write, block));
	data = BF_Block_GetData(block);
	memcpy(&number_of_records, data + sizeof(int), sizeof(int));
	CALL_BF(BF_UnpinBlock(block));

	// Bucket has space for the record => insert it there
	if (number_of_records < sht_records_per_block) {	
		
		uint8_t index[sht_number_of_indexes];

		CALL_BF(BF_GetBlock(bf_index, block_to_write, block));
		data = BF_Block_GetData(block);
		int prefix = 0;
		memcpy(&prefix, data + 2 * sizeof(int) + sizeof(uint8_t) * sht_number_of_indexes, sizeof(int));
		
		memcpy(index, data + 2 * sizeof(int), sizeof(index));

		// Find first empty spot
		for (i = 0; i < sht_number_of_indexes; i++) {
			// For all bits check if any is 0 => the bytes of the record corresponding there aren't used 
			for (j = 7; j >= 0; j--) {
				if ( (index[i] & ( 1 << j)) == 0 )
					break;
			}

			if (j != -1) 
				break;
		}
	
		// Write the new number of records stored over the old one 
		number_of_records++;
		memcpy(data + sizeof(int), &number_of_records, sizeof(int));
		
		// Update the index byte to include the new entry
		index[i] |= (1 << j);
		// Write the new index bytes over the old
		memcpy(data + 2 * sizeof(int), index, sizeof(uint8_t) * sht_number_of_indexes);

		// Write every field of the record to the file on the appropriate position of the block
		memcpy_secondary_rec(data + 2 * sizeof(int) + sizeof(uint8_t) * sht_number_of_indexes + sizeof(int) + (i * 8 + 7 - j) * REAL_SEC_RECORD_SIZE, record);

		BF_Block_SetDirty(block);
		CALL_BF(BF_UnpinBlock(block));
		BF_Block_Destroy(&block);
		return HT_OK;

	}
	
	// Bucket doesn't have space for the record and we need to split the bucket or expand the directory 
	CALL_BF(BF_GetBlock(bf_index, block_to_write, block));
	data = BF_Block_GetData(block);
	memcpy(&local_depth, data, sizeof(int));
	CALL_BF(BF_UnpinBlock(block));

	// Pointers pointing to bucket are more than 1 and therefore bucket can be split
	if (local_depth < depth) {
		if (split_bucket(bf_index, block_to_write, depth) == HT_ERROR)
			return HT_ERROR;
	// Directory needs expansion
	} else {
		if (expand_dir(bf_index, depth) == HT_ERROR)
			return HT_ERROR;
	}
	
	BF_Block_Destroy(&block);

	// Either we split a bucket and we will insert the record 
	// or we expanded the directory and we will now split a bucket and finally insert the record 
	return SHT_SecondaryInsertEntry(indexDesc, record);

}

// sindexDesc is the index of the file on the bf level
static HT_ErrorCode update_record(int sindexDesc, int depth, tuple tuple)
{
	BF_Block* dir_block;
	BF_Block_Init(&dir_block);
	BF_Block* data_block;
	BF_Block_Init(&data_block);

	CALL_BF(BF_GetBlock(sindexDesc, 0, dir_block));
	char* data = BF_Block_GetData(dir_block);


	char attrName[20];
	memcpy(attrName , data + sizeof(SECONDARY_HASH_ID) + 2* sizeof(int)  , sizeof(attrName));
	int  attr_size = 0;
	uint32_t hash = 0;


	if (!strcmp(attrName, "surname")) { 
		attr_size = member_size(Record, surname);
		hash = string_hash(tuple.surname, depth);
	}
	else if (!strcmp(attrName , "city")) { 
		attr_size = member_size(Record , city);
		hash = string_hash(tuple.city, depth);
	}  

	int dir_block_to_get = hash / MAX_HASH_TABLE + 1;

	int position = hash % MAX_HASH_TABLE;
	int block_to_find = 0;

	CALL_BF(BF_UnpinBlock(dir_block));
	CALL_BF(BF_GetBlock(sindexDesc, dir_block_to_get, dir_block));
	data = BF_Block_GetData(dir_block);

	// Find the block that contains the records with this hash and get it
	memcpy(&block_to_find, data + position * sizeof(int), sizeof(int));
	
	CALL_BF(BF_UnpinBlock(dir_block));
	CALL_BF(BF_GetBlock(sindexDesc, block_to_find, data_block));
	char* records_block_data = BF_Block_GetData(data_block);

	// Iterate over the indexes of the block
	// Each index is an 8bit map that shows if a spot contains an entry or not
	uint8_t indexes[sht_number_of_indexes];
	memcpy(&indexes , records_block_data + 2*sizeof(int) , sizeof(uint8_t) * sht_number_of_indexes);
	int records_offset = 2*sizeof(int) + sizeof(uint8_t) * sht_number_of_indexes + sizeof(int);
	for (int k = 0; k < sht_number_of_indexes; k++) {
				
		
		// check_bit starts from 10000000 and will help to check if a bit of the index is set or not
		uint8_t check_bit = 0x80;
		int record_index = 0;

		// Loop until copy index contains only zeros, thus there are no more records to print
		while (indexes[k]) {
			
			// if this bit is set we found a record
			if (indexes[k] & check_bit) {

				// Get its record id to see if it is the record we are looking for
				// Every index is of size 8 so the record's offset is (k * 8 + record_index ) * REAL_RECORD_SIZE 
				int id;
				memcpy(&id , records_block_data + records_offset + (k*8 + record_index ) *REAL_SEC_RECORD_SIZE + attr_size, sizeof(int));
				
				if (tuple.oldTupleId == id) {
					memcpy(records_block_data + records_offset + (k*8 + record_index ) *REAL_SEC_RECORD_SIZE + attr_size , &(tuple.newTupleId) , sizeof(int));
					break;
				}


			// Unset the bit in the index copy
			indexes[k] &= ~check_bit;
			}

			// Increase the record_index by 1 each time to calculate the offset properly and get ready to check the next bit
			record_index++;
			check_bit >>= 1;	
		}
	}
	CALL_BF(BF_UnpinBlock(dir_block));
	CALL_BF(BF_UnpinBlock(data_block));
	BF_Block_Destroy(&dir_block);
	BF_Block_Destroy(&data_block);

	return HT_OK;

}


HT_ErrorCode SHT_SecondaryUpdateEntry (int indexDesc, UpdateRecordArray *updateArray ) {
	

	if (!updateArray->changed_tuples)
		return HT_OK;

	BF_Block* dir_block;
	BF_Block_Init(&dir_block);


	int bf_index = open_files2[indexDesc].location;

	// Get the depth from the info block
	CALL_BF(BF_GetBlock(bf_index, 0, dir_block));
	char* data = BF_Block_GetData(dir_block);

	int depth;
	memcpy(&depth, data + sizeof(SECONDARY_HASH_ID), sizeof(int));

	CALL_BF(BF_UnpinBlock(dir_block));
	BF_Block_Destroy(&dir_block);


	for (size_t i = 0; i < updateArray->tuples_changed; i++)
		update_record(bf_index, depth, updateArray->changed_tuples[i]);

  	return HT_OK;
}


static void simple_print(char* data  , int* count , char* none , int indexDesc,int depth)  {

	(*count )++;
	int tupleId;
	memcpy(&tupleId , data +member_size(SecondaryRecord , index_key) , sizeof(int));

	Record record;
	get_primary_recrord(tupleId , indexDesc , &record , 2*sizeof(int) + sizeof(uint8_t)*number_of_indexes +sizeof(int) );
	
	printf("Record id : %-6d", record.id );
	printf("Record name : %-15s", record.name );
	printf("Record surname : %-20s", record.surname );
	printf("Record city : %-20s\n", record.city );
}

static void compare_and_print(char* data , int *found , char*index_key , int indexDesc , int depth) {
	
	int size = member_size(SecondaryRecord, index_key);
	char rec_index_key[size];
	memcpy(&rec_index_key, data, size);
				
					
	if (!strcmp( index_key , rec_index_key)) {
		
		(*found) ++;
		int tupleId;
		memcpy(&tupleId , data +member_size(SecondaryRecord , index_key) , sizeof(int));

		Record record;
		get_primary_recrord(tupleId , indexDesc , &record , 2*sizeof(int) + sizeof(uint8_t)*number_of_indexes +sizeof(int) );

		printf("Record id : %-6d", record.id );
		printf("Record name : %-15s", record.name );
		printf("Record surname : %-20s", record.surname );
		printf("Record city : %-20s\n", record.city );

	}
}


static void iterate_print(char* records_block_data , int* count , char* index_key ,int indexDesc , int depth,  void (*fun_ptr) (char* , int* , char* , int ,int)  ){
	uint8_t indexes[sht_number_of_indexes];
	int records_offset = 2*sizeof(int) + sizeof(uint8_t) * sht_number_of_indexes + sizeof(int);
	memcpy(&indexes , records_block_data + 2*sizeof(int) , sizeof(uint8_t) * sht_number_of_indexes);
	for (int k = 0 ; k < sht_number_of_indexes; k++) {

		// Copy the k -th index to a variable			
					
		// check_bit starts from 10000000 and will help to check if a bit of the index is set or not
		uint8_t check_bit = 0x80;
		int record_index = 0;
					
		// Loop until copy index contains only zeros, thus there are no more records to print
		while (indexes[k]) {

			// If this bit is set we found a record
			if (indexes[k] & check_bit) {

				// Every index is of size 8 so the record's offset is (k * 8 + record_index ) * REAL_RECORD_SIZE 
				//print_record(records_block_data + records_offset + (k*8 + record_index) *REAL_SEC_RECORD_SIZE);
				fun_ptr(records_block_data + records_offset + (k*8 + record_index) *REAL_SEC_RECORD_SIZE ,count , index_key , indexDesc , depth);
				// Unset the bit in the index copy
				indexes[k] &= ~check_bit;
			}

			// Increase the record_index by 1 each time to calculate the offset properly
			// and get ready to check the next bit
			record_index++;
			check_bit >>= 1; 	
		}
	}
}


HT_ErrorCode SHT_PrintAllEntries(int sindexDesc, char *index_key ) {
  //insert code here
	int depth ;
	BF_Block* dir_block, *data_block;
	BF_Block_Init(&data_block);
	BF_Block_Init(&dir_block);

	sindexDesc = open_files2[sindexDesc].location;
	
	// Get the depth from the info block
	CALL_BF(BF_GetBlock(sindexDesc, 0, dir_block));
	char* data = BF_Block_GetData(dir_block), *records_block_data;
	
	memcpy(&depth, data + sizeof(SECONDARY_HASH_ID), sizeof(int));

	int primary , primary_file_location; 
	char* attribute_name =  get_primary_info(data , &primary , &primary_file_location);
	if (primary == -1) return HT_ERROR;

	CALL_BF(BF_UnpinBlock(dir_block));
	// Size is 2^depth
	int size = 1 << depth; 

	// Calculate the number of directory blocks that exist in the file
	int number_of_dir_blocks = (size / MAX_HASH_TABLE) > 1 ? size / MAX_HASH_TABLE : 1;


	// We print every entry in this case
	if (index_key == NULL) {
		int count = 0;

		// Iterate over the directory blocks
		
		int blocks_num;
		CALL_BF(BF_GetBlockCounter(sindexDesc, &blocks_num));
		for (int i = number_of_dir_blocks + 1; i < blocks_num ; i++) {
			
			BF_GetBlock(sindexDesc, i, data_block);
			records_block_data = BF_Block_GetData(data_block);
		
			iterate_print(records_block_data , &count  , NULL , primary , depth, &simple_print );
			CALL_BF(BF_UnpinBlock(data_block));
		}

		printf("File : %s , Printed %d records\n",  open_files[primary_file_location].filename , count);
	
	}else{

		// We look for a record in this case
		// Check the record's hash and calculate which dir block should contain it
		uint32_t hash = string_hash(index_key, depth);
		int dir_block_to_get = hash / MAX_HASH_TABLE + 1;

		int position = hash % MAX_HASH_TABLE;
		int block_to_find;

		CALL_BF(BF_GetBlock(sindexDesc, dir_block_to_get, dir_block));
		char* data = BF_Block_GetData(dir_block);

		// Find the block that contains the records with this hash and get it
		memcpy(&block_to_find, data + position * sizeof(int), sizeof(int));
		
		CALL_BF(BF_UnpinBlock(dir_block));
		CALL_BF(BF_GetBlock(sindexDesc, block_to_find, data_block));
		records_block_data = BF_Block_GetData(data_block);


		// found will hold the number of times we found the record with id = *id in the file
		int found = 0; 

		// Iterate over the indexes of the block
		// Each index is an 8bit map that shows if a spot contains an entry or not
		iterate_print(records_block_data , &found , index_key ,primary , depth , &compare_and_print);

		printf("File : %s , Found %d records with %s = %s\n" , open_files[primary_file_location].filename ,  found , attribute_name , index_key);	
		
		CALL_BF(BF_UnpinBlock(data_block));
	}

	BF_Block_Destroy(&dir_block);
	BF_Block_Destroy(&data_block);
	free(attribute_name);
	return HT_OK;
  
}


HT_ErrorCode SHT_HashStatistics(char *filename ) {
	//insert code here

  	// Open file with name "filename"
	int file_desc;
	CALL_BF(BF_OpenFile(filename, &file_desc));

	// Get number of blocks in file and print it
	int blocks_num;
	CALL_BF(BF_GetBlockCounter(file_desc, &blocks_num));

	printf("File %s has %d blocks\n", filename, blocks_num);

	// Get block 0 of file to read global depth
	BF_Block* block;
	BF_Block_Init(&block);
	CALL_BF(BF_GetBlock(file_desc, 0, block));

	// Get block's 0 data
	char* data;
	data = BF_Block_GetData(block);

	// Global depth is written after HASH_ID and has size of an int
	int* global_depth_pointer = malloc(sizeof(int));
	memcpy(global_depth_pointer, data+sizeof(SECONDARY_HASH_ID), sizeof(int));

	int global_depth = *global_depth_pointer;

	// Unpin and destroy block
	CALL_BF(BF_UnpinBlock(block));
	BF_Block_Destroy(&block);

	// Calculate hash table size according to global depth
	uint32_t hash_table_size = 1;
	
	uint32_t size = 1 << global_depth;
	uint32_t number_of_dir_blocks = (size / MAX_HASH_TABLE) > 1 ? size / MAX_HASH_TABLE : 1;
	
	if (number_of_dir_blocks == 1) hash_table_size = size; 
	else hash_table_size = MAX_HASH_TABLE;

	// Initialize a new block, which will be used to take block's 1 data, which contains the hash table
	BF_Block_Init(&block);

	CALL_BF(BF_GetBlock(file_desc, 1, block));

	// Get block's 1 data
	data = BF_Block_GetData(block);

	int min = INT_MAX;
	int max = 0;
	int sum = 0;

	// Iterate through the blocks that form the dictionary
	for (int j = number_of_dir_blocks + 1; j <  blocks_num; j++) {

		// Initialize a new block which will be used to get information of every bucket (one at each iteration)
		BF_Block* block_bucket;
		BF_Block_Init(&block_bucket);

		// Get the block which contains the information for the desired bucket
		CALL_BF(BF_GetBlock(file_desc, j, block_bucket));

		// Get the data of this block
		char* bucket_data;
		bucket_data = BF_Block_GetData(block_bucket);

		// Memory address [bucket_data, bucket_data+sizeof(int)] contains local depth of the bucket, so we ignore it
		// We get the next sizeof(int), which contains the number of elements in the bucket
		int* bucket_elements = malloc(sizeof(int));
		memcpy(bucket_elements, bucket_data+sizeof(int), sizeof(int));

		// Unpin and destroy the new block used for the bucket
		CALL_BF(BF_UnpinBlock(block_bucket));
		BF_Block_Destroy(&block_bucket);

		// Add the bucket's elements to sum and check if a new min or max is found
		sum += *bucket_elements;
		if (*bucket_elements < min)
			min = *bucket_elements;
		if (*bucket_elements > max)
			max = *bucket_elements;

		// Free malloc'd variables used before
		free(bucket_elements);

	}

	// Print results
	printf("Minimum records in a bucket accross the file are: %d\n"
	 "Average records in a bucket accross the file are: %lf\n"
	 "Maximum records in a bucket accross the file are: %d\n", min, (double)sum/(hash_table_size *number_of_dir_blocks), max);

	// Free malloc'd variables allocated in the beginning
	free(global_depth_pointer);

	// Unpin and destroy block used for hash table retrieval
	CALL_BF(BF_UnpinBlock(block));
	BF_Block_Destroy(&block);

	// Close file
	CALL_BF(BF_CloseFile(file_desc));
	
	return HT_OK;
}

// Returns data of a block pointed to by the hash function for a given secondary hash table, index key and depth
static HT_ErrorCode get_data(int depth, char* index_key, int sindexDesc, char** data) {
	BF_Block *block;
	BF_Block_Init(&block);
	uint32_t hash = string_hash(index_key, depth);

	int dir_block_to_get = hash / MAX_HASH_TABLE + 1;

	int position = hash % MAX_HASH_TABLE;

	CALL_BF(BF_GetBlock(sindexDesc, dir_block_to_get, block));
	*data = BF_Block_GetData(block);
	int block_to_find;

	// Find the block that contains the records with this hash and get it
	memcpy(&block_to_find, *data + position * sizeof(int), sizeof(int));
	CALL_BF(BF_UnpinBlock(block));
	CALL_BF(BF_GetBlock(sindexDesc, block_to_find, block));

	*data = BF_Block_GetData(block);
	CALL_BF(BF_UnpinBlock(block));
	BF_Block_Destroy(&block);

	return HT_OK;
}

static void interate_join(char *data, char *index_key, int* count, int tupleIds[sht_records_per_block]) {

	// Records in every block exist after local depth, number of records, indexes of existence and prefix
	int records_offset = 2*sizeof(int) + sizeof(uint8_t) * sht_number_of_indexes + sizeof(int);
	uint8_t indexes[sht_number_of_indexes];

	// Indexes of the block exist after local depth and number of records
	memcpy(&indexes, data + 2*sizeof(int), sizeof(uint8_t) * sht_number_of_indexes);

	// For every index
	for (int k = 0; k < sht_number_of_indexes; k++) {

		// check_bit starts from 10000000 and will help to check if a bit of the index is set or not
		uint8_t check_bit = 0x80;
		int record_index = 0;
		int size = member_size(SecondaryRecord, index_key);
		
		// Loop until copy index contains only zeros, thus there are no more records to print
		while (indexes[k]) {

			// If this bit is set we found a record
			if (indexes[k] & check_bit) {

				// Allocate space to copy index key
				// data points to space address of the block, we add records_offset to bypass header
				// Every index is an 8bitmap and record_index shows whether the current record exists or nor
				char rec_index_key[size];
				memcpy(&rec_index_key, data + records_offset + (k*8 + record_index) *REAL_SEC_RECORD_SIZE, size);
		
				if (!strcmp(index_key , rec_index_key)) {
					// Allocate space to copy index id and save it on tupleIds array
					int new ;
					memcpy(&new , data + records_offset + (k*8 + record_index) *REAL_SEC_RECORD_SIZE + member_size(SecondaryRecord , index_key) , sizeof(int));
					tupleIds[*count] = new;
					(*count)++;
				}
				// Unset the bit in the index copy
				indexes[k] &= ~check_bit;
			}

			// Increase the record_index by 1 each time to calculate the offset properly
			// and get ready to check the next bit
			record_index++;
			check_bit >>= 1; 	
		}
	}
}

// a function to concatenate two strings without changing them
static char *cat(const char* a , const char* b) {

	int l1 = strlen(a) , l2 = strlen(b);
	static char conc[256];
	int i , j;

	for (i =0; i< l1 ; i++) 
		conc[i] = a[i];

	for (j = 0; j < l2 ; j++) 
		conc[i+j] = b[j];
	conc[l1+l2] = '\0';

	return conc;
}

static void print_style(int style , Record record) {
	const int dont_print_city = 1;
	if (style == dont_print_city) {
		printf("|%-19d|%-19s|%-19s" ,   record.id , record.name , record.surname );
	}
	else {
		printf("|%-19d|%-19s|%-19s" ,   record.id , record.name , record.city );
	}
}

static void print_joined(int style1 , int style2 , Record record1 , Record record2) {

	if (style1) {
		printf("|%-19s" , record1.city);
	}
	else {
		printf("|%-19s" , record1.surname);
	}
	if (style2) {
		printf("|%-19s" , record2.city);
	}
	else {
		printf("|%-19s" , record2.surname);
	}
	print_style(style1 , record1);
	print_style(style2 , record2);
	printf("\n");
}

static void print_title(char *attribute_name1 , char* attribute_name2 , int primary1_file_location , int primary2_file_location) {
	

	printf("|%-19s", cat(cat(open_files[primary1_file_location].filename , "."), attribute_name1) );
	printf("|%-19s", cat(cat(open_files[primary2_file_location].filename , "."), attribute_name2) );
	printf("|%-19s" , cat(open_files[primary1_file_location].filename , ".id") );
	printf("|%-19s", cat(open_files[primary1_file_location].filename , ".name"));

	if (!strcmp(attribute_name1  , "city")) {
		printf("|%-19s", cat(open_files[primary1_file_location].filename , ".surname")  );
	}
	else {
		printf("|%-19s", cat(open_files[primary1_file_location].filename , ".city") );
	}

	printf("|%-19s" , cat(open_files[primary2_file_location].filename , ".id") );
	printf("|%-19s", cat(open_files[primary2_file_location].filename , ".name"));

	if (!strcmp(attribute_name2 , "city") ){
		printf("|%-19s\n", cat(open_files[primary2_file_location].filename , ".surname") );
	}
	else {
		printf("|%-19s\n", cat(open_files[primary2_file_location].filename , ".city") );
	}

	printf("|%-19s|%-19s|%-19s|%-19s" , " ", " ", " ", " " );
	printf("|%-19s|%-19s|%-19s|%-19s\n" ," ", " " , " " , " " );
}

HT_ErrorCode SHT_InnerJoin(int sindexDesc1, int sindexDesc2,  char *index_key ) {
	//insert code here
	
	// assert that the two files given are open
	if (open_files2[sindexDesc1].used != true || open_files2[sindexDesc2].used !=true) return HT_ERROR;
	
	int depth1, depth2, primary1, primary2;
	BF_Block* block;
	BF_Block_Init(&block);

	////////////////////////////// SECONDARY 1 //////////////////////////////

	// Get depth of secondary1 and info of primary1 hash table
	int count_result = 0;
	sindexDesc1 = open_files2[sindexDesc1].location;
	CALL_BF(BF_GetBlock(sindexDesc1, 0, block));
	char* data = BF_Block_GetData(block);
	memcpy(&depth1, data + sizeof(SECONDARY_HASH_ID), sizeof(int));
	int primary1_file_location, primary2_file_location;
	char *atrribute_name1 = get_primary_info(data , &primary1 , &primary1_file_location);

	if (primary1 == -1) return HT_ERROR;

	CALL_BF(BF_UnpinBlock(block));

	////////////////////////////// SECONDARY 2 //////////////////////////////

	// Get depth of secondary1 and info of primary1 hash table

	sindexDesc2 = open_files2[sindexDesc2].location;
	CALL_BF(BF_GetBlock(sindexDesc2, 0, block));
	data = BF_Block_GetData(block);
	memcpy(&depth2, data + sizeof(SECONDARY_HASH_ID), sizeof(int));
	char *attribute_name2 = get_primary_info(data , &primary2 , &primary2_file_location);
	if (primary2 == -1) return HT_ERROR;

	CALL_BF(BF_UnpinBlock(block));

	//variable that decides the way results are printed according to attribute name	
	int print_city1 = (strcmp(atrribute_name1 , "city") == 0) ;
	int print_city2 = (strcmp(attribute_name2 , "city") == 0) ;

	if (index_key != NULL) {
		char *data1, *data2;
		
		// Get the data of the block pointed by the hash function for the given secondary hash tables, index key and depths
		get_data(depth1, index_key, sindexDesc1, &data1);
		get_data(depth2, index_key, sindexDesc2, &data2);

	////////////////////////////// SECONDARY 1 //////////////////////////////

	// get records and tuple ids of every record with index_key for the block hashed in secondary1
	
		int tupleIds1[sht_records_per_block];
		int count1 = 0;
		interate_join(data1 , index_key , &count1 , tupleIds1);
	
	////////////////////////////// SECONDARY 2 //////////////////////////////

	// get records and tuple ids of every record with index_key for the block hashed in secondary2

		int tupleIds2[sht_records_per_block];
		int count2 = 0;
		interate_join(data2,index_key , &count2 , tupleIds2);
	
		
		printf("%40s JOINED    RECORDS on  %s.%s = %s.%s where %s.%s = %s  \n\n" ," ", open_files[primary1_file_location].filename ,
		atrribute_name1 , open_files[primary2_file_location].filename , attribute_name2 , open_files[primary1_file_location].filename , 
		atrribute_name1 ,  index_key);

		print_title(atrribute_name1 , attribute_name2 , primary1_file_location , primary2_file_location);
			
		int records_offset = 2*sizeof(int) + sizeof(uint8_t) * number_of_indexes + sizeof(int);
		for (int i =0; i < count1 ; i++) {
			Record record1;
			get_primary_recrord(tupleIds1[i] , primary1 , &record1 , records_offset  );
			for (int j = 0; j < count2; j++) {
				Record record2;
				get_primary_recrord(tupleIds2[j] , primary2 , &record2 , records_offset);
				print_joined(print_city1, print_city2, record1, record2);
				count_result++;
			}
		}
		BF_Block_Destroy(&block);
		free(atrribute_name1);
		free(attribute_name2);
		printf("Result has %d lines\n" , count_result);
		return HT_OK;
	}
	
	int records_offset = 2*sizeof(int) + sizeof(uint8_t) * number_of_indexes + sizeof(int);
	int sec_rec_offset =  2*sizeof(int) + sizeof(uint8_t) * sht_number_of_indexes + sizeof(int);
	
	int size1 = 1 << depth1;
	// Calculate the number of directory blocks that exist in the file
	int number_of_dir_blocks = (size1 / MAX_HASH_TABLE) > 1 ? size1 / MAX_HASH_TABLE : 1;
	
	int blocks_num;
	CALL_BF(BF_GetBlockCounter(sindexDesc1, &blocks_num));
	// printf("the file has %d\n" , blocks_num);
	BF_Block* data_block1;
	BF_Block_Init(&data_block1);
	// int primary_printed = 0;

	printf("%40s JOINED    RECORDS on  %s.%s = %s.%s \n\n" ," ", open_files[primary1_file_location].filename ,
	atrribute_name1 , open_files[primary2_file_location].filename , attribute_name2 );
	print_title(atrribute_name1 , attribute_name2 , primary1_file_location , primary2_file_location);
	int size = member_size(SecondaryRecord, index_key);
	for (int i = number_of_dir_blocks + 1; i < blocks_num ; i++) {
			
		BF_GetBlock(sindexDesc1, i, data_block1);
		char* data1 = BF_Block_GetData(data_block1);
		CALL_BF(BF_UnpinBlock(data_block1));
		
		// Iterate over the indexes of the block
		// Each index is an 8bit map that shows whether a spot contains an entry or not
		uint8_t indexes1[sht_number_of_indexes];
		memcpy(&indexes1 , data1 + 2*sizeof(int) , sizeof(uint8_t) * sht_number_of_indexes);
		for (int k = 0; k < sht_number_of_indexes; k++) {

			// Copy the k -th index to a variable			
			// check_bit starts from 10000000 and will help to check if a bit of the index is set or not
			uint8_t check_bit1 = 0x80;
			int record_index1 = 0;
			
			// Loop until copy index contains only zeros, thus there are no more records to print
			while (indexes1[k]) {

				// If this bit is set we found a record
				if (indexes1[k] & check_bit1) {
				
					SecondaryRecord secRec1;
					memcpy_to_secondary_rec(&secRec1 , data1+ sec_rec_offset + (k*8 +record_index1) *REAL_SEC_RECORD_SIZE );
	
					Record record1;
					get_primary_recrord(secRec1.tupleId , primary1 , &record1 , records_offset);
				
					char *data2;
					get_data(depth2 , secRec1.index_key , sindexDesc2 , &data2);
					uint8_t indexes2[sht_number_of_indexes];
					memcpy(&indexes2 , data2 + 2*sizeof(int) , sizeof(uint8_t) * sht_number_of_indexes);
					
					for (int k2 = 0; k2 < sht_number_of_indexes; k2++) {

						// Copy the k -th index to a variable			

						// check_bit starts from 10000000 and will help to check if a bit of the index is set or not
						uint8_t check_bit2 = 0x80;
						int record_index2 = 0;
				
						// Loop until copy index contains only zeros, thus there are no more records to print
						while (indexes2[k2]) {

							// If this bit is set we found a record
							if (indexes2[k2] & check_bit2) {
								
								char rec_index_key2[size];
								memcpy(&rec_index_key2, data2 + sec_rec_offset + (k2*8 + record_index2 ) *REAL_SEC_RECORD_SIZE, size);
					
								if (!strcmp(rec_index_key2 , secRec1.index_key)) {
									int new2 ;
									memcpy(&new2 , data2 + sec_rec_offset + (k2*8 + record_index2 ) *REAL_SEC_RECORD_SIZE + member_size(SecondaryRecord , index_key) , sizeof(int));
									Record record2;
								
									get_primary_recrord(new2 , primary2 , &record2 , records_offset);	
									print_joined(print_city1, print_city2, record1, record2);
									count_result++;							
								}

								// Unset the bit in the index copy
								indexes2[k2] &= ~check_bit2;
							}

							// Increase the record_index by 1 each time to calculate the offset properly
							// and get ready to check the next bit
							record_index2++;
							check_bit2 >>= 1; 	
						}
					}		
					indexes1[k] &= ~check_bit1;
				}	

				// Increase the record_index by 1 each time to calculate the offset properly
				// and get ready to check the next bit
				record_index1++;
				check_bit1 >>= 1;
			}
		}
	}
	printf("Result has %d lines\n" , count_result);
	BF_Block_Destroy(&data_block1);
	BF_Block_Destroy(&block);
	free(atrribute_name1);
	free(attribute_name2);
	return HT_OK;
}



HT_ErrorCode HT_SHT_Insert(int indexDesc, Record record, size_t sec_count, ...)
{

	int tupleId;
	UpdateRecordArray arr;

	CALL_HT(HT_InsertEntry(indexDesc, record, &tupleId, &arr))


	BF_Block* block;
	BF_Block_Init(&block);

	int off =  sizeof(SECONDARY_HASH_ID) + sizeof(int) + sizeof(int);

	va_list valist;
	
	va_start(valist, sec_count);

	for (int i = 0; i < sec_count; i++) 
	{

		int sht_index = va_arg(valist, int);
		int sindexDesc2 = open_files2[sht_index].location;

		CALL_BF(BF_GetBlock(sindexDesc2, 0, block));
		char* data = BF_Block_GetData(block);
		int count = 0;
		char *temp = data + off;

		while (*temp++ !='\0' ) 
			count++;

		char* attribute_name = malloc((count + 1) *sizeof(char));
		
		for (int i = 0; i < count; i++)
			attribute_name[i] = data[off + i];

		attribute_name[count] = '\0';

		// Update secondary index
		CALL_HT(SHT_SecondaryUpdateEntry(sht_index, &arr));

		SecondaryRecord dummy2;
		strcpy(dummy2.index_key, strcmp(attribute_name, "surname") == 0 ? record.surname : record.city);
		dummy2.tupleId = tupleId;

		// Insert new value to secondary index
		CALL_HT(SHT_SecondaryInsertEntry(sht_index, dummy2));
			
	}
		
	/* clean memory reserved for valist */
	va_end(valist);

	free(arr.changed_tuples);

	return HT_OK;	


}




