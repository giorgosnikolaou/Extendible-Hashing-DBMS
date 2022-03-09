#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bf.h"
#include "hash_file.h"

#define RECORDS_NUM 65540 // you can change it if you want
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
  int a;
  UpdateRecordArray b;

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

		CALL_OR_DIE(HT_InsertEntry(indexDesc, record, &a, &b));
	}

  HashStatistics(FILE_NAME);

	printf("RUN PrintAllEntries\n");
	int id = rand() % RECORDS_NUM;
	CALL_OR_DIE(HT_PrintAllEntries(indexDesc, &id));
	CALL_OR_DIE(HT_PrintAllEntries(indexDesc, NULL));

  //print_file(indexDesc);


	CALL_OR_DIE(HT_CloseFile(indexDesc));
	BF_Close();

}
