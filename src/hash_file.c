#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <stdint.h>

#include "bf.h"
#include "hash_file.h"
#include "common.h"




#define CALL_BF(call) {       	\
	BF_ErrorCode code = call; 	\
	if (code != BF_OK) {       	\
		BF_PrintError(code);    \
		return HT_ERROR;        \
	}                         	\
}


File open_files[MAX_OPEN_FILES];
int records_per_block = -1;
int number_of_indexes = -1;


void memcpy_rec(char* data, Record rec) {
	int offset = 0;
	memcpy(data, &rec.id, sizeof(rec.id));
	offset += sizeof(rec.id);

	for (int i = 0, len = strlen(rec.name); i < member_size(Record, name); i++, offset++)
		if (i < len)
			data[offset] = rec.name[i];
		else
			data[offset] = '\0';

	for (int i = 0, len = strlen(rec.surname); i < member_size(Record, surname); i++, offset++)
		if (i < len)
			data[offset] = rec.surname[i];
		else
			data[offset] = '\0';

	for (int i = 0, len = strlen(rec.city); i < member_size(Record, city); i++, offset++)
		if (i < len)
			data[offset] = rec.city[i];
		else
			data[offset] = '\0';
}


void memcpy_to_rec(Record* rec, char* data) {
	int offset = 0;
	
	memcpy(&rec->id, data, sizeof(rec->id));
	offset += sizeof(rec->id);

	for (int i = 0; i < member_size(Record, name); i++, offset++) 
		rec->name[i] = data[offset];

	for (int i = 0; i < member_size(Record, surname); i++, offset++) 
		rec->surname[i] = data[offset];

	for (int i = 0; i < member_size(Record, city); i++, offset++) 
		rec->city[i] = data[offset];
		
}


HT_ErrorCode print_file(int indexDesc)
{
	BF_Block* block;
	BF_Block_Init(&block);

	int gd = 0;
	CALL_BF(BF_GetBlock(open_files[indexDesc].location, 0, block));
	char* data = BF_Block_GetData(block);
	memcpy(&gd, data + sizeof(HASH_ID), sizeof(int));
	CALL_BF(BF_UnpinBlock(block));

	int num_blocks = 0;
	CALL_BF(BF_GetBlockCounter(open_files[indexDesc].location, &num_blocks));

	// Print general information about the file structure
	printf("glopal depth: %d\n", gd);
	printf("hash indexs: %d\n", 1 << gd);
	printf("hash blocks: %ld\n\n", ((1 << gd ) - 1) / MAX_HASH_TABLE + 1);

	int count = 0;
	
	// Print the hash table, ie where each index is pointing to
	for (int i = 0; i < 1 + (1 << gd ) / MAX_HASH_TABLE; i++)
	{
		
		CALL_BF(BF_GetBlock(open_files[indexDesc].location, i + 1, block));
		char* data = BF_Block_GetData(block);
		uint32_t hash[MAX_HASH_TABLE];
		memcpy(hash, data, sizeof(hash));

		printf("|");

		for (int j = 0; j < MAX_HASH_TABLE; j++)
		{
			printf("%3d|", hash[j]);
			if (++count % 32 == 0)
				printf("\n%s", !(count == (((1 << gd ) - 1) / MAX_HASH_TABLE + 1) * MAX_HASH_TABLE) ? "|" : "");
		}
		
		printf("\t");
		
		CALL_BF(BF_UnpinBlock(block));
	}

	printf("\n");


	// For each bucket print its local depth, number of records in the bucket, prefix and the bytes that make the bitmap for the records
	// Then print the record, or 0 if there isnt a record in that place 
	for (int i = 1 + (1 << gd ) / MAX_HASH_TABLE + 1; i < num_blocks; i++)
	{
		CALL_BF(BF_GetBlock(open_files[indexDesc].location, i, block));
		char* data = BF_Block_GetData(block);

		int ld = 0;
		memcpy(&ld, data, sizeof(int));

		int ins = 0;
		memcpy(&ins, data + sizeof(int), sizeof(int));

		uint8_t indexes[number_of_indexes];
		memcpy(indexes, data + 2 * sizeof(int), sizeof(uint8_t) * number_of_indexes);

		uint32_t prefix = 0;
		memcpy(&prefix, data + 2 * sizeof(int) + sizeof(uint8_t) * number_of_indexes, sizeof(uint32_t));
		printf("%3d: %2d %d %3d ", i, ld, ins, prefix);

		for (int k = 0; k < number_of_indexes; k++)
			printf("%3d ", indexes[k]);
		
		printf("| ");
		
		for (int k = 0; k < number_of_indexes; k++)
		{
			for (int j = 0; j < records_per_block; j++)
			{
				if (((indexes[k] >> (7-j)) & 1) == 0)
				{
					printf("%3d-%2s-%2s ", 0, "", "");
					continue;
				}
				Record rec;
				memcpy_to_rec(&rec, data + 2 * sizeof(int) + sizeof(uint8_t) * number_of_indexes + sizeof(uint32_t) + j * REAL_RECORD_SIZE);
				// We dont print the name since it isnt used on the secondary module
				printf("%3d-%2s-%2s ", rec.id, rec.surname, rec.city);
			}
		}

		printf("\n");

		CALL_BF(BF_UnpinBlock(block));
	}

	BF_Block_Destroy(&block);
	printf("\n");
	
	return HT_OK;
}


