#include "endpointreference.h"

#include <QDomElement>
#include <common/messagehandler.h>
#include <common/nsmanager.h>
#include <common/qname.h>

#include <QDebug>

using namespace KWSDL;

EndpointReference::EndpointReference()
{
}

EndpointReference::EndpointReference(const QString &nameSpace):
    Element( nameSpace )
{

}

EndpointReference::~EndpointReference()
{
}

void EndpointReference::loadXML( ParserContext *context, const QDomElement &element )
{
    qDebug() << "EndpointReference::loadXML !";
    QDomElement child = element.firstChildElement();

    while ( !child.isNull() ) {
      NSManager namespaceManager( context, child );
      const QName tagName( child.tagName() );
      if ( tagName.localName() == QLatin1String("Address") ) {
          mAddress = child.text();
      } else if ( tagName.localName() == QLatin1String("ReferenceParameters")) {
            // how to handle this data ? any object of any namespace...
      } else if ( tagName.localName() == QLatin1String("Metadata")) {
            // how to handle this data ? any object of any namespace...
      } else {
        context->messageHandler()->warning( QString::fromLatin1("EndPointReference: unknown tag %1" ).arg( child.tagName() ) );
      }

      child = child.nextSiblingElement();
    }
    if (mAddress.isEmpty())
        context->messageHandler()->warning( QLatin1String("EndPointReference: 'Address' required") );
}

QString EndpointReference::address() const
{
    return mAddress;
}
