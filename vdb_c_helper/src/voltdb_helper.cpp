/* This file is part of VoltDB.
 * Copyright (C) 2008-2016 VoltDB Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "header/base64.h"
#include "header/voltdb_helper.h"

#undef htonll
#undef ntohll



#define __STDC_CONSTANT_MACROS
#define __STDC_LIMIT_MACROS
#include <stdint-gcc.h>
#include <limits>
#include <vector>
#include <iostream>
#include <string>
#include <boost/shared_ptr.hpp>
#include "Client.h"
#include "Table.h"
#include "TableIterator.h"
#include "Row.hpp"
#include "WireType.h"
#include "Parameter.hpp"
#include "ParameterSet.hpp"
#include "ClientConfig.h"
#include "string.h"

#define COMMA                  ","
#define COLON                  ":"
#define QOUTES_BEGIN           "\""
#define QOUTES_END QOUTES_BEGIN
#define CURLY_BRACES_OPEN      "{"
#define CURLY_BRACES_CLOSE     "}"
#define SQUARE_BRACES_OPEN     "["
#define SQUARE_BRACES_CLOSE    "]"

static const char * unknown_err = "voltdb unknown error occurred";

static void fire ( void * client,  char * query, voltdb::InvocationResponse* resp);
int toBSON ( voltdb::InvocationResponse* resp, process_rows cb,  query_resp_t* arg );
int toJSON ( voltdb::InvocationResponse* resp, process_rows cb,  query_resp_t* arg );
static int get_rows_as_json(std::ostringstream & ostream, voltdb::TableIterator tit, uint64_t max_size );
//static char * get_errmsg(const char * __function__, const char * errmsg);


void * vdb_create_client_config( char* uname, char* passwd, _UNUSED unsigned int auth_type, char ** err_msg )
{
	voltdb::ClientConfig* cc = NULL;

	try
	{
		cc =  new voltdb::ClientConfig(uname, passwd, voltdb::HASH_SHA1);
	}
    catch( voltdb::Exception& e )
    {
        *err_msg = strdup(e.what());
        return NULL;
    }
	catch (...)
	{
        *err_msg = strdup( unknown_err);
		return NULL;
	}

	return cc;
}


void * vdb_create_client( void * cc, char* host, unsigned short port, unsigned keepconnecting, char ** err_msg )
{
	voltdb::Client* c = NULL;
	try
	{
		 c = voltdb::Client::create(true, *(voltdb::ClientConfig*)cc);
		(*c).createConnection(host, port, keepconnecting);
	}
	catch( voltdb::ConnectException& e )
	{
	     *err_msg = strdup(e.what());
	     return NULL;
	}
    catch( voltdb::LibEventException& e )
    {
         *err_msg = strdup(e.what());
         return NULL;
    }
	catch( voltdb::Exception& e )
	{
        *err_msg = strdup(e.what());
	    return NULL;
	}
	catch(...)
	{
        *err_msg = strdup(unknown_err);
		return NULL;
	}

	return c;

}

int vdb_fire_upsert_query( void * client,  char * query, char** resp, char ** err_msg)
{
	voltdb::InvocationResponse response;
	try
	{
		fire(client, query, &response);

	}
    catch( voltdb::NoConnectionsException& e)
    {
        *err_msg = strdup(e.what());
        return -1;
    }
    catch( voltdb::UninitializedParamsException& e)
    {
        *err_msg = strdup(e.what());
        return -1;
    }
    catch( voltdb::LibEventException& e)
    {
        *err_msg = strdup(e.what());
        return -1;
    }
    catch( voltdb::Exception& e)
    {
        *err_msg = strdup(e.what());
        return -1;
    }
	catch (...)
	{
        *err_msg = strdup(unknown_err);
		return -1;
	}



	*resp = (char*)calloc (1 , strlen((char*)response.toString().c_str()));
	strncpy(*resp ,  (char*)response.toString().c_str(), strlen((char*)response.toString().c_str()));

	if (response.failure())
	{
		*err_msg = strdup(response.statusString().c_str());
		return -1;
	}
	return 0;

}

int vdb_fire_read_query( void * client,  char * query, process_rows cb, query_resp_t* arg, char ** err_msg )
{

	voltdb::InvocationResponse response;

    try
    {
	fire(client, query, &response);
    }
    catch( voltdb::NoConnectionsException& e)
    {
        *err_msg = strdup(e.what());
        return -1;
    }
    catch( voltdb::UninitializedParamsException& e)
    {
        *err_msg = strdup(e.what());
        return -1;
    }
    catch( voltdb::LibEventException& e)
    {
        *err_msg = strdup(e.what());
        return -1;
    }
    catch( voltdb::Exception& e)
    {
        *err_msg = strdup(e.what());
        return -1;
    }
    catch (...)
    {
        *err_msg = strdup(unknown_err);
        return -1;
    }

    try
    {
	//FIXME: function should throw exception in error cases which should be caught and errmsg shall be returned.
	toJSON( &response, cb, arg);    
    }
    catch( voltdb::Exception& e )
    {
        *err_msg = strdup( e.what());
        return -1;
    }
	catch (...)
	{
        *err_msg = strdup(unknown_err);
		return -1;
	}

	return 0;

}

void  vdb_destroy_client ( void * c )
{
	delete (voltdb::Client*)c;
}

void  vdb_destroy_client_config( void * cc )
{
	delete (voltdb::ClientConfig*)cc;
}


void fire ( void * client,  char * query, voltdb::InvocationResponse* resp)
{
	voltdb::Client* pc = reinterpret_cast<voltdb::Client*>(client);


	std::vector<voltdb::Parameter> parameterTypes(1);
	parameterTypes[0] = voltdb::Parameter(voltdb::WIRE_TYPE_STRING);


	voltdb::Procedure procedure("@AdHoc", parameterTypes);
	procedure.params()->addString(query);
	*resp = pc->invoke(procedure);

}

int toBSON ( _UNUSED voltdb::InvocationResponse* resp, _UNUSED process_rows cb,  _UNUSED query_resp_t* arg )
{
     return 0;
}

int toJSON ( voltdb::InvocationResponse* resp, process_rows cb,  query_resp_t* arg )
{
	//fetch table iterartor
	for ( uint64_t jj = 0; jj < resp->results().size(); jj ++)
	{
		voltdb::Table  tb         = (resp->results())[jj];
		if ( 0 == tb.rowCount() ) //result set is null
		{
		    return 0;
		}

		voltdb::TableIterator tit = (resp->results())[jj].iterator();
		std::ostringstream ostream;

		//start the json document creation
		ostream << CURLY_BRACES_OPEN;

		if ( NULL == arg->table_name)
		{
			return -1;
		}
		ostream << QOUTES_BEGIN << "TABLE_NAME" << QOUTES_END << COLON << QOUTES_BEGIN << arg->table_name << QOUTES_END << COMMA;
		ostream << QOUTES_BEGIN << "COL_DESC" << QOUTES_END <<   COLON ;

		ostream << SQUARE_BRACES_OPEN;


		if ( true  == tit.hasNext())
		{
			voltdb::Row row = tit.next();
			std::vector<voltdb::Column> cols = row.columns();
			const int32_t size = static_cast<int32_t>(cols.size());
			(arg)->final_size = (uint64_t)size;


			for ( int ii = 0; ii < size; ii++ )
			{
				switch( cols.at(ii).m_type )
				{
					case voltdb::WIRE_TYPE_TINYINT:
					case voltdb::WIRE_TYPE_SMALLINT:
					case voltdb::WIRE_TYPE_INTEGER:
					{
						ostream << CURLY_BRACES_OPEN;

						ostream << QOUTES_BEGIN;
						ostream << "COL_NAME";
						ostream << QOUTES_END;
						ostream << COLON;
						ostream << QOUTES_BEGIN;
						ostream << (cols.at(ii).m_name.c_str());
						ostream << QOUTES_END;

						ostream << COMMA;

						ostream << QOUTES_BEGIN;
						ostream << "COL_TYPE";
						ostream << QOUTES_END;
						ostream << COLON;
						ostream << "INT32";

						ostream << CURLY_BRACES_CLOSE;

						if( ii < (size -1)) ostream << COMMA;
						break;
					}
					case voltdb::WIRE_TYPE_BIGINT:
					case voltdb::WIRE_TYPE_TIMESTAMP:
					{
						ostream << CURLY_BRACES_OPEN;

						ostream << QOUTES_BEGIN;
						ostream << "COL_NAME";
						ostream << QOUTES_END;
						ostream << COLON;
						ostream << QOUTES_BEGIN;
						ostream << (cols.at(ii).m_name.c_str());
						ostream << QOUTES_END;

						ostream << COMMA;

						ostream << QOUTES_BEGIN;
						ostream << "COL_TYPE";
						ostream << QOUTES_END;
						ostream << COLON;
						ostream << "INT64";

						ostream << CURLY_BRACES_CLOSE;

						if( ii < (size -1)) ostream << COMMA;
						break;
					}
					case voltdb::WIRE_TYPE_FLOAT:
					{
						ostream << CURLY_BRACES_OPEN;

						ostream << QOUTES_BEGIN;
						ostream << "COL_NAME";
						ostream << QOUTES_END;
						ostream << COLON;
						ostream << QOUTES_BEGIN;
						ostream << (cols.at(ii).m_name.c_str());
						ostream << QOUTES_END;

						ostream << COMMA;

						ostream << QOUTES_BEGIN;
						ostream << "COL_TYPE";
						ostream << QOUTES_END;
						ostream << COLON;
						ostream << "DOUBLE_64";

						ostream << CURLY_BRACES_CLOSE;

						if( ii < (size -1)) ostream << COMMA;
						break;
					}
					case voltdb::WIRE_TYPE_STRING:
					{
						ostream << CURLY_BRACES_OPEN;

						ostream << QOUTES_BEGIN;
						ostream << "COL_NAME";
						ostream << QOUTES_END;
						ostream << COLON;
						ostream << QOUTES_BEGIN;
						ostream << (cols.at(ii).m_name.c_str());
						ostream << QOUTES_END;

						ostream << COMMA;

						ostream << QOUTES_BEGIN;
						ostream << "COL_TYPE";
						ostream << QOUTES_END;
						ostream << COLON;
						ostream << "UTF8";

						ostream << CURLY_BRACES_CLOSE;

						if( ii < (size -1)) ostream << COMMA;
						break;
					}
					case voltdb::WIRE_TYPE_VARBINARY:
					{
						ostream << CURLY_BRACES_OPEN;

						ostream << QOUTES_BEGIN;
						ostream << "COL_NAME";
						ostream << QOUTES_END;
						ostream << COLON;
						ostream << QOUTES_BEGIN;
						ostream << (cols.at(ii).m_name.c_str());
						ostream << QOUTES_END;

						ostream << COMMA;

						ostream << QOUTES_BEGIN;
						ostream << "COL_TYPE";
						ostream << QOUTES_END;
						ostream << COLON;
						ostream << "BINARY"; //generic binary

						ostream << CURLY_BRACES_CLOSE;

						if( ii < (size -1)) ostream << COMMA;
						break;
					}
					case voltdb::WIRE_TYPE_GEOGRAPHY_POINT:
					{
						break;
					}
					case voltdb::WIRE_TYPE_DECIMAL:
					{
						return -1;
						break;
					}
					case voltdb::WIRE_TYPE_GEOGRAPHY:
					{
						return -1;
						break;
					}
					default:
					{
						//assert(false);
						return -1;
						break;
					}
				}
			}
		}
		ostream << SQUARE_BRACES_CLOSE;

		int rc = get_rows_as_json(ostream, (resp->results())[jj].iterator(), (arg)->max_size);
		if (0 != rc )
		{
			ostream << CURLY_BRACES_CLOSE;
			return -1;
		}

		ostream << CURLY_BRACES_CLOSE;

		char * json = strdup(ostream.str().c_str());
		if (json == NULL)
		{
			//std::cout << "json is null" << std::endl;
			return -1;
		}
		cb(arg, json);
	}
	return 0;
}


static int get_rows_as_json(std::ostringstream & ostream, voltdb::TableIterator tit, uint64_t max_size )
{
	uint64_t nrows = 0;

	if ( false  == tit.hasNext()) return -1;

	ostream << COMMA;
	ostream << QOUTES_BEGIN << "ROWS" << QOUTES_END << COLON;
	ostream << SQUARE_BRACES_OPEN;

	while( true  == tit.hasNext() && nrows < max_size )
	{
		nrows ++;
		voltdb::Row row = tit.next();
		std::vector<voltdb::Column> cols = row.columns();
		const int32_t size = static_cast<int32_t>(cols.size());
		//(arg)->final_size = (uint64_t)size;

		ostream << SQUARE_BRACES_OPEN;
		for ( int ii = 0; ii < size; ii++ )
		{
			switch( cols.at(ii).m_type )
			{
				case voltdb::WIRE_TYPE_TINYINT:
				{
				    ostream <<   static_cast<int32_t>(row.getInt8(ii));
	                            if( ii < (size -1)) ostream << COMMA;
				    break;
				}
				case voltdb::WIRE_TYPE_SMALLINT:
				{
				    ostream <<   static_cast<int32_t>(row.getInt16(ii));
				    if( ii < (size -1)) ostream << COMMA;
				    break;
				}
				case voltdb::WIRE_TYPE_INTEGER:
				{
				     ostream <<   static_cast<int32_t>(row.getInt32(ii));
				     if( ii < (size -1)) ostream << COMMA;
				     break;
			    }
			    case voltdb::WIRE_TYPE_BIGINT:
		 	    {
		 	    	 ostream <<   static_cast<int64_t>(row.getInt64(ii));
				 if( ii < (size -1)) ostream << COMMA;
				 break;
			    }
			    case voltdb::WIRE_TYPE_TIMESTAMP:
		 	    {
		 	    	 ostream <<   static_cast<int64_t>(row.getTimestamp(ii));
				 if( ii < (size -1)) ostream << COMMA;
				 break;
			    }
			    case voltdb::WIRE_TYPE_FLOAT:
			    {
			    	 ostream <<   static_cast<int32_t>(row.getDouble(ii));
				 if( ii < (size -1)) ostream << COMMA;
				 break;
			    }
			    case voltdb::WIRE_TYPE_STRING:
			    {
			    	 ostream << QOUTES_BEGIN;
			    	 ostream << (char*)row.getString(ii).c_str();
			    	 ostream << QOUTES_END;
				 if( ii < (size -1)) ostream << COMMA;
				 break;
			    }
			    case voltdb::WIRE_TYPE_DECIMAL:
			    {
				     return -1;
				     break;
			    }
			    case voltdb::WIRE_TYPE_VARBINARY:
			    {

			    	uint8_t outbuf[BUFSIZ];
			    	char b64encbuf[BUFSIZ];
			    	memset(b64encbuf, 0x00, sizeof(BUFSIZ));
			    	memset(outbuf, 0x00, sizeof(BUFSIZ));
			    	int32_t len = BUFSIZ;
			    	row.getVarbinary(ii, BUFSIZ - 1, outbuf, &len);

			    	if (0 == b64::b64_encode(outbuf, len, b64encbuf, BUFSIZ -1 ))
			    	{
			    		return -1;
			    	}

			    	ostream << QOUTES_BEGIN;
			    	ostream << b64encbuf;
			    	ostream << QOUTES_END;

				break;
			    }
			    case voltdb::WIRE_TYPE_GEOGRAPHY_POINT:
			    {
				     return -1;
				     break;
			    }
			    case voltdb::WIRE_TYPE_GEOGRAPHY:
			    {
				     return -1;
				     break;
			    }
			    default:
			    {
				     //assert(false);
				     return -1;
				     break;
			    }
			}
		}
		ostream << SQUARE_BRACES_CLOSE;
		if (true  == tit.hasNext()) ostream << COMMA;
	}
	ostream << SQUARE_BRACES_CLOSE;

	ostream << COMMA;
	ostream << QOUTES_BEGIN << "NROWS" << QOUTES_END << COLON << nrows;


	return 0;
}

/*
char * get_errmsg(const char * __function__, const char * errmsg)
{
    char err_msg[BUFSIZ] = {0};
    snprintf(err_msg , sizeof(errmsg)-1 , "%s err: %s", __function__, errmsg);
    strdup(err_msg);

    return NULL;
}
*/