HT_ErrorCode HT_Init() {

	for (size_t i = 0; i < MAX_OPEN_FILES; i++)
		open_files[i].used = false;

	// Only do it the first time init is called
	if (records_per_block < 0) {
		// block size - header size
		size_t remaining = BF_BLOCK_SIZE - 2 * sizeof(int) - sizeof(int);

		// first evaluation of the # of buckets
		records_per_block = remaining / REAL_RECORD_SIZE;

		// Bytes that we would need to store the indexes with the above # of buckets
		int indexes = records_per_block / 8 + (records_per_block % 8 != 0);

		// Bytes required for the buckets
		int buckets_size = records_per_block * REAL_RECORD_SIZE;

		// Available bytes
		int av_bytes = remaining - buckets_size;

		// If available bytes are not enough substruct a record and update the values
		records_per_block -= av_bytes < indexes;
		number_of_indexes = indexes;
	}
	return HT_OK;
}


HT_ErrorCode HT_CreateIndex(const char* filename, int depth) {

	CALL_BF(BF_CreateFile(filename));

	char* data;
	int indexDesc, empty = 0;

	// number of buckets will be 2^depth
	int number_of_buckets = 1 << depth;


	// Calculate the directory blocks that need to be allocated to host these buckets
	int blocks_to_allocate = number_of_buckets > MAX_HASH_TABLE ? number_of_buckets / MAX_HASH_TABLE : 1;
	
	// if we need more than 1 dir blocks every dir block will host MAX_HASH_TABLE buckets
	if (blocks_to_allocate > 1)
		number_of_buckets = MAX_HASH_TABLE;


	CALL_BF(BF_OpenFile(filename, &indexDesc));

	BF_Block* block;
	BF_Block_Init(&block);

	// Write the hash identifier and the global depth to the first block of the file
	CALL_BF(BF_AllocateBlock(indexDesc, block));
	data = BF_Block_GetData(block);
	memcpy(data, HASH_ID, sizeof(HASH_ID));
	memcpy(data + sizeof(HASH_ID), &depth, sizeof(int));

	BF_Block_SetDirty(block);
	CALL_BF(BF_UnpinBlock(block));

	BF_Block* bucket;
	uint32_t hash_table[number_of_buckets];
	uint8_t index[number_of_indexes];


	for (int i = 0; i < number_of_indexes; i++)
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

			// Write the local depth, amount of records and index array to the block (initialization)
			char* bucket_data = BF_Block_GetData(bucket);
			memcpy(bucket_data, &depth, sizeof(int));
			memcpy(bucket_data + sizeof(int), &empty, sizeof(int)); 
			memcpy(bucket_data + 2 * sizeof(int), &index, sizeof(index));
			int pointed_prefix = j * MAX_HASH_TABLE + i;
			memcpy(bucket_data + 2 * sizeof(int) + sizeof(index), &pointed_prefix, sizeof(int));

			// Store the location of the block to the array that will be written on the directory block
			hash_table[i] = ((i + blocks) + j * 128 ); 

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

	return HT_OK;
}


