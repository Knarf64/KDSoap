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
//#include "wsdl_transformMedia-V1_0_7.h"
#include "source.h"
#include "httpserver_p.h"
#include <QtTest/QtTest>
#include <QEventLoop>
#include <QDebug>
#include <KDSoapClientInterface.h>
#include <KDSoapMessage.h>
#include <KDSoapServer.h>
#include <KDSoapNamespaceManager.h>

using namespace KDSoapUnitTestHelpers;

// Server side to perform job operation request
class TransformMediaBindingServerObject : public TransformMediaBindingServerBase //generated from wsdl
{
    Q_OBJECT
public:

    virtual TFMS__TransformResponseType transform( const TFMS__TransformRequestType& in ) {
        Q_UNUSED(in);
        // Answer SOAP 1.1 <faultcode>, <faultstring>, <faultfactor>, <detail>
        if (in.transformJob().operationName() == QLatin1String("specific")) {
            TFMS__TransformFaultType tft;
            tft.setCode(BMS__ErrorCodeType::DAT_S00_0001);
            tft.setDescription(QString("Invalid target media format.")); // spec example
            tft.setExtendedCode(TFMS__TransformErrorCodeType::SVC_S02_0001);
            BMS__InnerFaultType inner;
            inner.setCode(QString("innerCode"));
            inner.setDescription("innerDescription");
            inner.setDetail(QString("innerDetail"));
            QList< BMS__InnerFaultType > innerList; innerList << (inner);
            tft.setInnerFault(innerList);
            TransformFaultException tfe(QString("100"), QString("Application specific Fault, see Detail"), QString(), tft);
            throw tfe;
        }
        else {
            KDSoapFaultException kfe = KDSoapFaultException(QString("200"),QString("Soap Generic Fault"));
            throw kfe;
        }
        // TODO  : Answer SOAP 1.2  <Code> , <Reason> , <Node> , <Role> , <Detail>
        Q_ASSERT(false);
        TFMS__TransformResponseType ret;
        KDSoapMessage response;
        KDSoapValue _valueTransformAck(ret.serialize(QString::fromLatin1("transformAck")));
        _valueTransformAck.setNamespaceUri(QString::fromLatin1("http://transformmedia.fims.tv"));
        _valueTransformAck.setQualified(true);
        response = _valueTransformAck;
        ret.deserialize(response);
        return ret;
    }
};

class TransformMediaBindingServer : public KDSoapServer
{
    Q_OBJECT
public:
    TransformMediaBindingServer() : KDSoapServer(), m_lastServerObject(0) {
        setPath(QLatin1String("/xml"));
    }
    virtual QObject* createServerObject() { m_lastServerObject = new TransformMediaBindingServerObject; return m_lastServerObject; }
    TransformMediaBindingServerObject* lastServerObject() { if(!m_lastServerObject) createServerObject(); return m_lastServerObject; }
private:
    TransformMediaBindingServerObject* m_lastServerObject; // only for unittest purposes
};

// Need for data driven testing
Q_DECLARE_METATYPE(KDSoapFaultException)
Q_DECLARE_METATYPE(KDSoapFaultException*)
Q_DECLARE_METATYPE(TFMS__TransformFaultType)

class FaultsSupportTest : public QObject
{
    Q_OBJECT
private:
    QEventLoop m_eventLoop;

signals:
    void soapExceptionRequest(bool specific);


private Q_SLOTS:
    void initTestCase()
    {
        KDSoapUnitTestHelpers::initHashSeed();
    }

    void synchronousRaisingFaultExceptions_data() {
        QTest::addColumn<KDSoapFaultException*>("expectedException");
        QTest::addColumn<QString>("expectedElementName");
        QTest::addColumn<TFMS__TransformFaultType>("expectedFaultType");

        TFMS__TransformFaultType tft;
        tft.setCode(BMS__ErrorCodeType::DAT_S00_0001);
        tft.setDescription(QString("Invalid target media format.")); // spec example
        tft.setExtendedCode(TFMS__TransformErrorCodeType::SVC_S02_0001);
        BMS__InnerFaultType inner;
        inner.setCode(QString("innerCode"));
        inner.setDescription("innerDescription");
        inner.setDetail(QString("innerDetail"));
        QList< BMS__InnerFaultType > innerList; innerList << (inner);
        tft.setInnerFault(innerList);

        KDSoapFaultException* tfe = new TransformFaultException(QString("100"), QString("Application specific Fault, see Detail"), QString(), tft);
        KDSoapFaultException* kfe = new KDSoapFaultException(QString("200"), QString("Soap Generic Fault"));

        // add the Specific & the Generic Fault
        QTest::newRow("specificFault") << tfe << TransformFaultException::faultElementName() << tft;
        QTest::newRow("genericFault") << kfe << QString() << TFMS__TransformFaultType();
    }

