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

/*
 *
 * This test is making sure the fault support is working.
 * Fault in the soap protocol is an answer to a failing request
 * serveral reason can make a fault happen : generic one such as SSL or Http issue,
 * and specific one (related to the business) wrong semantic carried in the request.
 *
 * SOAP define two versions of fault : 1.1 and 1.2, where the structure change a bit from
 * one to the other but ideas and concepts remains.
 *
 * In both version the generic fault filled up some part of the fault structure such as
 * faultcode , faultstring ( or description ) etc...
 * and the specific part of the fault fills the <detail> tag
 *
 * A fault is declared in the wsdl file and can take several aspects :
 *  - complexType
 *  - built-in type declare inside a simpleType
 *  - bult-in type declare directly in the part of the fault message
 *  - mix of everything
 *
 * This test is trying to be exhaustive by testing soap 1.1 and 1.2 fault support,
 * and all possible combinaison of fault structure.
 *
 * The WSDL used is a custom transformMedia WSDL file, there is a service carrying SOAP1.1 and
 * a copy of it using SOAP1.2 which make it easier to differenciate.
 *
 */

using namespace KDSoapUnitTestHelpers;

static const char s_builtin[] = "builtin";
static const char s_complex[] = "complex";
static const char s_mixedType[] = "mixedtype";

/***************************** SERVER SIDE SOAP 1.1 ****************************************/
class TransformMediaBindingServerObject : public TransformMediaBindingServerBase //generated from wsdl
{
    Q_OBJECT
public:

