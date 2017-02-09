#ifndef __VDB_HELPER__
#define __VDB_HELPER__

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Marks following parameter as not used in function call, to remove warnings
 */
#define _UNUSED         __attribute__((unused))

//output params
typedef struct query_resp
{
	char * 		table_name;
	uint64_t 	max_size;   //max_size it can hold
	uint64_t 	final_size; //final size after populatation
	char *  	cumulative_resp;
	int 		sock_fd;    // keep provision for publishing data to socket
} query_resp_t;


typedef void* (*process_rows)( query_resp_t*, char* );

void * vdb_create_client_config( char* uname, char* passwd, unsigned int conn_type, char ** err_msg );

void * vdb_create_client( void * cc, char* host, unsigned short port, unsigned keepconnecting, char ** err_msg );

//Supports : C=Create U=Update D=Delete
int vdb_fire_upsert_query( void * client,  char * query, char ** response, char ** err_msg );

//Supports : R=Read, 50 MB limit to the amount of data volt db returns from a query
int vdb_fire_read_query( void * client,  char * query, process_rows cb,  query_resp_t* cb_arg, char ** err_msg);

void  vdb_destroy_client ( void * c );

void  vdb_destroy_client_config( void * cc );


#ifdef __cplusplus
}
#endif

#endif /* __VDB_HELPER__ */