    void synchronousRaisingFaultExceptions()
    {
        //qputenv( "KDSOAP_DEBUG", "1" );
        TestServerThread<TransformMediaBindingServer> serverThread;
        TransformMediaBindingServer* server = serverThread.startThread();
        TransformMediaService::TransformMediaBinding service;
        service.setEndPoint(server->endPoint());

        // data driven testing
        QFETCH(KDSoapFaultException*, expectedException);
        QFETCH(QString, expectedElementName);
        QFETCH(TFMS__TransformFaultType, expectedFaultType);

        TFMS__TransformJobType transformJob; // usefull to make the server throws the right exception
        QLatin1String type = ( expectedElementName == TransformFaultException::faultElementName() ) ? QLatin1String("specific") : QLatin1String("generic");
        transformJob.setOperationName(type);
        TFMS__TransformRequestType request;
        request.setTransformJob(transformJob);
        try {
            service.transform(request);
            QVERIFY(false);
        }
        catch (const TransformFaultException &ex) {
            //qDebug() << "Test caught a TransformFaultException";
            QCOMPARE( ex.faultCode(), expectedException->faultCode() );
            QCOMPARE( ex.faultString(), expectedException->faultString() );
            // specific part
            TFMS__TransformFaultType tft = ex.faultType();
            QCOMPARE( int(tft.code().type()), int(expectedFaultType.code().type()) );
            QCOMPARE( tft.description(), expectedFaultType.description() );
            QCOMPARE( int(tft.extendedCode().type()), int(expectedFaultType.extendedCode()) );
        }
        catch (const KDSoapFaultException &ex) {
            //qDebug() << "Test caught a KDSoapFaultException";
            QCOMPARE(ex.faultCode() , expectedException->faultCode());
            QCOMPARE(ex.faultString() , expectedException->faultString());
        }
        //TODO : don't let the ptr leak
    }

    void asynchronousRaisingFaultExceptions_data() {
       synchronousRaisingFaultExceptions_data();
    }

    void asynchronousRaisingFaultExceptions() {
        TestServerThread<TransformMediaBindingServer> serverThread;
        TransformMediaBindingServer* server = serverThread.startThread();
        TransformMediaService::TransformMediaBinding service;
        service.setEndPoint(server->endPoint());

        QFETCH(KDSoapFaultException*, expectedException);
        QFETCH(QString, expectedElementName);
        QFETCH(TFMS__TransformFaultType, expectedFaultType);

        TFMS__TransformJobType transformJob; // usefull to make the server throws the right exception
        QLatin1String type = ( expectedElementName == TransformFaultException::faultElementName() ) ? QLatin1String("specific") : QLatin1String("generic");
        transformJob.setOperationName(type);
        TFMS__TransformRequestType request;
        request.setTransformJob(transformJob);

        QEventLoop loop;
        TransformMediaService::TransformMediaBindingJobs::TransformJob transformAsyncJob(&service, this);
        connect(&transformAsyncJob, SIGNAL(finished(KDSoapJob*)), &loop , SLOT(quit()));
        transformAsyncJob.setIn(request);
        transformAsyncJob.start();
        loop.exec();

        try {
            transformAsyncJob.ack();
        }
        catch (const TransformFaultException &ex) {
            //qDebug() << "Test Async Call caught a TransformFaultException";
            QCOMPARE( ex.faultCode(), expectedException->faultCode() );
            QCOMPARE( ex.faultString(), expectedException->faultString() );
            // specific part
            TFMS__TransformFaultType tft = ex.faultType();
            QCOMPARE( int(tft.code().type()), int(expectedFaultType.code().type()) );
            QCOMPARE( tft.description(), expectedFaultType.description() );
            QCOMPARE( int(tft.extendedCode().type()), int(expectedFaultType.extendedCode()) );
            return;
        }
        catch (const KDSoapFaultException &ex) {
            //qDebug() << "Test async call caught a KDSoapFaultException";
            QCOMPARE(ex.faultCode() , expectedException->faultCode());
            QCOMPARE(ex.faultString() , expectedException->faultString());
            return;
        }
        Q_ASSERT(false);
    }
};

QTEST_MAIN(FaultsSupportTest)

#include "test_faults_support.moc"
