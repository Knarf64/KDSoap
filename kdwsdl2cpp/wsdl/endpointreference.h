// Define the copyright

#ifndef ENDPOINTREFERENCE_H
#define ENDPOINTREFERENCE_H

#include <QDomElement>

#include <common/parsercontext.h>
#include <wsdl/element.h>

#include <kode_export.h>

// Class following WS-Addressing specification

namespace KWSDL {

class KWSDL_EXPORT EndpointReference : public Element
{
  public:
    typedef QList<EndpointReference> List;

    EndpointReference();
    EndpointReference( const QString &nameSpace );
    ~EndpointReference();

    void loadXML( ParserContext *context, const QDomElement &element );

    QString address() const;

  private:
    QString mAddress; // mandatory //see  predefined addresses : anonymous an none"
    // reference properties : *
    // reference parameters : *
    // metadata : *
    // qname of the port type : 0..1
    // policy : 0*

};

}

#endif // ENDPOINTREFERENCE_H

