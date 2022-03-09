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
			fprintf(stderr, "Error\n");	\
			exit(code);             	\
			}                         	\
		}

#define BILLION 1E9
#define INSERTIONS 10000

int main() 
{
	CALL_OR_DIE(BF_Init(LRU));

	CALL_HT(HT_Init());

	CALL_HT(HT_CreateIndex(names[0], 1));
	
	int loc;

	// loc1 should be equal to 0
	CALL_HT(HT_OpenIndex(names[0], &loc));
    
	PRINT_LINE()	
	PRINT_LINE()

	int tuple_dummy;
	UpdateRecordArray array_dummy;

	Record dummy;

	strcpy(dummy.name, "George");
	strcpy(dummy.surname, "Nikolaou");
	strcpy(dummy.city, "Athens");

	fprintf(stderr, "Inserting %d elements!\n", INSERTIONS);

	// Calculate time taken by a request
	struct timespec requestStart, requestEnd;
	clock_gettime(CLOCK_REALTIME, &requestStart);

	for (int i = 0; i < INSERTIONS; i++)
	{
		dummy.id = i;
		CALL_HT(HT_InsertEntry(loc, dummy, &tuple_dummy, &array_dummy));
		free(array_dummy.changed_tuples);
	}

    clock_gettime(CLOCK_REALTIME, &requestEnd);

	// Calculate time it took
	double accum = ( requestEnd.tv_sec - requestStart.tv_sec ) + ( requestEnd.tv_nsec - requestStart.tv_nsec ) / BILLION;
    fprintf(stderr, "Insertions took %lf seconds!\n", accum);

	PRINT_LINE()
	PRINT_LINE()

	fprintf(stderr, "Searching %d elements located on %d different blocks!\n", INSERTIONS, INSERTIONS / 8);

	// Calculate time taken by a request
	clock_gettime(CLOCK_REALTIME, &requestStart);

	for (int i = 0; i < INSERTIONS; i++)
	{
		dummy.id = i;
		CALL_HT(HT_PrintAllEntries(loc, &dummy.id));	
	}

    clock_gettime(CLOCK_REALTIME, &requestEnd);

	// Calculate time it took
	accum = ( requestEnd.tv_sec - requestStart.tv_sec ) + ( requestEnd.tv_nsec - requestStart.tv_nsec ) / BILLION;
    fprintf(stderr, "Searches took %lf seconds!\n", accum);	


	CALL_HT(HT_PrintAllEntries(loc, NULL));

	CALL_HT(HT_CloseFile(loc));

	CALL_OR_DIE(BF_Close());

	return 0;
}