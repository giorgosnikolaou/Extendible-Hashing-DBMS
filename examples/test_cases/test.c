#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>


#include "bf.h"
#include "hash_file.h"
#include "sht_file.h"

const char* names[] = { "data1.db", "data2.db", "data3.db", "data4.db", "data5.db", "data6.db", "data7.db", "data8.db", "data9.db", "data10.db", 
						"data11.db", "data12.db", "data13.db", "data14.db", "data15.db", "data16.db", "data17.db", "data18.db", "data19.db", "data20.db"  };



#define CALL_OR_DIE(call)     			\
		{                           	\
			BF_ErrorCode code = call; 	\
			if (code != BF_OK) 			\
			{        					\
				BF_PrintError(code);    \
				exit(0);			    \
			}                           \
		}

#define CALL_HT(call)     				\
		{                           	\
			HT_ErrorCode code = call;	\
			if (code != HT_OK) {      	\
			fprintf(stderr, "Error\n");	\
			exit(code);             	\
			}                         	\
		}

void insert_primary(int loc, int id, const char* surname, const char* city)
{
	Record dummy;

	strcpy(dummy.name, "George");

    int tupleId = 0;

    UpdateRecordArray arr;

	strcpy(dummy.surname, surname);
	strcpy(dummy.city, city);
    dummy.id = id;
    CALL_HT(HT_InsertEntry(loc, dummy, &tupleId, &arr));
	free(arr.changed_tuples);
}

static void set_record(Record* rec, int id, const char* name, const char* surname, const char* city)
{
	rec->id = id;
	strcpy(rec->name, name);
	strcpy(rec->surname, surname);
	strcpy(rec->city, city);
}