    virtual TFMS__TransformResponseType transform( const TFMS__TransformRequestType& in ) {

        if (soapVersion() == SOAP1_1) { // Answer SOAP 1.1 <faultcode>, <faultstring>, <faultfactor>, <detail>
            prepareExceptionSoap1(in.transformJob().operationName());
        } // no way to get a soap 1.2 request

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
    void prepareExceptionSoap1(const QString& exceptionKind)
    {
        Q_UNUSED(exceptionKind);
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
        TransformFaultException tfe(QString("100"), QString("Application specific Fault, see Detail"), QString());
        tfe.setFault(tft);
        throw tfe;
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

/***************************** SERVER SIDE SOAP 1.2 ****************************************/
class TransformMediaBinding12ServerObject : public TransformMediaService12ServerBase  //generated from wsdl
{
    Q_OBJECT
public:

    virtual TFMS__TransformResponseType transformSoap12( const TFMS__TransformRequestType& in ) {
        prepareExceptionSoap2(in.transformJob().operationName());
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
    void prepareExceptionSoap2(QString exceptionKind)
    {
        if (exceptionKind == s_builtin) {
            TransformFaultBuiltInException tfe(KDSoapFaultException::MustUnderstand, QString("Application specific builtin Fault, see Detail"),
                                               QStringList() << "subcode1" << "subcode2", QString("nodestring"), QString("role") );
            tfe.setSimpleTypeElementName("BuiltIn type part of the fault message");
            throw tfe;
        }
        if (exceptionKind == s_complex) {
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
            TransformFaultException tfe( KDSoapFaultException::MustUnderstand, QString("Application specific Fault, see Detail"),
                                         QStringList() << "subcode1" << "subcode2", QString("nodestring"), QString("role") );
            tfe.setFault(tft);
            throw tfe;
        }
        if (exceptionKind == s_mixedType) {
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

            TransformFaultMixedTypeException tfe (  KDSoapFaultException::MustUnderstand,
                                                    QString("Application specific builtin Fault, see Detail"),
                                                    QStringList() << "subcode1" << "subcode2",
                                                    QString("nodestring"), QString("role") );

            tfe.setSimpleTypeElementName("Mixed type part of the fault message");// built-in part
            tfe.setFaultTest(tft); // complexe type part
            throw tfe; // mixed exception then
        }
    }
};

class TransformMediaBinding12Server : public KDSoapServer {
    Q_OBJECT
public:
    TransformMediaBinding12Server() : KDSoapServer(), m_lastServerObject(0) {
        setPath(QLatin1String("/xml"));
    }
    virtual QObject* createServerObject() { m_lastServerObject = new TransformMediaBinding12ServerObject; return m_lastServerObject; }
    TransformMediaBinding12ServerObject* lastServerObject() { if(!m_lastServerObject) createServerObject(); return m_lastServerObject; }
private:
    TransformMediaBinding12ServerObject* m_lastServerObject; // only for unittest purposes
};

// Needed for data driven testing
Q_DECLARE_METATYPE(TFMS__TransformFaultType)
Q_DECLARE_METATYPE(TransformFaultException)
Q_DECLARE_METATYPE(TransformFaultException*)
Q_DECLARE_METATYPE(TransformFaultBuiltInException*)
Q_DECLARE_METATYPE(TransformFaultMixedTypeException*)

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
        //KDSoapUnitTestHelpers::initHashSeed();
    }

    void syncFault_data() {
        QTest::addColumn<TransformFaultException*>("expectedException");
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

        TransformFaultException* tfe = new TransformFaultException(QString("100"), QString("Application specific Fault, see Detail"), QString());
        tfe->setFault(tft);

        // add the Specific & the Generic Fault
        QTest::newRow("specificFault") << tfe << QString("specific") << tft;
    }

    void syncFault()
    {
        //qputenv( "KDSOAP_DEBUG", "1" );
        TestServerThread<TransformMediaBindingServer> serverThread;
        TransformMediaBindingServer* server = serverThread.startThread();
        TransformMediaService::TransformMediaBinding service;
        service.setEndPoint(server->endPoint());

        // data driven testing
        QFETCH(TransformFaultException*, expectedException);
        QFETCH(QString, exceptionType);
        QFETCH(TFMS__TransformFaultType, expectedFaultType);

        TFMS__TransformJobType transformJob;
        transformJob.setOperationName(exceptionType); // used to make the server throws the right type of exception
        TFMS__TransformRequestType request;
        request.setTransformJob(transformJob);
        try {
            service.transform(request);
            QVERIFY(false); // shouldn't happen
        }
        catch (const TransformFaultException &ex) {
            //qDebug() << "Test caught a transformFaultException";
            QCOMPARE( ex.faultCode(), expectedException->faultCode() );
            QCOMPARE( ex.faultString(), expectedException->faultString() );
            // specific part
            TFMS__TransformFaultType tft = ex.fault();
            QCOMPARE( int(tft.code().type()), int(expectedFaultType.code().type()) );
            QCOMPARE( tft.description(), expectedFaultType.description() );
            QCOMPARE( int(tft.extendedCode().type()), int(expectedFaultType.extendedCode()) );
        }
    }

    void asyncFault_data() {
       syncFault_data();
    }
    void asyncFault()
    {
        //qputenv( "KDSOAP_DEBUG", "1" );
        TestServerThread<TransformMediaBindingServer> serverThread;
        TransformMediaBindingServer* server = serverThread.startThread();
        TransformMediaService::TransformMediaBinding service;
        service.setEndPoint(server->endPoint());

        QFETCH(TransformFaultException*, expectedException);
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
            QVERIFY(false); // shouldn't happen
        }
        catch (const TransformFaultException &ex) {
            //qDebug() << "Test Async Call caught a transformFaultException";
            QCOMPARE( ex.faultCode(), expectedException->faultCode() );
            QCOMPARE( ex.faultString(), expectedException->faultString() );
            // specific part
            TFMS__TransformFaultType tft = ex.fault();
            QCOMPARE( int(tft.code().type()), int(expectedFaultType.code().type()) );
            QCOMPARE( tft.description(), expectedFaultType.description() );
            QCOMPARE( int(tft.extendedCode().type()), int(expectedFaultType.extendedCode()) );
        }
        delete expectedException; //don't let the ptr leak
    }

    void syncFaultSOAP2_data() {
        QTest::addColumn<TransformFaultException*>("expectedException");
        QTest::addColumn<QString>("exceptionType");
        QTest::addColumn<TFMS__TransformFaultType>("expectedFaultType");
        QTest::addColumn<QString>("faultCode");

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

        TransformFaultException* tfe = new TransformFaultException(KDSoapFaultException::MustUnderstand,
                                                                QString("Application specific Fault, see Detail"), /*tft,*/
                                                                QStringList() << "subcode1" << "subcode2",
                                                                QString("nodestring"), QString("role") );
        tfe->setFault(tft);

        QTest::newRow("specificFault") << tfe << QString(s_complex)
                                       << tft << KDSoapFaultException::faultCodeEnumToString( KDSoapFaultException::MustUnderstand );
    }

    void syncFaultSOAP2()
    {
        //qputenv( "KDSOAP_DEBUG", "1" );
        TestServerThread<TransformMediaBinding12Server> serverThread;
        TransformMediaBinding12Server* server = serverThread.startThread();
        TransformMediaService12 service;
        service.setEndPoint(server->endPoint());
        service.setSoapVersion(KDSoapClientInterface::SOAP1_2); // useless but expressive

        // data driven testing
        QFETCH(TransformFaultException*, expectedException);
        QFETCH(QString, exceptionType);
        QFETCH(TFMS__TransformFaultType, expectedFaultType);
        QFETCH(QString, faultCode); // useless at the moment

        TFMS__TransformJobType transformJob;
        transformJob.setOperationName(exceptionType); // used to make the server throw the right type of exception
        transformJob.setStatusDescription( faultCode ); // will be used to get the right code value within the exception

        TFMS__TransformRequestType request;
        request.setTransformJob(transformJob);
        try {
            service.transformSoap12(request);
            QVERIFY(false);
        }
        catch (const TransformFaultException &ex) {
//            qDebug() << "Test fault SOAP 2 caught a transformFaultException";
            QCOMPARE( ex.code(), expectedException->code() );
            QCOMPARE( ex.reason(), expectedException->reason());
            QCOMPARE( ex.subcodes(), expectedException->subcodes());
            QCOMPARE( ex.node(), expectedException->node());
            QCOMPARE( ex.role(), expectedException->role());
            // specific part
            TFMS__TransformFaultType tft = ex.fault();
            QCOMPARE( int(tft.code().type()), int(expectedFaultType.code().type()) );
            QCOMPARE( tft.description(), expectedFaultType.description() );
            QCOMPARE( int(tft.extendedCode().type()), int(expectedFaultType.extendedCode()) );
        }
        delete expectedException; //don't let the ptr leak
    }

    void syncFaultBuiltInSOAP2_data()
    {
        QTest::addColumn<TransformFaultBuiltInException*>("expectedException");
        QTest::addColumn<QString>("exceptionType");

        TransformFaultBuiltInException* tfe = new TransformFaultBuiltInException(KDSoapFaultException::MustUnderstand,
                                                                QString("Application specific builtin Fault, see Detail"),
                                                                QStringList() << "subcode1" << "subcode2",
                                                                QString("nodestring"), QString("role") );
        tfe->setSimpleTypeElementName("BuiltIn type part of the fault message");
        QTest::newRow("specificBuiltInTypeFault") << tfe << QString(s_builtin);
    }

    void syncFaultBuiltInSOAP2() // testing built-in fault
    {
        //qputenv( "KDSOAP_DEBUG", "1" );
        TestServerThread<TransformMediaBinding12Server> serverThread;
        TransformMediaBinding12Server* server = serverThread.startThread();
        TransformMediaService12 service;
        service.setEndPoint(server->endPoint());
        service.setSoapVersion(KDSoapClientInterface::SOAP1_2); // useless but expressive

        // data driven testing
        QFETCH(TransformFaultBuiltInException*, expectedException);
        QFETCH(QString, exceptionType);

        TFMS__TransformJobType transformJob;
        transformJob.setOperationName(exceptionType); // used to make the server throw the right type of exception

        TFMS__TransformRequestType request;
        request.setTransformJob(transformJob);
        try {
            service.transformSoap12(request);
            QVERIFY(false);
        }
        catch (const TransformFaultBuiltInException &ex) {
            Q_ASSERT(true);
//            qDebug() << "Test fault SOAP 2 caught a transformFaultBuiltInException";
            QCOMPARE( ex.code(), expectedException->code() );
            QCOMPARE( ex.reason(), expectedException->reason());
            QCOMPARE( ex.subcodes(), expectedException->subcodes());
            QCOMPARE( ex.node(), expectedException->node());
            QCOMPARE( ex.role(), expectedException->role());
            // specific builtin part
            QCOMPARE(ex.simpleTypeElementName(), expectedException->simpleTypeElementName());
        }
        delete expectedException; //don't let the ptr leak
    }

    void syncFaultMixedTypeSOAP2_data()
    {
        QTest::addColumn<TransformFaultMixedTypeException*>("expectedException");
        QTest::addColumn<QString>("exceptionType");
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

        TransformFaultMixedTypeException* tfe = new TransformFaultMixedTypeException(KDSoapFaultException::MustUnderstand,
                                                                QString("Application specific builtin Fault, see Detail"),
                                                                QStringList() << "subcode1" << "subcode2",
                                                                QString("nodestring"), QString("role") );

        // setSimpleTypeElement or setSimpleTypeElementName ? part.type() vs part.name()
        tfe->setSimpleTypeElementName("Mixed type part of the fault message");
        tfe->setFaultTest(tft); // name should be different too ?
        QTest::newRow("specificMixedTypeFault") << tfe << QString(s_mixedType);
    }

    void syncFaultMixedTypeSOAP2() // testing mixing type fault (builtIn and ComplexType together
    {
        //qputenv( "KDSOAP_DEBUG", "1" );
        TestServerThread<TransformMediaBinding12Server> serverThread;
        TransformMediaBinding12Server* server = serverThread.startThread();
        TransformMediaService12 service;
        service.setEndPoint(server->endPoint());
        service.setSoapVersion(KDSoapClientInterface::SOAP1_2); // useless but expressive

        // data driven testing
        QFETCH(TransformFaultMixedTypeException*, expectedException);
        QFETCH(QString, exceptionType);

        TFMS__TransformJobType transformJob;
        transformJob.setOperationName(exceptionType); // used to make the server throw the right type of exception

        TFMS__TransformRequestType request;
        request.setTransformJob(transformJob);

        try {
            service.transformSoap12(request);
            QVERIFY(false);
        }
        catch (const TransformFaultMixedTypeException &ex) {
            Q_ASSERT(true);
//            qDebug() << "Test fault SOAP 2 caught a transformFaultBuiltInException";
            QCOMPARE( ex.code(), expectedException->code() );
            QCOMPARE( ex.reason(), expectedException->reason());
            QCOMPARE( ex.subcodes(), expectedException->subcodes());
            QCOMPARE( ex.node(), expectedException->node());
            QCOMPARE( ex.role(), expectedException->role());

            // specific builtin part
            QCOMPARE(ex.simpleTypeElementName(), expectedException->simpleTypeElementName());
            TFMS__TransformFaultType received = ex.faultTest() ; // should let this name ? or use the element type name ?
            TFMS__TransformFaultType expected = expectedException->faultTest();
            QCOMPARE(received.code(), expected.code());
            QCOMPARE(received.description(), expected.description());
            QCOMPARE(received.extendedCode(), expected.extendedCode());
            QCOMPARE(received.innerFault().first().code(), expected.innerFault().first().code());
        }
        // we could caught the wrong exception, some debug to help with it
        catch(const TransformFaultBuiltInException& fail) {
            qDebug() << "Mixed type exception failed to be caught, ending up catching builtInException instead";
            QVERIFY(false);
        }
        catch(const TransformFaultException& fail2) {
            qDebug() << "Mixed type exception failed to be caught, ending up catching transformFaultException instead";
            QVERIFY(false);
        }
        catch (const KDSoapFaultException &fail3) {
            qDebug() << "Mixed type exception failed to be caught, ending up catching KDSoapFaultException instead";
            QVERIFY(false);
        }
        delete expectedException; // don't let the ptr leak
    }
};

QTEST_MAIN(FaultsSupportTest)

#include "test_faults_support.moc"
