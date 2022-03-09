#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>


#include "bf.h"
#include "hash_file.h"

#define PRINT_LINE() { fprintf(stderr, "====================================================================================================================================================================\n"); }


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
			fprintf(stderr, "Error\n"); \
			exit(code);             	\
			}                         	\
		}

#define INSERTIONS 10

int main() 
{
	CALL_OR_DIE(BF_Init(LRU));

	CALL_HT(HT_Init());

	CALL_HT(HT_CreateIndex(names[0], 1));
	CALL_HT(HT_CreateIndex(names[1], 4));
	CALL_HT(HT_CreateIndex(names[2], 3));
	
	int loc1, loc2, loc3;

	// loc1 should be equal to 0
	CALL_HT(HT_OpenIndex(names[0], &loc1));
	// loc2 should be equal to 1
	CALL_HT(HT_OpenIndex(names[1], &loc2));
    
	PRINT_LINE()

    // Output should be: loc1: 0, loc2: 1
	fprintf(stderr, "loc1: %d, loc2: %d\n", loc1, loc2);
	
	PRINT_LINE()

	Record dummy;

	strcpy(dummy.name, "George");
	strcpy(dummy.surname, "Nikolaou");
	strcpy(dummy.city, "Athens");

	int tuple_dummy;
	UpdateRecordArray array_dummy;

	for (int i = 0; i < INSERTIONS * 2; i++)
	{
		dummy.id = i;
		int insert_to = i % 2 ? loc1 : loc2;
		CALL_HT(HT_InsertEntry(insert_to, dummy, &tuple_dummy, &array_dummy))
		free(array_dummy.changed_tuples);
		fprintf(stderr, "Inserted record with id: %d to file with index %d!\n", dummy.id, insert_to);
	}

	PRINT_LINE()
	PRINT_LINE()

	CALL_HT(HT_PrintAllEntries(loc1, NULL));
	CALL_HT(HT_PrintAllEntries(loc2, NULL));

	CALL_HT(HT_CloseFile(loc1));

	// loc3 should be equal to 0
	CALL_HT(HT_OpenIndex(names[1], &loc3));
	// loc1 should be equal to 2
	CALL_HT(HT_OpenIndex(names[2], &loc1));

	PRINT_LINE()

    // Output should be: loc1: 2, loc3: 0
	fprintf(stderr, "loc1: %d, loc3: %d\n", loc1, loc3);
	
	PRINT_LINE()


	// Should insert the same record to the same file twice
	dummy.id = INSERTIONS * 2;
	CALL_HT(HT_InsertEntry(loc3, dummy, &tuple_dummy, &array_dummy))
	free(array_dummy.changed_tuples);
	CALL_HT(HT_InsertEntry(loc2, dummy, &tuple_dummy, &array_dummy))
	free(array_dummy.changed_tuples);

	CALL_HT(HT_PrintAllEntries(loc2, NULL));

	CALL_HT(HT_CloseFile(loc2));

	CALL_HT(HT_PrintAllEntries(loc3, NULL));

	CALL_HT(HT_CloseFile(loc1));
	CALL_HT(HT_CloseFile(loc3));


	CALL_OR_DIE(BF_Close());

	return 0;
}