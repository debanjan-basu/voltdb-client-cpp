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

#include "header/voltdb_helper.h"


#define __STDC_CONSTANT_MACROS
#define __STDC_LIMIT_MACROS

#include <vector>
#include <iostream>
#include <boost/shared_ptr.hpp>
#include "Client.h"
#include "Table.h"
#include "TableIterator.h"
#include "Row.hpp"
#include "WireType.h"
#include "Parameter.hpp"
#include "ParameterSet.hpp"
#include "ClientConfig.h"


static void fire ( void * client,  char * query, voltdb::InvocationResponse* resp);

void * vdb_create_client_config( char* uname, char* passwd, unsigned int security)
{
	voltdb::ClientConfig* cc = NULL;

	try
	{
		cc =  new voltdb::ClientConfig(uname, passwd, voltdb::HASH_SHA1);
	}
	catch (...)
	{
		return NULL;
	}

	return cc;
}


void * vdb_create_client( void * cc, char* host )
{
	voltdb::Client* c = NULL;
	try
	{
		 c = voltdb::Client::create(true, *(voltdb::ClientConfig*)cc);
		(*c).createConnection(host);
	}
	catch(...)
	{
		return NULL;
	}

	return c;

}

int vdb_fire_update_query( void * client,  char * query, char** resp)
{
	voltdb::InvocationResponse response;
	try
	{
		fire(client, query, &response);

	}
	catch (...)
	{
		return -1;
	}

	*resp = (char*)calloc (1 , strlen((char*)response.toString().c_str()));
	strncpy(*resp ,  (char*)response.toString().c_str(), strlen((char*)response.toString().c_str()));


	return 0;

}


int vdb_fire_read_query( void * client,  char * query, char** resp)
{

	int32_t   ii = 0;
	unsigned  jj = 0;
	voltdb::InvocationResponse response;

	try
	{
		fire(client, query, &response);
	}
	catch (...)
	{
		return -1;
	}

	*resp = (char*)calloc (1 , strlen((char*)response.toString().c_str()));
	strncpy(*resp ,  (char*)response.toString().c_str(), strlen((char*)response.toString().c_str()));

	//fetch table iterartor
	for ( jj = 0; jj < response.results().size(); jj ++)
	{
		voltdb::Table  tb         = (response.results())[jj];
		if ( 0 == tb.rowCount() )
		{
			printf("The query : %s did not return any rows", query);
			return 0;

		}
		printf("the number of rows returned by the query : %d", tb.rowCount());
		voltdb::TableIterator tit = (response.results())[jj].iterator();

		while  ( true  == tit.hasNext())
		{
			std::cout << "row index : " << std::endl;
			voltdb::Row row = tit.next();

			//for ( ii = 0; ii << row.columnCount(); ii++)
			{
				std::vector<voltdb::Column> cols = row.columns();
				const int32_t size = static_cast<int32_t>(cols.size());
				for ( ii = 0; ii < size; ii++ )
				{
					switch( cols.at(ii).m_type)
					{
					case voltdb::WIRE_TYPE_TINYINT:
						std::cout << static_cast<int32_t>(row.getInt8(ii)); break;
					case voltdb::WIRE_TYPE_SMALLINT:
						std::cout << row.getInt16(ii); break;
					case voltdb::WIRE_TYPE_INTEGER:
						std::cout << row.getInt32(ii); break;
					case voltdb::WIRE_TYPE_BIGINT:
						std::cout << row.getInt64(ii); break;
					case voltdb::WIRE_TYPE_FLOAT:
						std::cout << row.getDouble(ii); break;
					case voltdb::WIRE_TYPE_STRING:
						std::cout << "\"" << row.getString(ii) << "\""; break;
					case voltdb::WIRE_TYPE_TIMESTAMP:
						std::cout << row.getTimestamp(ii); break;
					case voltdb::WIRE_TYPE_DECIMAL:
						std::cout << row.getDecimal(ii).toString(); break;
					case voltdb::WIRE_TYPE_VARBINARY:
						std::cout << "VARBINARY VALUE"; break;
					case voltdb::WIRE_TYPE_GEOGRAPHY_POINT:
						std::cout << row.getGeographyPoint(ii).toString();
						break;
					case voltdb::WIRE_TYPE_GEOGRAPHY:
						std::cout << row.getGeography(ii).toString();
						break;
					default:
						//assert(false);
						return -1;
					}
				}
			}

		}

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


static void fire ( void * client,  char * query, voltdb::InvocationResponse* resp)
{
	voltdb::Client* pc = reinterpret_cast<voltdb::Client*>(client);


	std::vector<voltdb::Parameter> parameterTypes(1);
	parameterTypes[0] = voltdb::Parameter(voltdb::WIRE_TYPE_STRING);


	voltdb::Procedure procedure("@AdHoc", parameterTypes);
	procedure.params()->addString(query);
	*resp = pc->invoke(procedure);

}





