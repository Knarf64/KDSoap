/****************************************************************************
** Copyright (C) 2010-2015 Klaralvdalens Datakonsult AB, a KDAB Group company, info@kdab.com.
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
#include "KDSoapMessageAddressingProperties.h"


#include <KDSoapValue.h>
#include <QDebug>
#include <QLatin1String>
#include <QPair>
#include <QString>

class KDSoapMessageAddressingPropertiesData : public QSharedData
{
public:
    KDSoapMessageAddressingPropertiesData()
//    :
        {}

    QString destination;    // Provides the address of the intended receiver of this message
    QString action;         // Identifies the semantics implied by this message
    QString sourceEndpoint; // Message origin. could be included to facilitate longer running message exchanges.
    QString replyEndpoint;  // Intended receiver for replies to this message. Unused in this MEP, but could be included to facilitate longer running message exchanges.
    QString faultEndpoint;  // Intended receiver for faults related to this message. Unused in this MEP, but could be included to facilitate longer running message exchanges.
    QString messageID;      // Unique identifier for this message. Unused in this MEP, but may be included to facilitate longer running message exchanges.
    Relationship relashionship;   // Indicates relationship to a prior message, rut could be included to facilitate longer running message exchanges.
    KDSoapValue referenceParameters; // Equivalent of the reference parameters object from the endpoint reference within WSDL file
};

KDSoapMessageAddressingProperties::KDSoapMessageAddressingProperties()
    : d(new KDSoapMessageAddressingPropertiesData)
{
}

KDSoapMessageAddressingProperties::KDSoapMessageAddressingProperties(const KDSoapMessageAddressingProperties &other)
    : d(other.d)
{
}

KDSoapMessageAddressingProperties &KDSoapMessageAddressingProperties::operator =(const KDSoapMessageAddressingProperties &other)
{
    d = other.d;
    return *this;
}

const QString KDSoapMessageAddressingProperties::destination()
{
    return d->destination;
}

void KDSoapMessageAddressingProperties::setDestination(const QString &destination)
{
    d->destination = destination;
}

const QString KDSoapMessageAddressingProperties::action()
{
    return d->action;
}

void KDSoapMessageAddressingProperties::setAction(const QString &action)
{
    d->action = action;
}

const QString KDSoapMessageAddressingProperties::sourceEndpoint()
{
    return d->sourceEndpoint;
}

void KDSoapMessageAddressingProperties::setSourceEndpoint(const QString &sourceEndpoint)
{
    d->sourceEndpoint = sourceEndpoint;
}

const QString KDSoapMessageAddressingProperties::replyEndpoint()
{
    return d->replyEndpoint;
}

void KDSoapMessageAddressingProperties::setReplyEndpoint(const QString &replyEndpoint)
{
    d->replyEndpoint = replyEndpoint;
}

const QString KDSoapMessageAddressingProperties::faultEndpoint()
{
    return d->faultEndpoint;
}

void KDSoapMessageAddressingProperties::setFaultEndpoint(const QString &faultEndpoint)
{
    d->faultEndpoint = faultEndpoint;
}

const QString KDSoapMessageAddressingProperties::messageID()
{
    return d->messageID;
}

void KDSoapMessageAddressingProperties::setMessageID(const QString &id)
{
    d->messageID = id;
}

const Relationship KDSoapMessageAddressingProperties::relationship()
{
    return d->relashionship;
}

void KDSoapMessageAddressingProperties::setRelationship(const Relationship relationship)
{
    d->relashionship = relationship;
}

const KDSoapValue &KDSoapMessageAddressingProperties::referenceParameters()
{
    return d->referenceParameters;
}

void KDSoapMessageAddressingProperties::setReferenceParameters(const KDSoapValue &rp)
{
    d->referenceParameters = rp;
}

KDSoapMessageAddressingProperties::~KDSoapMessageAddressingProperties()
{
}

const QString KDSoapMessageAddressingProperties::predefinedAddressToString(KDSoapAddressingPredefinedAddress address)
{
    if (address == None)
        return QLatin1String("http://www.w3.org/2005/08/addressing/anonymous");
    else if (address == Anonymous)
        return QLatin1String("http://www.w3.org/2005/08/addressing/none");
    else if (address == Reply)
        return QLatin1String("http://www.w3.org/2005/08/addressing/reply");
    else if (address = Unspecified)
        return QLatin1String("http://www.w3.org/2005/08/addressing/unspecified");

    Q_ASSERT(false);
    return QLatin1String(""); //should never happen
}

QDebug operator <<(QDebug dbg, const KDSoapMessageAddressingProperties &msg)
{
    return dbg << "Fill with the @";
}