HT_ErrorCode HT_OpenIndex(const char* fileName, int* indexDesc) {

	int loc;
	CALL_BF(BF_OpenFile(fileName, &loc));

	BF_Block* block;
	BF_Block_Init(&block);

	CALL_BF(BF_GetBlock(loc, 0, block));

	char* data = BF_Block_GetData(block);

	// Copy the bytes that should make the HASH_ID into a char array so we can use strcmp to check if it is correct
	// We do this this way in order to have a more general approach 
	// i.e. to be able to change only the HASH_ID when we want a new hash identifier
	size_t length = strlen(HASH_ID);
	char str[length + 1];
	str[length] = '\0';

	for (size_t i = 0; i < length; i++)
		str[i] = data[i];


	// We don't need to set the block as dirty since we didn't alter its content
	CALL_BF(BF_UnpinBlock(block));
	BF_Block_Destroy(&block);

	// Assert the file given is a hash file this module made
	if (strcmp(str, HASH_ID)) {
		CALL_BF(BF_CloseFile(loc));
		return HT_ERROR;
	}

	// Find first avalaible index to "store" the open file
	for (size_t i = 0; i < MAX_OPEN_FILES; i++) {
		if (!open_files[i].used) {
			// Cell is used
			open_files[i].used = true;

			// Store the location returned from the bf module to the struct
			open_files[i].location = loc;

			open_files[i].filename = calloc(strlen(fileName) + 1, sizeof(char));

			strcpy(open_files[i].filename, fileName);

			// "Return" the appropriate index of the open_files array
			*indexDesc = i;

			return HT_OK;		
		}
	}

	// Didn't find an empty spot, return error
	CALL_BF(BF_CloseFile(loc));
	return HT_ERROR;
}


HT_ErrorCode HT_CloseFile(int indexDesc) {

	assert(indexDesc >= 0 && indexDesc < 20);

	// If there is a file open in the corresponding array index
	if (open_files[indexDesc].used) {

		// Close the file
		CALL_BF(BF_CloseFile(open_files[indexDesc].location));

		// Cell no longer used
		open_files[indexDesc].used = false;
		free(open_files[indexDesc].filename);
		open_files[indexDesc].filename = NULL;

		return HT_OK;			
	}

	return HT_ERROR;
}


