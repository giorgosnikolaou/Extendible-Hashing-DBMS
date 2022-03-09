#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bf.h"
#include "hash_file.h"

#define RECORDS_NUM 1000 // you can change it if you want
#define GLOBAL_DEPT 2 // you can change it if you want
#define FILE_NAME "data.db"

const char* names[] = {
  "Yannis",
  "Christofos",
  "Sofia",
  "Marianna",
  "Vagelis",
  "Maria",
  "Iosif",
  "Dionisis",
  "Konstantina",
  "Theofilos",
  "Giorgos",
  "Dimitris"
};

const char* surnames[] = {
  "Ioannidis",
  "Svingos",
  "Karvounari",
  "Rezkalla",
  "Nikolopoulos",
  "Berreta",
  "Koronis",
  "Gaitanis",
  "Oikonomou",
  "Mailis",
  "Michas",
  "Halatsis"
};

const char* cities[] = {
  "Athens",
  "San Francisco",
  "Los Angeles",
  "Amsterdam",
  "London",
  "New York",
  "Tokyo",
  "Hong Kong",
  "Munich",
  "Miami"
};

#define CALL_OR_DIE(call)     \
  {                           \
    HT_ErrorCode code = call; \
    if (code != HT_OK) {      \
      printf("Error\n");      \
      exit(code);             \
    }                         \
  }

int main() {
	BF_Init(LRU);
	
	CALL_OR_DIE(HT_Init());

	int indexDesc;
	CALL_OR_DIE(HT_CreateIndex(FILE_NAME, GLOBAL_DEPT));
	CALL_OR_DIE(HT_OpenIndex(FILE_NAME, &indexDesc)); 

	int tuple_dummy;
	UpdateRecordArray array_dummy;

	Record record;
	srand(12569874);
	int r;
	printf("Insert Entries\n");
	for (int id = 0; id < RECORDS_NUM; ++id) {
		// create a record
		record.id = id;
		r = rand() % 12;
		memcpy(record.name, names[r], strlen(names[r]) + 1);
		r = rand() % 12;
		memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
		r = rand() % 10;
		memcpy(record.city, cities[r], strlen(cities[r]) + 1);

		CALL_OR_DIE(HT_InsertEntry(indexDesc, record, &tuple_dummy, &array_dummy));
		free(array_dummy.changed_tuples);
	}

	printf("RUN PrintAllEntries\n");
    CALL_OR_DIE(HT_PrintAllEntries(indexDesc, NULL)); // printing all records
	
    printf("\nRunning 2 succesful and 2 unsuccessful finds\n\n");
    for (int i=0 ; i < 2; i++) {
        int id = rand() % RECORDS_NUM; // successful finds
        CALL_OR_DIE(HT_PrintAllEntries(indexDesc, &id));
        printf("\n");
        id = RECORDS_NUM + i; //unsuccessful finds
        CALL_OR_DIE(HT_PrintAllEntries(indexDesc, &id));
        printf("\n");
    }

    printf("Running HastStatistics\n");
    HashStatistics(FILE_NAME);
	CALL_OR_DIE(HT_CloseFile(indexDesc));
	BF_Close();
    printf("\n\n\n");

}