/* This file is part of VoltDB.
 * Copyright (C) 2008-2010 VoltDB L.L.C.
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
#include <cppunit/TestCase.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestFixture.h>
#include "Client.h"
#include "MockVoltDB.h"
#include "StatusListener.h"
#include "Parameter.hpp"
#include "ParameterSet.hpp"
#include "Procedure.hpp"
#include "WireType.h"
#include <vector>
#include "InvocationResponse.hpp"

namespace voltdb {

class DelegatingListener : public StatusListener {
public:
    StatusListener *m_listener;
   virtual bool uncaughtException(
           std::exception exception,
           boost::shared_ptr<voltdb::ProcedureCallback> callback) {
       if (m_listener != NULL) {
           return m_listener->uncaughtException(exception, callback);
       }
       return false;
   }
   virtual bool connectionLost(std::string hostname, int32_t connectionsLeft) {
       if (m_listener != NULL) {
           return m_listener->connectionLost(hostname, connectionsLeft);
       }
       return false;
   }
   virtual bool backpressure(bool hasBackpressure) {
       if (m_listener != NULL) {
           return m_listener->backpressure(hasBackpressure);
       }
       return false;
   }
};

class ClientTest : public CppUnit::TestFixture {
CPPUNIT_TEST_SUITE( ClientTest );
CPPUNIT_TEST( testConnect );
CPPUNIT_TEST( testSyncInvoke );
CPPUNIT_TEST_EXCEPTION( testInvokeSyncNoConnections, voltdb::NoConnectionsException );
CPPUNIT_TEST_EXCEPTION( testInvokeAsyncNoConnections, voltdb::NoConnectionsException );
CPPUNIT_TEST_EXCEPTION( testRunNoConnections, voltdb::NoConnectionsException );
CPPUNIT_TEST_EXCEPTION( testRunOnceNoConnections, voltdb::NoConnectionsException );
CPPUNIT_TEST_EXCEPTION( testNullCallback, voltdb::NullPointerException );
CPPUNIT_TEST_EXCEPTION( testLostConnection, voltdb::NoConnectionsException );
CPPUNIT_TEST( testLostConnectionBreaksEventLoop );
CPPUNIT_TEST( testBreakEventLoopViaCallback );
CPPUNIT_TEST( testCallbackThrows );
CPPUNIT_TEST( testBackpressure );
CPPUNIT_TEST_SUITE_END();

public:
    void setUp() {
        m_dlistener = new boost::shared_ptr<DelegatingListener>(new DelegatingListener());
        m_client = new boost::shared_ptr<Client>(Client::create(*m_dlistener));
        m_voltdb = new MockVoltDB(*m_client);
    }

    void tearDown() {
        delete m_voltdb;
        delete m_client;
        delete m_dlistener;
    }

    void testConnect() {
        (*m_client)->createConnection("localhost", "hello", "world");
    }

    void testSyncInvoke() {
        (*m_client)->createConnection("localhost", "hello", "world");
        std::vector<Parameter> signature;
        signature.push_back(Parameter(WIRE_TYPE_STRING));
        signature.push_back(Parameter(WIRE_TYPE_STRING));
        signature.push_back(Parameter(WIRE_TYPE_STRING));
        Procedure proc("Insert", signature);
        ParameterSet *params = proc.params();
        params->addString("Hello").addString("World").addString("English");
        m_voltdb->filenameForNextMessage("invocation_response_success.msg");
        boost::shared_ptr<InvocationResponse> response = (*m_client)->invoke(proc);
        CPPUNIT_ASSERT(response->success());
        CPPUNIT_ASSERT(response->statusString() == "");
        CPPUNIT_ASSERT(response->appStatusCode() == -128);
        CPPUNIT_ASSERT(response->appStatusString() == "");
        CPPUNIT_ASSERT(response->results().size() == 0);
    }

    class SyncCallback : public ProcedureCallback {
    public:
        boost::shared_ptr<InvocationResponse> m_response;
        virtual bool callback(boost::shared_ptr<InvocationResponse> response) throw (voltdb::Exception) {
            m_response = response;
            return false;
        }
    };

    void testAsyncInvoke() {
        (*m_client)->createConnection("localhost", "hello", "world");
        std::vector<Parameter> signature;
        signature.push_back(Parameter(WIRE_TYPE_STRING));
        signature.push_back(Parameter(WIRE_TYPE_STRING));
        signature.push_back(Parameter(WIRE_TYPE_STRING));
        Procedure proc("Insert", signature);
        ParameterSet *params = proc.params();
        params->addString("Hello").addString("World").addString("English");
        m_voltdb->filenameForNextMessage("invocation_response_success.msg");

        SyncCallback *cb = new SyncCallback();
        boost::shared_ptr<ProcedureCallback> callback(cb);
        (*m_client)->invoke(proc, callback);
        while (cb->m_response.get() == NULL) {
            (*m_client)->runOnce();
        }
        boost::shared_ptr<InvocationResponse> response = cb->m_response;
        CPPUNIT_ASSERT(response->success());
        CPPUNIT_ASSERT(response->statusString() == "");
        CPPUNIT_ASSERT(response->appStatusCode() == -128);
        CPPUNIT_ASSERT(response->appStatusString() == "");
        CPPUNIT_ASSERT(response->results().size() == 0);
    }

    void testInvokeSyncNoConnections() {
        std::vector<Parameter> signature;
        Procedure proc("foo", signature);
        proc.params();
        (*m_client)->invoke(proc);
    }

    void testInvokeAsyncNoConnections() {
        std::vector<Parameter> signature;
        Procedure proc("foo", signature);
        proc.params();
        SyncCallback *cb = new SyncCallback();
        boost::shared_ptr<ProcedureCallback> callback(cb);
        (*m_client)->invoke(proc, callback);
    }

    void testRunNoConnections() {
        (*m_client)->run();
    }

    void testRunOnceNoConnections() {
        (*m_client)->runOnce();
    }

    void testNullCallback() {
        std::vector<Parameter> signature;
        Procedure proc("foo", signature);
        proc.params();
        (*m_client)->invoke(proc, boost::shared_ptr<ProcedureCallback>());
    }

    void testLostConnection() {
        class Listener : public StatusListener {
        public:
            bool reported;
           Listener() : reported(false) {}
            virtual bool uncaughtException(
                    std::exception exception,
                    boost::shared_ptr<voltdb::ProcedureCallback> callback) {
                CPPUNIT_ASSERT(false);
                return false;
            }
            virtual bool connectionLost(std::string hostname, int32_t connectionsLeft) {
                reported = true;
                CPPUNIT_ASSERT(connectionsLeft == 0);
                return false;
            }
            virtual bool backpressure(bool hasBackpressure) {
                CPPUNIT_ASSERT(false);
                return false;
            }
        }  listener;
        (*m_dlistener)->m_listener = &listener;
        (*m_client)->createConnection("localhost", "hello", "world");

        std::vector<Parameter> signature;
        Procedure proc("Insert", signature);
        proc.params();

        SyncCallback *cb = new SyncCallback();
        boost::shared_ptr<ProcedureCallback> callback(cb);
        (*m_client)->invoke(proc, callback);
        m_voltdb->hangupOnNextRequest();

        proc.params();
        boost::shared_ptr<InvocationResponse> syncResponse = (*m_client)->invoke(proc);
        CPPUNIT_ASSERT(syncResponse->failure());
        CPPUNIT_ASSERT(syncResponse->statusCode() == voltdb::STATUS_CODE_CONNECTION_LOST);
        CPPUNIT_ASSERT(syncResponse->results().size() == 0);

        boost::shared_ptr<InvocationResponse> response = cb->m_response;
        CPPUNIT_ASSERT(response->failure());
        CPPUNIT_ASSERT(response->statusCode() == voltdb::STATUS_CODE_CONNECTION_LOST);
        CPPUNIT_ASSERT(response->results().size() == 0);
        CPPUNIT_ASSERT(listener.reported);
        (*m_client)->runOnce();
    }

    void testLostConnectionBreaksEventLoop() {
        class Listener : public StatusListener {
        public:
            virtual bool uncaughtException(
                    std::exception exception,
                    boost::shared_ptr<voltdb::ProcedureCallback> callback) {
                CPPUNIT_ASSERT(false);
                return false;
            }
            virtual bool connectionLost(std::string hostname, int32_t connectionsLeft) {
                CPPUNIT_ASSERT(connectionsLeft == 1);
                return true;
            }
            virtual bool backpressure(bool hasBackpressure) {
                CPPUNIT_ASSERT(false);
                return false;
            }
        }  listener;
        (*m_dlistener)->m_listener = &listener;
        (*m_client)->createConnection("localhost", "hello", "world");
        (*m_client)->createConnection("localhost", "hello", "world");

        std::vector<Parameter> signature;
        Procedure proc("Insert", signature);
        proc.params();

        SyncCallback *cb = new SyncCallback();
        boost::shared_ptr<ProcedureCallback> callback(cb);
        (*m_client)->invoke(proc, callback);
        m_voltdb->hangupOnNextRequest();

        (*m_client)->run();
        (*m_client)->runOnce();
    }

    class BreakingSyncCallback : public ProcedureCallback {
    public:
        boost::shared_ptr<InvocationResponse> m_response;
        virtual bool callback(boost::shared_ptr<InvocationResponse> response) throw (voltdb::Exception) {
            m_response = response;
            return true;
        }
    };

    void testBreakEventLoopViaCallback() {
        (*m_client)->createConnection("localhost", "hello", "world");
        std::vector<Parameter> signature;
        signature.push_back(Parameter(WIRE_TYPE_STRING));
        signature.push_back(Parameter(WIRE_TYPE_STRING));
        signature.push_back(Parameter(WIRE_TYPE_STRING));
        Procedure proc("Insert", signature);
        ParameterSet *params = proc.params();
        params->addString("Hello").addString("World").addString("English");
        m_voltdb->filenameForNextMessage("invocation_response_success.msg");

        BreakingSyncCallback *cb = new BreakingSyncCallback();
        boost::shared_ptr<ProcedureCallback> callback(cb);
        (*m_client)->invoke(proc, callback);

        (*m_client)->run();
    }

    class ThrowingCallback : public ProcedureCallback {
    public:
        virtual bool callback(boost::shared_ptr<InvocationResponse> response) throw (voltdb::Exception) {
            throw voltdb::Exception();
        }
    };

    void testCallbackThrows() {
        class Listener : public StatusListener {
        public:
            bool reported;
           Listener() : reported(false) {}
            virtual bool uncaughtException(
                    std::exception exception,
                    boost::shared_ptr<voltdb::ProcedureCallback> callback) {
                reported = true;
                return true;
            }
            virtual bool connectionLost(std::string hostname, int32_t connectionsLeft) {
                CPPUNIT_ASSERT(false);
                return false;
            }
            virtual bool backpressure(bool hasBackpressure) {
                CPPUNIT_ASSERT(false);
                return false;
            }
        }  listener;
        (*m_dlistener)->m_listener = &listener;

        (*m_client)->createConnection("localhost", "hello", "world");
        std::vector<Parameter> signature;
        signature.push_back(Parameter(WIRE_TYPE_STRING));
        signature.push_back(Parameter(WIRE_TYPE_STRING));
        signature.push_back(Parameter(WIRE_TYPE_STRING));
        Procedure proc("Insert", signature);
        ParameterSet *params = proc.params();
        params->addString("Hello").addString("World").addString("English");
        m_voltdb->filenameForNextMessage("invocation_response_success.msg");

        ThrowingCallback *cb = new ThrowingCallback();
        boost::shared_ptr<ProcedureCallback> callback(cb);
        (*m_client)->invoke(proc, callback);

        (*m_client)->run();
    }

    void testBackpressure() {
        class Listener : public StatusListener {
        public:
            bool reported;
           Listener() : reported(false) {}
            virtual bool uncaughtException(
                    std::exception exception,
                    boost::shared_ptr<voltdb::ProcedureCallback> callback) {
                CPPUNIT_ASSERT(false);
                return true;
            }
            virtual bool connectionLost(std::string hostname, int32_t connectionsLeft) {
                CPPUNIT_ASSERT(false);
                return false;
            }
            virtual bool backpressure(bool hasBackpressure) {
                CPPUNIT_ASSERT(hasBackpressure);
                reported = true;
                return true;
            }
        }  listener;
        (*m_dlistener)->m_listener = &listener;

        (*m_client)->createConnection("localhost", "hello", "world");
        std::vector<Parameter> signature;
        Procedure proc("Insert", signature);
        SyncCallback *cb = new SyncCallback();
        boost::shared_ptr<ProcedureCallback> callback(cb);
        m_voltdb->dontRead();
        while (!listener.reported) {
            (*m_client)->invoke(proc, callback);
            (*m_client)->runOnce();
        }
//        (*m_client)->invoke(proc);
    }

private:
    boost::shared_ptr<Client> *m_client;
    MockVoltDB *m_voltdb;
    boost::shared_ptr<DelegatingListener> *m_dlistener;
};
CPPUNIT_TEST_SUITE_REGISTRATION( ClientTest );
}