static uint32_t significant_hash(int value, int bits) {
	int bits_num = sizeof(value) * 8;
	uint32_t reverse_num = 0;

	for (int i = 0; i < bits_num; i++) {
	int temp = (value & (1 << i));
	if (temp)
		reverse_num |= (1 << ((bits_num - 1) - i));
	}

	return (reverse_num >> (bits_num - bits));
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


// We need this variable for 2 reasons
// First, 	we don't want to allocate memory for the array again, because it will disgard the previous changes
// Second, 	on the same insert with multiple split a record could change bucket multiple times
// 			in that case we want to store the original position as the 'oldTupleId' and not the one on the last split
//			and this variable allows us to do just that
static size_t recursive_split_calls = 0;

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

	CALL_BF(BF_GetBlock(open_files[indexDesc].location, 0, block));
	char* data = BF_Block_GetData(block);
	memcpy(data + sizeof(HASH_ID), &global_depth, sizeof(int));
	BF_Block_SetDirty(block);
	CALL_BF(BF_UnpinBlock(block));

	CALL_BF(BF_GetBlock(open_files[indexDesc].location, 1, block));
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
		CALL_BF(BF_GetBlock(open_files[indexDesc].location, i + index_blocks + 1, bucket));
		char* bucket_data = BF_Block_GetData(bucket);
		
		CALL_BF(BF_AllocateBlock(open_files[indexDesc].location, bucket2));
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
	CALL_BF(BF_GetBlockCounter(open_files[indexDesc].location, &num_blocks));

	int last_dir = -1;
	char* bucket_data2 = NULL;
	int wrote = 0;

	// For every data block (bucket) calculate the indexes of the directory that should be pointing to it 
	// and update the directory accordingly
	for (size_t i = new_index_blocks + 1, end = num_blocks, offset = 2 * sizeof(int) + sizeof(uint8_t) * number_of_indexes; i < end; i++) {
		CALL_BF(BF_GetBlock(open_files[indexDesc].location, i, bucket));
		char* bucket_data = BF_Block_GetData(bucket);
		
		int depth = 0;
		memcpy(&depth, bucket_data, sizeof(int));

		int prefix = 0;
		memcpy(&prefix, bucket_data + offset, sizeof(int));

		int* buckets = find_prefixes(prefix, depth, global_depth);

		for (int j = buckets[0], end = buckets[3]; j <= end; j++) {
			int dir = j / MAX_HASH_TABLE;

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

				CALL_BF(BF_GetBlock(open_files[indexDesc].location, dir + 1, bucket2));
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

static HT_ErrorCode split_bucket(int indexDesc, int bucket_num, int global_depth, UpdateRecordArray* updateArray) {
	// Split a data block
	// New block goes to the end of the file to improve performance 
	int size_of_bucket_header = 2 * sizeof(int) + number_of_indexes * sizeof(uint8_t) + sizeof(int);


	BF_Block* bucket;
	BF_Block_Init(&bucket);

	CALL_BF(BF_GetBlock(open_files[indexDesc].location, bucket_num, bucket));
	char* bucket_data = BF_Block_GetData(bucket);
	
	int depth = 0;
	memcpy(&depth, bucket_data, sizeof(int));

	int prefix = 0;
	memcpy(&prefix, bucket_data + 2 * sizeof(int) + sizeof(uint8_t) * number_of_indexes, sizeof(int));

	int* buckets = find_prefixes(prefix, depth, global_depth);

	BF_Block* new_bucket;
	BF_Block_Init(&new_bucket);


	CALL_BF(BF_AllocateBlock(open_files[indexDesc].location, new_bucket));
	char* new_data = BF_Block_GetData(new_bucket);

	// For every record on bucket if its hash is in [buckets[0], buckets[1]] it stays there
	// else goes to new bucket and we just remove the 1 bit from index

	uint8_t indexes[number_of_indexes];
	memcpy(indexes, bucket_data + 2 * sizeof(int), sizeof(indexes));

	uint8_t new_indexes[number_of_indexes];
	memcpy(new_indexes, new_data + 2 * sizeof(int), sizeof(new_indexes));

	// Allocate memory only on the first split
	// We know that the maximum amount of records that will change position is the maximum amount of records in a bucket, and we allocate memory accordingly
	if (recursive_split_calls == 1)
		updateArray->changed_tuples = calloc(records_per_block, sizeof(*updateArray->changed_tuples));
	
	assert(updateArray->changed_tuples);

	int new_block_num = 0;
	BF_GetBlockCounter(open_files[indexDesc].location, &new_block_num);

	int inserted = 0;

	for (int i = 0; i < number_of_indexes; i++) {
		// For all bits check if any is 1 => the bytes of the record corresponding there are used 
		for (int j = 7; j >= 0 && (i * 8 + 7 - j) < records_per_block; j--) {
			
			if ( ((indexes[i] >> j) & 1 ) == 1) {
			
				Record record;
				memcpy_to_rec(&record, bucket_data + size_of_bucket_header + (i * 8 + 7 - j) * REAL_RECORD_SIZE);

				uint32_t hash = significant_hash(record.id, global_depth);

				// If its hash corresponds to the new bucket put it there
				// We don't remove the record from the old block, we just set its index to 0 to improve performance
				if (hash >= buckets[2]) {
					
					// Update the index char to account for the lost entry
					indexes[i] &= ~(1 << j);

					// Write the new index char over the old one 
					memcpy(new_data + 2 * sizeof(int), indexes, sizeof(indexes));

					// Write every field of the record to the file on the appropriate position of the new block
					memcpy_rec(new_data + size_of_bucket_header + inserted * REAL_RECORD_SIZE, record);

					size_t old_tuple_id = bucket_num * records_per_block + i * 8 + 7 - j;
					size_t new_tuple_id = (new_block_num - 1) * records_per_block + inserted;

					// If this is not the first split
					if (recursive_split_calls > 1)
					{
						// Find the record matching the current record we are moving and only change its 'newTupleId'
						for (size_t i = 0; i < updateArray->tuples_changed; i++)
						{
							if (updateArray->changed_tuples[i].newTupleId == old_tuple_id)
							{
								(updateArray->changed_tuples[i]).newTupleId = new_tuple_id;
								break;
							}
						}
					}
					// Else just store the necessary information
					else
					{
						strcpy((updateArray->changed_tuples[inserted]).surname, record.surname); 
						strcpy((updateArray->changed_tuples[inserted]).city, record.city);
						(updateArray->changed_tuples[inserted]).oldTupleId = old_tuple_id;
						(updateArray->changed_tuples[inserted]).newTupleId = new_tuple_id;
					}


					inserted++;

				}
			}
		}
	}

	// Update the number of records inserted
	updateArray->tuples_changed = inserted > updateArray->tuples_changed ? inserted : updateArray->tuples_changed;

	int number_of_records = 0;
	memcpy(&number_of_records, bucket_data + sizeof(int), sizeof(int));

	// Write the new number of records stored over the original block 
	number_of_records -= inserted;
	memcpy(bucket_data + sizeof(int), &number_of_records, sizeof(int));
	memcpy(bucket_data + 2 * sizeof(int), indexes, sizeof(indexes));

	// Write # of records stored to the new block 
	memcpy(new_data + sizeof(int), &inserted, sizeof(int));
	
	// Update the index char to include the new entries
	for (size_t i = 0; i < number_of_indexes && inserted > 0; i++) {
		for (size_t j = 7; j >= 0 && inserted > 0; j--, inserted--)
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
	memcpy(bucket_data + 2 * sizeof(int) + sizeof(uint8_t) * number_of_indexes, &prefix, sizeof(int));
	memcpy(new_data + 2 * sizeof(int) + sizeof(uint8_t) * number_of_indexes, &new_prefix, sizeof(int));


	BF_Block_SetDirty(bucket);
	BF_Block_SetDirty(new_bucket);
		
	CALL_BF(BF_UnpinBlock(bucket));
	CALL_BF(BF_UnpinBlock(new_bucket));


	// Update indexes

	int last_dir = -1;
	char* bucket_data2 = NULL;
	int wrote = 0;
	int num_blocks = 0;
	CALL_BF(BF_GetBlockCounter(open_files[indexDesc].location, &num_blocks));
		
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
			
			CALL_BF(BF_GetBlock(open_files[indexDesc].location, dir + 1, new_bucket));
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


HT_ErrorCode HT_InsertEntry(int indexDesc, Record record, int* tupleId, UpdateRecordArray* updateArray) {
	
	// If file is not opened or index is out of range, return error
	if (indexDesc >= 0 && indexDesc <= MAX_OPEN_FILES && !open_files[indexDesc].used)
		return HT_ERROR;

	if (!recursive_split_calls)
	{
		updateArray->changed_tuples = NULL;
		updateArray->tuples_changed = 0;
	}

	BF_Block* block;
	char* data ;
	int depth ;

	BF_Block_Init(&block);
	
	CALL_BF(BF_GetBlock(open_files[indexDesc].location, 0, block));
	data = BF_Block_GetData(block);
	memcpy(&depth, data + sizeof(HASH_ID), sizeof(int));
	CALL_BF(BF_UnpinBlock(block));

	uint32_t hash = significant_hash(record.id, depth);

	int number_of_records, local_depth, i, j;
	uint32_t block_to_write;
	uint32_t block_to_get = hash / MAX_HASH_TABLE + 1; 

	int num_blocks = 0;
	CALL_BF(BF_GetBlockCounter(open_files[indexDesc].location, &num_blocks));

	CALL_BF(BF_GetBlock(open_files[indexDesc].location, block_to_get, block));
	data = BF_Block_GetData(block);
	memcpy(&block_to_write, data + ( (hash % MAX_HASH_TABLE) * sizeof(uint32_t)), sizeof(uint32_t));
	CALL_BF(BF_UnpinBlock(block));

	CALL_BF(BF_GetBlock(open_files[indexDesc].location, block_to_write, block));
	data = BF_Block_GetData(block);
	memcpy(&number_of_records, data + sizeof(int), sizeof(int));
	CALL_BF(BF_UnpinBlock(block));

	// Bucket has space for the record => insert it there
	if (number_of_records < records_per_block) {	
		uint8_t index[number_of_indexes];

		CALL_BF(BF_GetBlock(open_files[indexDesc].location, block_to_write, block));
		data = BF_Block_GetData(block);
		int prefix = 0;
		memcpy(&prefix, data + 2 * sizeof(int) + sizeof(uint8_t) * number_of_indexes, sizeof(int));
		
		memcpy(index, data + 2 * sizeof(int), sizeof(index));

		// Find first empty spot
		for (i = 0; i < number_of_indexes; i++) {
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
		memcpy(data + 2 * sizeof(int), index, sizeof(uint8_t) * number_of_indexes);

		// Write every field of the record to the file on the appropriate position of the block
		memcpy_rec(data + 2 * sizeof(int) + sizeof(uint8_t) * number_of_indexes + sizeof(int) + (i * 8 + 7 - j) * REAL_RECORD_SIZE, record);
		
		BF_Block_SetDirty(block);
		CALL_BF(BF_UnpinBlock(block));
		BF_Block_Destroy(&block);


		*tupleId = block_to_write * records_per_block + (i * 8 + 7 - j);
		recursive_split_calls = 0;
		return HT_OK;
	}

	// Bucket doesn't have space for the record and we need to split the bucket or expand the directory 
	CALL_BF(BF_GetBlock(open_files[indexDesc].location, block_to_write, block));
	data = BF_Block_GetData(block);
	memcpy(&local_depth, data, sizeof(int));
	CALL_BF(BF_UnpinBlock(block));

	// Pointers pointing to bucket are more than 1 and therefore bucket can be split
	if (local_depth < depth) {
		recursive_split_calls++;
		if (split_bucket(indexDesc, block_to_write, depth, updateArray) == HT_ERROR) {
			recursive_split_calls = 0;
			return HT_ERROR;
		}
	// Directory needs expansion
	} else {
		if (expand_dir(indexDesc, depth) == HT_ERROR) {	
			recursive_split_calls = 0;
			return HT_ERROR;
		}	
	}
	
	BF_Block_Destroy(&block);

	// Either we split a bucket and we will insert the record 
	// or we expanded the directory and we will now split a bucket and finally insert the record 
	return HT_InsertEntry(indexDesc, record, tupleId, updateArray);
	
}


void print_record(const void *data) {
	printf("Record id : %-6d", (*(int*) (data)) );
	printf("Record name : %-15s", (char*) (data + member_size(Record, id)) );
	printf("Record surname : %-20s", (char*) (data + sizeof(int) + member_size(Record, name)) );
	printf("Record city : %-20s\n", (char*) (data + sizeof(int) + member_size(Record, name) + member_size(Record, surname)) );
}


HT_ErrorCode HT_PrintAllEntries(int indexDesc, int* id) {

	int depth ;
	BF_Block* dir_block, *data_block;
	BF_Block_Init(&data_block);
	BF_Block_Init(&dir_block);

	indexDesc = open_files[indexDesc].location;
	// Get the depth from the info block
	CALL_BF(BF_GetBlock(indexDesc, 0, dir_block));
	char* data = BF_Block_GetData(dir_block), *records_block_data;
	memcpy(&depth, data + sizeof(HASH_ID), sizeof(int));


	// Size is 2^depth
	int size = 1 << depth; 

	// Calculate the number of directory blocks that exist in the file
	int number_of_dir_blocks = (size / MAX_HASH_TABLE) > 1 ? size / MAX_HASH_TABLE : 1;
	
	

	CALL_BF(BF_UnpinBlock(dir_block));


	// We print every entry in this case
	if (id == NULL) {
		int count = 0;

		// Iterate over the directory blocks
		int blocks_num;
		CALL_BF(BF_GetBlockCounter(indexDesc, &blocks_num));
		for (int i = number_of_dir_blocks + 1; i < blocks_num ; i++) {


			BF_GetBlock(indexDesc, i, data_block);
			records_block_data = BF_Block_GetData(data_block);
				
				// Iterate over the indexes of the block
				// Each index is an 8bit map that shows whether a spot contains an entry or not
				for (int k = 0, new_offset = 2*sizeof(int); k < number_of_indexes; k++, new_offset += sizeof(uint8_t)) {

					// Copy the k -th index to a variable			
					uint8_t indexes;
					memcpy(&indexes, records_block_data + new_offset, sizeof(uint8_t)); 

					// check_bit starts from 10000000 and will help to check if a bit of the index is set or not
					uint8_t check_bit = 0x80;
					int record_index = 0, records_offset = 2*sizeof(int) + sizeof(uint8_t) * number_of_indexes + sizeof(int);
					
					// Loop until copy index contains only zeros, thus there are no more records to print
					while (indexes) {

						// If this bit is set we found a record
						if (indexes & check_bit) {

							// Every index is of size 8 so the record's offset is (k * 8 + record_index ) * REAL_RECORD_SIZE 
							print_record(records_block_data + records_offset + (k*8 + record_index) *REAL_RECORD_SIZE);
							count++;

							// Unset the bit in the index copy
							indexes &= ~check_bit;
						}

						// Increase the record_index by 1 each time to calculate the offset properly
						// and get ready to check the next bit
						record_index++;
						check_bit >>= 1; 	
					}
				}
				CALL_BF(BF_UnpinBlock(data_block));
		}

	printf("Printed %d records\n", count);
	
	} else {

		// We look for a record in this case
		// Check the record's hash and calculate which dir block should contain it
		uint32_t hash = significant_hash(*id, depth);
		int dir_block_to_get = hash / MAX_HASH_TABLE + 1;

		uint32_t position = hash % MAX_HASH_TABLE;
		int block_to_find;

		CALL_BF(BF_GetBlock(indexDesc, dir_block_to_get, dir_block));
		char* data = BF_Block_GetData(dir_block);

		// Find the block that contains the records with this hash and get it
		memcpy(&block_to_find, data + position * sizeof(uint32_t), sizeof(uint32_t));
		CALL_BF(BF_UnpinBlock(dir_block));
		CALL_BF(BF_GetBlock(indexDesc, block_to_find, data_block));
		records_block_data = BF_Block_GetData(data_block);


		// found will hold the number of times we found the record with id = *id in the file
		int found = 0; 

		// Iterate over the indexes of the block
		// Each index is an 8bit map that shows if a spot contains an entry or not
		for (int k = 0, new_offset = 2 * sizeof(int); k < number_of_indexes; k++, new_offset += sizeof(uint8_t)) {
					
			//copy the k-th index to a variable	
			uint8_t indexes;
			memcpy(&indexes, records_block_data + new_offset, sizeof(uint8_t)); 
			
			// check_bit starts from 10000000 and will help to check if a bit of the index is set or not
			uint8_t check_bit = 0x80;
			int record_index = 0, records_offset = 2*sizeof(int) + sizeof(uint8_t) * number_of_indexes + sizeof(int);

			// Loop until copy index contains only zeros, thus there are no more records to print
			while (indexes) {

				// if this bit is set we found a record
				if (indexes & check_bit) {

					// Get its record id to see if it is the record we are looking for
					// Every index is of size 8 so the record's offset is (k * 8 + record_index ) * REAL_RECORD_SIZE 
					int rec_id;
					memcpy(&rec_id, records_block_data + records_offset + (k*8 + record_index ) *REAL_RECORD_SIZE, sizeof(int));
					
					if (rec_id == *id) {
						print_record(records_block_data + records_offset + (k*8 + record_index)*REAL_RECORD_SIZE);
						found++;
					}


				// Unset the bit in the index copy
				indexes &= ~check_bit;
				}

				// Increase the record_index by 1 each time to calculate the offset properly and get ready to check the next bit
				record_index++;
				check_bit >>= 1;	
			}
		}

		if (!found) 
			printf("Record with id %d does NOT exist\n", *id);
		else 
			printf("Record with id %d exists %d time%s", *id, found, found > 1 ? "s\n" : "\n");	
		
		CALL_BF(BF_UnpinBlock(data_block));
	}

	BF_Block_Destroy(&dir_block);
	BF_Block_Destroy(&data_block);

	return HT_OK;
}


HT_ErrorCode HashStatistics(char* filename) {
	
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
	memcpy(global_depth_pointer, data+sizeof(HASH_ID), sizeof(int));

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