/****************************************************************************
** Copyright (C) 2010-2014 Klaralvdalens Datakonsult AB, a KDAB Group company, info@kdab.com.
** All rights reserved.
**
** This file is part of the KD Soap library.
**
** Licensees holding valid commercial KD Soap licenses may use this file in
** accordance with the KD Soap Commercial License Agreement provided with
** the Software.
**
**
** This file may be distributed and/or modified under the terms of the
** GNU Lesser General Public License version 2.1 and version 3 as published by the
** Free Software Foundation and appearing in the file LICENSE.LGPL.txt included.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** Contact info@kdab.com if any conditions of this licensing are not
** clear to you.
**
**********************************************************************/

#include "httpserver_p.h"

#include <QtTest/QtTest>
#include <QDebug>
#include <QObject>

#include "wsdl_wsaddressing.h"

class WSAddressingTest : public QObject
{
    Q_OBJECT

public:
    WSAddressingTest(){}
    virtual ~WSAddressingTest() {}

private Q_SLOTS:
    void shouldRecognizeWSAddressingFeatureFromWSDL()
    {

        HttpServerThread server(QByteArray(), HttpServerThread::Public);
        Hello_Service service;
        service.setEndPoint(server.endPoint());
        qputenv("KDSOAP_DEBUG" , "1");
        service.sayHello("Hi !");

        // check the SOAP HEADER received from the server, bitch !

//        <wsa:To>xs:anyURI</wsa:To> ?
//        <wsa:From>wsa:EndpointReferenceType</wsa:From> ?
//        <wsa:ReplyTo>wsa:EndpointReferenceType</wsa:ReplyTo> ?
//        <wsa:FaultTo>wsa:EndpointReferenceType</wsa:FaultTo> ?
//        <wsa:Action>xs:anyURI</wsa:Action>
//        <wsa:MessageID>xs:anyURI</wsa:MessageID> ?
//        <wsa:RelatesTo RelationshipType="xs:anyURI"?>xs:anyURI</wsa:RelatesTo> *
//        <wsa:ReferenceParameters>xs:any*</wsa:ReferenceParameters> ?

        // NEED TO add implicit / explicit recognition
    }
};

QTEST_MAIN(WSAddressingTest)

#include "wsaddressingtest.moc"