int main() 
{
	//	Uncomment the print_file, print_sec_file function calls to print the full information about each file and see that the file is indeed correct 
	//	Uncomment the print_file, print_sec_file function calls to print the full information about each file and see that the file is indeed correct 
	//	Uncomment the print_file, print_sec_file function calls to print the full information about each file and see that the file is indeed correct 

	CALL_OR_DIE(BF_Init(LRU));

	CALL_HT(HT_Init());
	CALL_HT(SHT_Init());

	CALL_HT(HT_CreateIndex(names[0], 1));
	
	int loc;

	
	CALL_HT(HT_OpenIndex(names[0], &loc));

	insert_primary(loc, 0b110, "george", "ff");
	insert_primary(loc, 0b1110, "georga", "gg");
	insert_primary(loc, 0b10010, "georgq", "aa");
	insert_primary(loc, 0b11010, "georga", "aa");
	insert_primary(loc, 0b100110, "georgw", "dd");
	insert_primary(loc, 0b101110, "georgo", "cc");
	insert_primary(loc, 0b110010, "georgr", "ff");
	insert_primary(loc, 0b111010, "georgt", "ee");

	CALL_HT(SHT_CreateSecondaryIndex(names[1], "surname", 20, 1, names[0]));
	CALL_HT(SHT_CreateSecondaryIndex(names[2], "city", 20, 1, names[0]));

	int loc1;
	CALL_HT(SHT_OpenSecondaryIndex(names[1], &loc1));

	int loc2;
	CALL_HT(SHT_OpenSecondaryIndex(names[2], &loc2));

	Record rec;
	set_record(&rec, 0b10, "George", "georgy", "aa");
	HT_SHT_Insert(loc, rec, 2, loc1, loc2);
	
	set_record(&rec, 0b101, "George", "georgu", "ww");
	HT_SHT_Insert(loc, rec, 2, loc1, loc2);
	
	set_record(&rec, 0b111, "George", "georgi", "ae");
	HT_SHT_Insert(loc, rec, 2, loc1, loc2);
	
	set_record(&rec, 0b10, "George", "makisas", "tr");
	HT_SHT_Insert(loc, rec, 2, loc1, loc2);

	CALL_HT(HT_CreateIndex(names[3], 1));
	
	int loc3;
	CALL_HT(HT_OpenIndex(names[3], &loc3));

	insert_primary(loc3, 0b110, "george", "ff");
	insert_primary(loc3, 0b1110, "georga", "gg");
	insert_primary(loc3, 0b10010, "georgq", "aa");
	insert_primary(loc3, 0b11010, "georga", "aa");
	insert_primary(loc3, 0b100110, "georgw", "dd");
	insert_primary(loc3, 0b101110, "georgo", "cc");
	insert_primary(loc3, 0b110010, "georgr", "ff");
	insert_primary(loc3, 0b111010, "georgt", "ee");

	CALL_HT(SHT_CreateSecondaryIndex(names[4], "city", 20, 1, names[3]));

	int loc4;
	CALL_HT(SHT_OpenSecondaryIndex(names[4], &loc4));
	
	set_record(&rec, 0b10, "George", "georgy", "aa");
	HT_SHT_Insert(loc3, rec, 1, loc4);
	
	set_record(&rec, 0b101, "George", "makisaq", "ht");
	HT_SHT_Insert(loc, rec, 2, loc1, loc2);
	
	set_record(&rec, 0b111, "George", "makisaw", "ht");
	HT_SHT_Insert(loc, rec, 2, loc1, loc2);
	
	set_record(&rec, 0b10, "George", "makisae", "makisas");
	HT_SHT_Insert(loc, rec, 2, loc1, loc2);
	
	set_record(&rec, 0b10, "George", "makisar", "makisas");
	HT_SHT_Insert(loc, rec, 2, loc1, loc2);
	
	set_record(&rec, 0b111, "George", "makisat", "pepa");
	HT_SHT_Insert(loc, rec, 2, loc1, loc2);
	
	set_record(&rec, 0b1110, "George", "makisay", "pipis");
	HT_SHT_Insert(loc, rec, 2, loc1, loc2);
	
	set_record(&rec, 0b0110, "George", "makisau", "spiros");
	HT_SHT_Insert(loc, rec, 2, loc1, loc2);
	
	set_record(&rec, 0b1110, "George", "makisai", "antreas");
	HT_SHT_Insert(loc, rec, 2, loc1, loc2);
	
	set_record(&rec, 0b11011, "George", "makisao", "kathe");
	HT_SHT_Insert(loc, rec, 2, loc1, loc2);
	
	set_record(&rec, 0b11011, "George", "makisap", "mera");
	HT_SHT_Insert(loc, rec, 2, loc1, loc2);
	
	set_record(&rec, 0b11111, "George", "makisal", "zo");
	HT_SHT_Insert(loc, rec, 2, loc1, loc2);

	printf("\n");

	printf("\n----------------------------------\nSecondary 1!\n----------------------------------\n");
	print_sec_file(loc1);
	// printf("\n----------------------------------\nSecondary 2!\n----------------------------------\n");
	// print_sec_file(loc2);
	printf("\n----------------------------------\nPrimary 1!\n----------------------------------\n");	
	print_file(loc);

	CALL_HT(SHT_PrintAllEntries(loc1, "georga"));
	printf("\n");
	CALL_HT(SHT_PrintAllEntries(loc2, NULL));

	CALL_HT(SHT_CreateSecondaryIndex(names[5], "surname", 20, 1, names[3]));

	int loc5;
	CALL_HT(SHT_OpenSecondaryIndex(names[5], &loc5));
	printf("\n----------------------------------\nSecondary 4!\n----------------------------------\n");
	CALL_HT(SHT_PrintAllEntries(loc5, NULL));

	printf("\n----------------------------------\nInner join!\n----------------------------------\n");
	CALL_HT(SHT_PrintAllEntries(loc1, "georga"));
	CALL_HT(SHT_PrintAllEntries(loc5, "georga"));
	CALL_HT(SHT_InnerJoin(loc1, loc5, "georga"));



	CALL_HT(HT_CloseFile(loc));
	CALL_HT(HT_CloseFile(loc3));
	CALL_HT(SHT_CloseSecondaryIndex(loc1));
	CALL_HT(SHT_CloseSecondaryIndex(loc2));
	CALL_HT(SHT_CloseSecondaryIndex(loc4));
	CALL_HT(SHT_CloseSecondaryIndex(loc5));

	CALL_OR_DIE(BF_Close());

	return 0;
}