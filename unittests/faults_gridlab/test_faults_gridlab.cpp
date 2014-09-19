/****************************************************************************
** Copyright (C) 2010-2013 Klaralvdalens Datakonsult AB.  All rights reserved.
**
** This file is part of the KD Soap library.
**
** Licensees holding valid commercial KD Soap licenses may use this file in
** accordance with the KD Soap Commercial License Agreement provided with
** the Software.
**
**
** This file may be distributed and/or modified under the terms of the
** GNU Lesser General Public License version 2 and version 3 as published by the
** Free Software Foundation and appearing in the file LICENSE.LGPL.txt included.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** Contact info@kdab.com if any conditions of this licensing are not
** clear to you.
**
**********************************************************************/
#include "wsdl_gridlab.h"
#include "httpserver_p.h"
#include <QtTest/QtTest>
#include <QEventLoop>
#include <QDebug>
#include <KDSoapClientInterface.h>
#include <KDSoapMessage.h>
#include <KDSoapServer.h>
#include <KDSoapNamespaceManager.h>

using namespace KDSoapUnitTestHelpers;

// here I set up the the wsdl of a website :
// http://www.gridlab.org/WorkPackages/wp-5/guide/faults.html
// proove of interoperability of our SOAP implentation

// server part
class MyServiceServerBaseObject : public MyServiceServerBase  {

    // this method will return an exception each time the input isn't containing the error string
    virtual NS1__MyOperationResponse myOperation( const NS1__MyOperation& parameters ) {
        if (parameters.myInput().contains("error")) {
            qDebug() << "error found, throwing exception" ;
            NS1__MyFirstException myExceptionContent;
            myExceptionContent.setText("fail : your input was containing the error word");
            MyFirstExceptionException exception;
            exception.setFaultCode("business failed, check detail entry to get the reason");
            exception.setFaultString("business failed, check detail entry to get the reason");
            exception.setFault(myExceptionContent);
            throw exception;
        }
        else {
            NS1__MyOperationResponse resp;
            resp.setMyOutput("success : no error word found withind the input");
            return resp;
        }
    }
};

class MyServiceServer : public KDSoapServer
{
    Q_OBJECT
public:
    MyServiceServer() : KDSoapServer(), m_lastServerObject(0) {
        setPath(QLatin1String("/xml"));
    }
    virtual QObject* createServerObject() { m_lastServerObject = new MyServiceServerBaseObject; return m_lastServerObject; }
    MyServiceServerBaseObject* lastServerObject() { if(!m_lastServerObject) createServerObject(); return m_lastServerObject; }
private:
    MyServiceServerBaseObject* m_lastServerObject; // only for unittest purposes
};


// client part
class FaultsGridlabTest : public QObject
{
    Q_OBJECT

public:
    FaultsGridlabTest() {}

private Q_SLOTS:

    void testGridlabFault_data() {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("output");

        QTest::newRow("sucess") << QString("this shouldnt trigger the exception") << QString("success : no error word found withind the input");
        QTest::newRow("fail") << QString("this should trigger the exception : error") << QString("fail : your input was containing the error word");
    }

    void testGridlabFault()
    {
        TestServerThread<MyServiceServer> serverThread;
        MyServiceServer* server = serverThread.startThread();
        MyService service;
        service.setEndPoint(server->endPoint());

        QFETCH(QString, input);
        QFETCH(QString, output);

        NS1__MyOperation operation;
        operation.setMyInput(input);

        try {
           NS1__MyOperationResponse resp = service.myOperation(operation);
           if (input.contains("error")) {
               QVERIFY(false);
           }

           else
               QCOMPARE(resp.myOutput(), output);
        }
        catch(const MyFirstExceptionException& ex)
        {
            QCOMPARE(ex.fault().text(), output);
        }
    }
};


QTEST_MAIN(FaultsGridlabTest)

#include "test_faults_gridlab.moc"

 
