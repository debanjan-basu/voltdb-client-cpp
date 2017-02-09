#include "stdio.h"
#include "stdlib.h"
#include "voltdb_helper.h"


#define false 0
#define true 1

void* process_table(_UNUSED query_resp_t* qr, char* json)
{
	printf( "The json dump of table : %s", json );
	return NULL;
}

int main()
{
	char * err_msg = NULL;
	void* cc = vdb_create_client_config( (char*)"admin", (char*)"superman", 0, &err_msg);
	if ( NULL == cc)
	{
		printf("failed to create client");
		return -1;
	}

	void * c = vdb_create_client( cc, (char*)"localhost", 21212, true, &err_msg );
	if ( NULL == c)
	{
		printf("failed to create client");
		return -1;
	}


	char * resp = NULL;
	vdb_fire_upsert_query( c,
	     (char*)"CREATE TABLE Customer (\
             CustomerID INTEGER UNIQUE NOT NULL,\
             FirstName VARCHAR(15),\
             LastName VARCHAR (15),\
             PRIMARY KEY(CustomerID)\
             );", &resp, &err_msg);
	free(resp);
	if ( NULL != err_msg) return -1;
	resp = NULL;

	vdb_fire_upsert_query( c, (char*)"INSERT INTO Customer VALUES (145303, 'Jane', 'Austin' );", &resp, &err_msg );
	if (resp) free(resp);
	resp = NULL;

	vdb_fire_upsert_query( c, (char*)"INSERT INTO Customer VALUES (145304, 'Ruby', 'Austin' );", &resp, &err_msg );
	if (resp) free(resp);
	resp = NULL;

	vdb_fire_upsert_query( c, (char*)"INSERT INTO Customer VALUES (145305, 'Peter', 'Austin' );", &resp, &err_msg );
	if (resp) free(resp);
	resp = NULL;

	vdb_fire_upsert_query( c, (char*)"INSERT INTO Customer VALUES (145306, 'Peter', 'Austin' );", &resp, &err_msg );
	if (resp) free(resp);
	resp = NULL;

	vdb_fire_read_query( c, (char*)"SELECT * from Customer;", process_table, NULL, &err_msg );
	if (resp) free(resp);
	resp = NULL;


	vdb_fire_upsert_query( c, (char*)"DROP TABLE Customer;",  &resp, &err_msg  );
	if (resp) free(resp);
	resp = NULL;


	vdb_destroy_client ( c );
	vdb_destroy_client_config( cc );
	return 0;
}
