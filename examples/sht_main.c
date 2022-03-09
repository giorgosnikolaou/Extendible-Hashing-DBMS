#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bf.h"
#include "hash_file.h"
#include "sht_file.h"
#include <stdlib.h>
#define RECORDS_NUM 50 // you can change it if you want
#define NEW_RECORDS_NUM 100
#define GLOBAL_DEPT 2 // you can change it if you want

#define FILE_NAME "primary.db"
#define FILE_NAME2 "primary1.db"
#define SFILE_NAME "secondary1.db"
#define SFILE_NAME2  "secondary2.db"

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
  "Halatsis",
  "London"
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
	CALL_OR_DIE(SHT_Init());
  	int sindexDesc1 , sindexDesc2 ;
	int indexDesc;
	UpdateRecordArray update_array;
	int tupleId ;
	int names_lenght = sizeof(names) / sizeof(names[0]) ;
	int surnames_lenght = sizeof(surnames) / sizeof(surnames[0]);
	int cities_lenght = sizeof(cities) / sizeof(cities[0]);
	Record record;
	SecondaryRecord sec_record_for_sec1 , sec_record_for_sec2;
	CALL_OR_DIE(HT_CreateIndex(FILE_NAME, 2));
	CALL_OR_DIE(HT_OpenIndex(FILE_NAME, &indexDesc)); 
  	int r;

  	for (int id = 0; id < RECORDS_NUM; ++id) {
		// create a record
		record.id = id;
		r = id % names_lenght;
		memcpy(record.name, names[r], strlen(names[r]) + 1);
		r = id % surnames_lenght;
		memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
		r = id % cities_lenght;
		memcpy(record.city, cities[r], strlen(cities[r]) + 1);
		CALL_OR_DIE(HT_InsertEntry(indexDesc, record , &tupleId , &update_array) ) ;
		free(update_array.changed_tuples);
	}

	char *key1 = "surname";
	char *key2 = "city";
	printf("\nCreating secondary indexes for file %s.File %s for surname , File %s for city\n" , FILE_NAME , SFILE_NAME , SFILE_NAME2);
	SHT_CreateSecondaryIndex(SFILE_NAME, key1, 20, GLOBAL_DEPT, FILE_NAME);
	SHT_OpenSecondaryIndex(SFILE_NAME, &sindexDesc1);


	SHT_CreateSecondaryIndex(SFILE_NAME2, key2, 20, GLOBAL_DEPT, FILE_NAME);
	SHT_OpenSecondaryIndex(SFILE_NAME2, &sindexDesc2);
	
	printf("Testing both files have %d records so far\n\n" , RECORDS_NUM);

	SHT_PrintAllEntries(sindexDesc1 , NULL);
	printf("\n");
	SHT_PrintAllEntries(sindexDesc2 , NULL);

	printf("Showing %s statistics\n\n" , SFILE_NAME);
	SHT_HashStatistics(SFILE_NAME);

	printf("Showing %s statistics\n\n" , SFILE_NAME2);
	SHT_HashStatistics(SFILE_NAME2);

	printf("\nUpdating both files to %d records\n\n" , NEW_RECORDS_NUM);

  	for (int id = RECORDS_NUM ; id < NEW_RECORDS_NUM ; id++) {
		record.id = id;
    	r = id % names_lenght;
		memcpy(record.name, names[r], strlen(names[r]) + 1);
    	r = id % surnames_lenght;
		memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
    	r = id % cities_lenght;
		memcpy(record.city, cities[r], strlen(cities[r]) + 1);
		CALL_OR_DIE(HT_InsertEntry(indexDesc, record , &tupleId , &update_array) ) ;
    
		CALL_OR_DIE(SHT_SecondaryUpdateEntry(sindexDesc1 , &update_array) );
		CALL_OR_DIE(SHT_SecondaryUpdateEntry(sindexDesc2 , &update_array) );
		sec_record_for_sec1.tupleId = tupleId;  
		sec_record_for_sec2.tupleId = tupleId;
		strcpy(sec_record_for_sec1.index_key , record.surname);
		strcpy(sec_record_for_sec2.index_key , record.city);
		CALL_OR_DIE(SHT_SecondaryInsertEntry(sindexDesc1 , sec_record_for_sec1));
		CALL_OR_DIE(SHT_SecondaryInsertEntry(sindexDesc2 , sec_record_for_sec2));
		free(update_array.changed_tuples);
	}

	printf("Testing both files now have %d records after updating\n\n", NEW_RECORDS_NUM);

	SHT_PrintAllEntries(sindexDesc1 , NULL);
	printf("\n");
	SHT_PrintAllEntries(sindexDesc2 , NULL);


	/*Added surname London to join the same file on diffrent columns */
	char *index_key = "London";
	printf("\nFinding record with surname %s on file %s\n" , index_key , FILE_NAME);
	SHT_PrintAllEntries(sindexDesc1 , "London");

	printf("\nFinding record with city %s on file %s\n" , index_key , FILE_NAME);
	SHT_PrintAllEntries(sindexDesc2 , "London");

	char* surname_not_exists = "Georgiou";
	printf("\nPerforming unsuccessful find on file %s , on surname = %s\n" , FILE_NAME , surname_not_exists);
	SHT_PrintAllEntries(sindexDesc1 , surname_not_exists);

	printf("\n Joining records of same primary file on diffrent columns city and surname\n");
	SHT_InnerJoin(sindexDesc1, sindexDesc2, NULL);
	
	// Commented since result is a bit too long
	// Uncomment to test
	/*
	printf("\n Joining records of same primary file on same column\n");
	SHT_InnerJoin(sindexDesc1, sindexDesc1, NULL);*/
	HT_CloseFile(indexDesc);
	SHT_CloseSecondaryIndex(sindexDesc1);
	SHT_CloseSecondaryIndex(sindexDesc2);

 	BF_Close();

  return 0;

}
