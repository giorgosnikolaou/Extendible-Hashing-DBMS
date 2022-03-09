#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bf.h"
#include "hash_file.h"
#include "sht_file.h"
#include <stdlib.h>
#include "../../include/essentials.h"
#define RECORDS_NUM 1000 // you can change it if you want
#define GLOBAL_DEPT 2 // you can change it if you want

#define FILE_NAME "primary1.db"
#define FILE_NAME2 "primary2.db"
#define FILE_NAME3 "primary3.db"
#define FILE_NAME4 "primary4.db"
#define FILE_NAME5 "primary5.db"
#define FILE_NAME6 "primary6.db"
#define SFILE_NAME "secondary1.db"
#define SFILE_NAME2  "secondary2.db"
#define SFILE_NAME3 "secondary3.db"
#define SFILE_NAME4 "secondary4.db"
#define SFILE_NAME5 "secondary5.db"
#define SFILE_NAME6 "secondary6.db"



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

    int ind1;
	  int indexDesc1 , indexDesc2;
    UpdateRecordArray up , up2;
    int mpla , mpla2;
    Record record;
    //SecondaryRecord rec;

	  CALL_OR_DIE(HT_CreateIndex(FILE_NAME, 2));
    CALL_OR_DIE(HT_CreateIndex(FILE_NAME2, 2));
	  CALL_OR_DIE(HT_OpenIndex(FILE_NAME, &indexDesc1)); 
    CALL_OR_DIE(HT_OpenIndex(FILE_NAME2, &indexDesc2));
    
    int r;
    int ht_1_count = 200;
    int ht_2_count = 250;
    int oxford_count1 = 0;
    int saline_count1 = 0;
    int saint_boswells_count1 = 0;
    int north_petherton_count1 = 0;

    for (int id = 0; id <ht_1_count; ++id) {
		// create a record
		record.id = id;
		r = rand() % 40;
		memcpy(record.name, names[r], strlen(names[r]) + 1);
		r = rand() % 40;
		memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
		r = rand() % 40;
        if(strcmp(cities[r], "Oxford") == 0) oxford_count1++;
        if(strcmp(cities[r], "Saline") == 0) saline_count1++;
        if(strcmp(cities[r], "Saint Boswells") == 0) saint_boswells_count1++;
        if(strcmp(cities[r], "North Petherton") == 0) north_petherton_count1++;
		memcpy(record.city, cities[r], strlen(cities[r]) + 1);

		CALL_OR_DIE(HT_InsertEntry(indexDesc1, record , &mpla , &up) ) ;
		free(up.changed_tuples);
	}

    int oxford_count2 = 0;
    int saline_count2 = 0;
    int saint_boswells_count2 = 0;
    int north_petherton_count2 = 0;

    for (int id = ht_1_count; id <ht_1_count + ht_2_count; ++id) {
		// create a record
		record.id = id;
		r = rand() % 40;
		memcpy(record.name, names[r], strlen(names[r]) + 1);
		r = rand() % 40;
		memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
		r = rand() % 40;
        if(strcmp(cities[r], "Oxford") == 0) oxford_count2++;
        if(strcmp(cities[r], "Saline") == 0) saline_count2++;
        if(strcmp(cities[r], "Saint Boswells") == 0) saint_boswells_count2++;
        if(strcmp(cities[r], "North Petherton") == 0) north_petherton_count2++;
		memcpy(record.city, cities[r], strlen(cities[r]) + 1);

		CALL_OR_DIE(HT_InsertEntry(indexDesc2, record , &mpla2 , &up2) ) ;
		free(up2.changed_tuples);
	}

    char *key = "city";
    int  ind2;
    CALL_OR_DIE(SHT_CreateSecondaryIndex(SFILE_NAME, key , 20, 2, FILE_NAME));
    CALL_OR_DIE(SHT_OpenSecondaryIndex(SFILE_NAME, &ind1)) ;
    
    CALL_OR_DIE(SHT_CreateSecondaryIndex(SFILE_NAME2, key , 20, 2, FILE_NAME2));
    CALL_OR_DIE(SHT_OpenSecondaryIndex(SFILE_NAME2, &ind2)) ;

    //  print_sec_file(ind);
    // printf("\nPrint all entries of 1st SHT with city=='Oxford', which must be %d:\n", oxford_count1);
    // SHT_PrintAllEntries(ind1 , "Oxford");
    // printf("\nPrint all entries of 1st SHT with city=='Saline', which must be %d:\n", saline_count1);
    // SHT_PrintAllEntries(ind1 , "Saline");
    // printf("\nPrint all entries of 1st SHT with city=='Saint Boswells', which must be %d:\n", saint_boswells_count1);
    // SHT_PrintAllEntries(ind1 , "Saint Boswells");
    // printf("\nPrint all entries of 2nd SHT with city=='North Petherton', which must be %d:\n", north_petherton_count2);
    // SHT_PrintAllEntries(ind2 , "North Petherton");
    // printf("\nPrint all entries of 2nd SHT, which must be %d:\n", ht_2_count);
    // SHT_PrintAllEntries(ind2 , NULL);

    printf("\nPrint inner join on city=='Oxford', which must have %d results:\n", oxford_count1*oxford_count2);
    SHT_InnerJoin(ind1, ind2, "Oxford");

    printf("\nPrint inner join on city=='Saline', which must have %d results:\n", saline_count1*saline_count2);
    SHT_InnerJoin(ind1, ind2, "Saline");

    printf("\nPrint inner join on city=='Saint Boswells', which must have %d results:\n", saint_boswells_count1*saint_boswells_count2);
    SHT_InnerJoin(ind1, ind2, "Saint Boswells");

    printf("\nPrint inner join on city=='North Petherton', which must have %d results:\n", north_petherton_count1*north_petherton_count2);
    SHT_InnerJoin(ind1, ind2, "North Petherton");

    CALL_OR_DIE(SHT_CloseSecondaryIndex(ind1)) ;
    
    CALL_OR_DIE(SHT_CloseSecondaryIndex(ind2)) ;

    CALL_OR_DIE(HT_CloseFile(indexDesc1)); 
  	CALL_OR_DIE(HT_CloseFile(indexDesc2)); 

	  CALL_OR_DIE(HT_CreateIndex(FILE_NAME3, 2));
    CALL_OR_DIE(HT_CreateIndex(FILE_NAME4, 2));
   
	  CALL_OR_DIE(HT_OpenIndex(FILE_NAME3, &indexDesc1)); 
  	CALL_OR_DIE(HT_OpenIndex(FILE_NAME4, &indexDesc2)); 
   

    ht_1_count = 20;
    ht_2_count = 25;
    int colon_count1 = 0;
    int frank_count1 = 0;
    int hudson_count1 = 0;
    int dawson_count1 = 0;
    int mccarty_count1 = 0;

    for (int id = 0; id <ht_1_count; ++id) {
		// create a record
		record.id = id;
		r = rand() % 5;
		memcpy(record.name, names[r], strlen(names[r]) + 1);
		r = rand() % 5;
        if(strcmp(surnames[r], "Frank") == 0) frank_count1++;
        if(strcmp(surnames[r], "Colon") == 0) colon_count1++;
        if(strcmp(surnames[r], "Hudson") == 0) hudson_count1++;
        if(strcmp(surnames[r], "Dawson") == 0) dawson_count1++;
        if(strcmp(surnames[r], "Mccarthy") == 0) mccarty_count1++;
		memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
		r = rand() % 5;
		memcpy(record.city, cities[r], strlen(cities[r]) + 1);

		CALL_OR_DIE(HT_InsertEntry(indexDesc1, record , &mpla , &up) ) ;
		free(up.changed_tuples);
	}

    int colon_count2 = 0;
    int frank_count2 = 0;
    int hudson_count2 = 0;
    int dawson_count2 = 0;
    int mccarty_count2 = 0;
    
    for (int id = ht_1_count; id <ht_1_count + ht_2_count; ++id) {
		// create a record
		record.id = id;
		r = rand() % 5;
		memcpy(record.name, names[r], strlen(names[r]) + 1);
		r = rand() % 5;
        if(strcmp(surnames[r], "Frank") == 0) frank_count2++;
        if(strcmp(surnames[r], "Colon") == 0) colon_count2++;
        if(strcmp(surnames[r], "Hudson") == 0) hudson_count2++;
        if(strcmp(surnames[r], "Dawson") == 0) dawson_count2++;
        if(strcmp(surnames[r], "Mccarthy") == 0) mccarty_count2++;
		memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
		r = rand() % 5;
		memcpy(record.city, cities[r], strlen(cities[r]) + 1);

		CALL_OR_DIE(HT_InsertEntry(indexDesc2, record , &mpla2 , &up2) ) ;
		free(up2.changed_tuples);
	}

    key = "surname";
    CALL_OR_DIE(SHT_CreateSecondaryIndex(SFILE_NAME3, key , 20, 2, FILE_NAME3));
    CALL_OR_DIE(SHT_OpenSecondaryIndex(SFILE_NAME3, &ind1)) ;
    
    CALL_OR_DIE(SHT_CreateSecondaryIndex(SFILE_NAME4, key , 20, 2, FILE_NAME4));
    CALL_OR_DIE(SHT_OpenSecondaryIndex(SFILE_NAME4, &ind2)) ;

    // SHT_PrintAllEntries(ind1 , NULL);
    printf("\nPrint inner join on surname=='Frank', which must have %d results:\n", frank_count1*frank_count2);
    SHT_InnerJoin(ind1, ind2, "Frank");

    printf("\nPrint inner join on surname=='Colon', which must have %d results:\n", colon_count1*colon_count2);
    SHT_InnerJoin(ind1, ind2, "Colon");

    printf("\nPrint inner join on surname=='Hudson', which must have %d results:\n", hudson_count1*hudson_count2);
    SHT_InnerJoin(ind1, ind2, "Hudson");

    printf("\nPrint inner join on surname==NULL, which must have %d results:\n", colon_count1*colon_count2+frank_count1*frank_count2+hudson_count1*hudson_count2+dawson_count1*dawson_count2+mccarty_count1*mccarty_count2);
    SHT_InnerJoin(ind1, ind2, NULL);

    CALL_OR_DIE(SHT_CloseSecondaryIndex(ind1)) ;
    
    CALL_OR_DIE(SHT_CloseSecondaryIndex(ind2)) ;

    CALL_OR_DIE(HT_CloseFile(indexDesc1)); 
  	CALL_OR_DIE(HT_CloseFile(indexDesc2)); 




    CALL_OR_DIE(HT_CreateIndex(FILE_NAME5, 2));
    CALL_OR_DIE(HT_CreateIndex(FILE_NAME6, 2));
   
	  CALL_OR_DIE(HT_OpenIndex(FILE_NAME5, &indexDesc1)); 
  	CALL_OR_DIE(HT_OpenIndex(FILE_NAME6, &indexDesc2)); 
   

    ht_1_count = 50;
    ht_2_count = 50;

    int common1_count = 0;
    int common2_count =0;
    char *common = "Athens";
    for (int id = 0; id <ht_1_count; ++id) {
		// create a record
		record.id = id;
		r = rand() % 5;
		memcpy(record.name, names[r], strlen(names[r]) + 1);
    if (id %10 == 0 ) {
      memcpy(record.surname , common , strlen(common)+1);
      common1_count++;
    }
    else {
		  r = rand() % 5;
		  memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
    }
		r = rand() % 5;
		memcpy(record.city, cities[r], strlen(cities[r]) + 1);

		CALL_OR_DIE(HT_InsertEntry(indexDesc1, record , &mpla , &up) ) ;
		free(up.changed_tuples);
	}
    
    for (int id = ht_1_count; id <ht_1_count + ht_2_count; ++id) {
		// create a record
		record.id = id;
		r = rand() % 5;
		memcpy(record.name, names[r], strlen(names[r]) + 1);
		r = rand() % 5;
		memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
		r = rand() % 5;
    if (id %10 == 0 ) {
      memcpy(record.city , common , strlen(common) + 1);
      common2_count++;
    }
    else {
		  memcpy(record.city, cities[r], strlen(cities[r]) + 1);
    }

		CALL_OR_DIE(HT_InsertEntry(indexDesc2, record , &mpla2 , &up2) ) ;
		free(up2.changed_tuples);
	}


  char *key1 = "surname";
  char *key2 = "city";

  CALL_OR_DIE(SHT_CreateSecondaryIndex(SFILE_NAME5, key1 , 20, 2, FILE_NAME5));
  CALL_OR_DIE(SHT_OpenSecondaryIndex(SFILE_NAME5, &ind1)) ;
    
  CALL_OR_DIE(SHT_CreateSecondaryIndex(SFILE_NAME6, key2 , 20, 2, FILE_NAME6));
  CALL_OR_DIE(SHT_OpenSecondaryIndex(SFILE_NAME6, &ind2)) ;

  printf("\nPerforming join on columns surname = city of files %s , %s which should have %d results\n" , FILE_NAME5 , FILE_NAME6 
  , common1_count * common1_count);

  SHT_InnerJoin(ind1 , ind2 , NULL);

  CALL_OR_DIE(SHT_CloseSecondaryIndex(ind1)) ;
    
  CALL_OR_DIE(SHT_CloseSecondaryIndex(ind2)) ;

  CALL_OR_DIE(HT_CloseFile(indexDesc1)); 
  CALL_OR_DIE(HT_CloseFile(indexDesc2)); 

    

	BF_Close();

}
