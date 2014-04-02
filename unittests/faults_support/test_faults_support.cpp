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
#include "wsdl_transformMedia-V1_0_7.h"
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

        if (soapVersion() == SOAP1_1) { // Answer SOAP 1.1 <faultcode>, <faultstring>, <faultfactor>, <detail>
            prepareExceptionSoap1(in.transformJob().operationName());
        }
        else { // Answer SOAP 1.2  <Code>, <Reason>, <Node>, <Role>, <Detail>
            prepareExceptionSoap2(in.transformJob().operationName(), in.transformJob().statusDescription());
        }
        Q_UNUSED(in);
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

private:
    void prepareExceptionSoap1(const QString &exceptionKind)
    {
        if (true/*exceptionKind == QLatin1String("specific")*/)
        {
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
            TFMS__TransformFaultTypeException tfe(QString("100"), QString("Application specific Fault, see Detail"), QString(), tft);
            throw tfe;
        }
//        else {
//            KDSoapFaultException kfe = KDSoapFaultException(QString("200"), QString("Soap Generic Fault"));
//            throw kfe;
//        }
    }

    void prepareExceptionSoap2(QString exceptionKind, const QString &codeValue)
    {
        Q_UNUSED(codeValue)
        if (true/*exceptionKind == QLatin1String("specific")*/)
        {
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
            TFMS__TransformFaultTypeException tfe( KDSoapFaultException::MustUnderstand, QString("Application specific Fault, see Detail"), tft, QStringList() << "subcode1" << "subcode2", QString("nodestring"), QString("role") );
            throw tfe;
        }
//        else {
//            const QString reason("Application Generic Fault, see codeValue");
//            KDSoapFaultException kfe(KDSoapFaultException::faultCodeStringToEnum(codeValue), reason, QStringList(), QString("nodestring"), QString("role"));
//            throw kfe;
//        }
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

    void syncFault_data() {
        QTest::addColumn<KDSoapFaultException*>("expectedException");
        QTest::addColumn<QString>("exceptionType");
        QTest::addColumn<TFMS__TransformFaultType>("expectedFaultType");

        TFMS__TransformFaultType tft;
        tft.setCode(BMS__ErrorCodeType::DAT_S00_0001);
        tft.setDescription(QString("Invalid target media format."));
        tft.setExtendedCode(TFMS__TransformErrorCodeType::SVC_S02_0001);
        BMS__InnerFaultType inner;
        inner.setCode(QString("innerCode"));
        inner.setDescription("innerDescription");
        inner.setDetail(QString("innerDetail"));
        QList< BMS__InnerFaultType > innerList; innerList << (inner);
        tft.setInnerFault(innerList);

        KDSoapFaultException* tfe = new TFMS__TransformFaultTypeException(QString("100"), QString("Application specific Fault, see Detail"), QString(), tft);
        //KDSoapFaultException* kfe = new KDSoapFaultException(QString("200"), QString("Soap Generic Fault"));

        // add the Specific & the Generic Fault
        QTest::newRow("specificFault") << tfe << QString("specific") << tft;
        //QTest::newRow("genericFault") << kfe << QString("generic") << TFMS__TransformFaultType();
    }

    void syncFault()
    {
        //qputenv( "KDSOAP_DEBUG", "1" );
        TestServerThread<TransformMediaBindingServer> serverThread;
        TransformMediaBindingServer* server = serverThread.startThread();
        TransformMediaService::TransformMediaBinding service;
        service.setEndPoint(server->endPoint());

        // data driven testing
        QFETCH(KDSoapFaultException*, expectedException);
        QFETCH(QString, exceptionType);
        QFETCH(TFMS__TransformFaultType, expectedFaultType);

        TFMS__TransformJobType transformJob;
        transformJob.setOperationName(exceptionType); // used to make the server throws the right type of exception
        TFMS__TransformRequestType request;
        request.setTransformJob(transformJob);
        try {
            service.transform(request);
            QVERIFY(false);
        }
        catch (const TFMS__TransformFaultTypeException &ex) {
            //qDebug() << "Test caught a TFMS__TransformFaultTypeException";
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
            QCOMPARE(ex.faultCode(), expectedException->faultCode());
            QCOMPARE(ex.faultString(), expectedException->faultString());
        }
        //TODO : don't let the ptr leak
    }

    void asyncFault_data() {
       syncFault_data();
    }

    void asyncFault() {
//        qputenv( "KDSOAP_DEBUG", "1" );

        TestServerThread<TransformMediaBindingServer> serverThread;
        TransformMediaBindingServer* server = serverThread.startThread();
        TransformMediaService::TransformMediaBinding service;
        service.setEndPoint(server->endPoint());

        QFETCH(KDSoapFaultException*, expectedException);
        QFETCH(QString, exceptionType);
        QFETCH(TFMS__TransformFaultType, expectedFaultType);

        TFMS__TransformJobType transformJob;
        transformJob.setOperationName(exceptionType); // used to make the server throw the right type of exception

        TFMS__TransformRequestType request;
        request.setTransformJob(transformJob);

        QEventLoop loop;
        TransformMediaService::TransformMediaBindingJobs::TransformJob transformAsyncJob(&service, this);
        connect(&transformAsyncJob, SIGNAL(finished(KDSoapJob*)), &loop, SLOT(quit()));
        transformAsyncJob.setIn(request);
        transformAsyncJob.start();
        loop.exec();

        try {
            TFMS__TransformResponseType  resp = transformAsyncJob.ack();
            qDebug() << "No exception raised !";
        }
        catch (const TFMS__TransformFaultTypeException &ex) {
            //qDebug() << "Test Async Call caught a TFMS__TransformFaultTypeException";
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
            QCOMPARE(ex.faultCode(), expectedException->faultCode());
            QCOMPARE(ex.faultString(), expectedException->faultString());
            return;
        }
        Q_ASSERT(false);
    }

    void syncFaultSOAP2_data() {

        QTest::addColumn<KDSoapFaultException*>("expectedException");
        QTest::addColumn<QString>("exceptionType");
        QTest::addColumn<TFMS__TransformFaultType>("expectedFaultType");
        QTest::addColumn<QString>("faultCode");

        /* Let All this for when a raising exception flag will exist and will have to be tested

        // Loop adding generic fault exception with all possible <code> <value>
        for (int rank = KDSoapFaultException::VersionMismatch ; rank <= KDSoapFaultException::Receiver; ++rank) {
            //qDebug() << " rank in the loop" << rank;
            KDSoapFaultException* kfe = new KDSoapFaultException( (KDSoapFaultException::FaultCode) rank,
                                                                   QString("Application Generic Fault, see codeValue"), QStringList(),
                                                                   QString("nodestring"), QString("role"));

             // add the fault exception to the data
             QTest::newRow("genericFault") << kfe
                                           << QString("generic")
                                           << TFMS__TransformFaultType()
                                           << KDSoapFaultException::faultCodeEnumToString( (KDSoapFaultException::FaultCode) rank );

        }
        */
        // adding a specific exception with <detail> tag filled up !
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

        KDSoapFaultException* tfe = new TFMS__TransformFaultTypeException(KDSoapFaultException::MustUnderstand,
                                                                QString("Application specific Fault, see Detail"), tft,
                                                                QStringList() << "subcode1" << "subcode2",
                                                                QString("nodestring"), QString("role") );

        QTest::newRow("specificFault") << tfe << QString("specific")
                                       << tft << KDSoapFaultException::faultCodeEnumToString( KDSoapFaultException::MustUnderstand );

    }

    void syncFaultSOAP2()
    {
//        qputenv( "KDSOAP_DEBUG", "1" );
        TestServerThread<TransformMediaBindingServer> serverThread;
        TransformMediaBindingServer* server = serverThread.startThread();
        TransformMediaService::TransformMediaBinding service;
        service.setEndPoint(server->endPoint());
        service.setSoapVersion(KDSoapClientInterface::SOAP1_2);

        // data driven testing
        QFETCH(KDSoapFaultException*, expectedException);
        QFETCH(QString, exceptionType);
        QFETCH(TFMS__TransformFaultType, expectedFaultType);
        QFETCH(QString, faultCode);

        TFMS__TransformJobType transformJob;
        transformJob.setOperationName(exceptionType); // used to make the server throw the right type of exception
        transformJob.setStatusDescription( faultCode ); // used to get the right code value within the exception

        TFMS__TransformRequestType request;
        request.setTransformJob(transformJob);
        try {
            service.transform(request);
            QVERIFY(false);
        }
        catch (const TFMS__TransformFaultTypeException &ex) {
//            qDebug() << "Test fault SOAP 2 caught a TFMS__TransformFaultTypeException";
            QCOMPARE( ex.code(), expectedException->code() );
            QCOMPARE( ex.reason(), expectedException->reason());
            QCOMPARE( ex.subcodes(), expectedException->subcodes());
            QCOMPARE(ex.node(), expectedException->node());
            QCOMPARE(ex.role(), expectedException->role());
            // specific part
            TFMS__TransformFaultType tft = ex.faultType();
            QCOMPARE( int(tft.code().type()), int(expectedFaultType.code().type()) );
            QCOMPARE( tft.description(), expectedFaultType.description() );
            QCOMPARE( int(tft.extendedCode().type()), int(expectedFaultType.extendedCode()) );
        }
        catch (const KDSoapFaultException &ex) {
//            qDebug() << "Test fault SOAP 2 caught a KDSoapFaultException";
            QCOMPARE(ex.code(), expectedException->code());
            QCOMPARE(ex.reason(), expectedException->reason());
            QCOMPARE(ex.node(), expectedException->node());
            QCOMPARE(ex.role(), expectedException->role());
        }
        //TODO : don't let the ptr leak
    }

};

QTEST_MAIN(FaultsSupportTest)

#include "test_faults_support.moc"
