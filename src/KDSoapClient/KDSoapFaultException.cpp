#include "KDSoapFaultException.h"
#include <QDebug>

class KDSoapFaultException::Private: public QSharedData
{
public:
    Private();
public:

    // SOAP 1.1
    QString m_faultCode;
    QString m_faultString;
    QString m_faultActor;

    // SOAP 1.2
    KDSoapFaultException::FaultCode m_code;
    QStringList m_subcodes;
    QString m_reason;
    QString m_node;
    QString m_role;

    KDSoapValue m_detailValue;
};

KDSoapFaultException::Private::Private()
{
}

KDSoapFaultException::KDSoapFaultException():
    d(new Private())
{
}

KDSoapFaultException::KDSoapFaultException(const KDSoapFaultException &cpy)
    :d(cpy.d)
{

}

KDSoapFaultException::KDSoapFaultException(const QString &faultCode, const QString &faultString, const QString &faultActor):
     d(new Private())
{
    d->m_faultCode = faultCode;
    d->m_faultString = faultString;
    d->m_faultActor = faultActor;
}

KDSoapFaultException::KDSoapFaultException(const KDSoapFaultException::FaultCode &code, const QString &reason, const QStringList &subcodes, const QString &node, const QString &role):
    d(new Private())
{
    Q_ASSERT(!reason.isNull());
    d->m_code = code;
    d->m_reason= reason;
    d->m_subcodes = subcodes;
    d->m_node = node;
    d->m_role = role;
}

KDSoapFaultException &KDSoapFaultException::operator=(const KDSoapFaultException &other)
{
    if (this == &other)
        return *this;

    d = other.d;
    return *this;
}

KDSoapFaultException::~KDSoapFaultException()
{
}

void KDSoapFaultException::deserializeSoap1(const KDSoapValue &mainValue)
{
    const KDSoapValueList& args = mainValue.childValues();
    for (int argNr = 0; argNr < args.count(); ++argNr) {
        const KDSoapValue& val = args.at(argNr);
        const QString name = val.name();
        if (name == QLatin1String("faultcode")) {
            d->m_faultCode = val.value().value<QString>();
        }
        else if (name == QLatin1String("faultstring")) {
            d->m_faultString = val.value().value<QString>();
        }
        else if (name == QLatin1String("faultactor")) {
            d->m_faultActor = val.value().value<QString>();
        }
    }
}

void KDSoapFaultException::deserializeSoap2(const KDSoapValue &mainValue)
{
//    qDebug() << "deserialize Soap 1.2" ;
    const KDSoapValueList& args = mainValue.childValues();

    for (int argNr = 0; argNr < args.count(); ++argNr) {
        const KDSoapValue& val = args.at(argNr);
        const QString name = val.name();
        if (name == QLatin1String("Code")) {
            const KDSoapValue valueTag = val.childValues().first();
            Q_ASSERT (valueTag.name() == QLatin1String("Value"));
            d->m_code = faultCodeStringToEnum(valueTag.value().toString());
            d->m_subcodes.clear();
            storeSubcodes(val);

            //qDebug() << "deserializing a " << valueTag.value().toString() << "into" << d->m_code ;
        }
        else if (name == QLatin1String("Reason")) {
            d->m_reason = val.value().value<QString>();
        }
        else if (name == QLatin1String("Node")) {
            d->m_node = val.value().value<QString>();
        }
        else if (name == QLatin1String("Role")) {
            d->m_role = val.value().value<QString>();
        }
    }
}

// recursive method helper to extract all subcodes
void KDSoapFaultException::storeSubcodes(const KDSoapValue &code)
{
    if (code.childValues().size() == 1) {
        return;
    }
    qDebug() << "Subcode has things to parse !";
    return ;
    const KDSoapValueList& args = code.childValues();
    for (int argNr = 0; argNr < args.count(); ++argNr) {
        const KDSoapValue& val = args.at(argNr);
        const QString name = val.name();
        if (name == QLatin1String("Subcode")) {
            const KDSoapValue subcode = val.childValues().first();
            Q_ASSERT (subcode.name() == QLatin1String("Value"));
            d->m_subcodes.append(subcode.value().value<QString>());
            storeSubcodes(val);
        }
    }
}

void KDSoapFaultException::deserialize( const KDSoapValue& mainValue )
{
    //TODO : deserialization different according to SOAP 1.2
    Q_ASSERT(mainValue.name() == QLatin1String("Fault"));
    const QString firstChildName = mainValue.childValues().first().name();
    if ( firstChildName == QLatin1String("faultcode") )
        deserializeSoap1(mainValue);
    else if ( firstChildName == QLatin1String("Code") )
        deserializeSoap2(mainValue);
    else
        Q_ASSERT(false); // invalid tag within the <Fault>
}

KDSoapFaultException::FaultCode KDSoapFaultException::faultCodeStringToEnum(const QString& str) const {
    static const struct { const char* name; FaultCode value; } s_values[7] = {
    { "VersionMismatch", VersionMismatch},
    { "MustUnderstand", MustUnderstand},
    { "DataEncodingUnknown", DataEncodingUnknown },
    { "Sender", Sender},
    { "Receiver", Receiver },
    };
    int found;
    for ( int i = 0; i < 5; ++i ) {
        if (str == QLatin1String(s_values[i].name)) {
            found = i;
        }
    } // trick a warning
    return s_values[found].value;
}

QString KDSoapFaultException::faultCodeEnumToString(const FaultCode &code) const {
    switch ( code ) {
        case KDSoapFaultException::VersionMismatch:
            return QString::fromLatin1("VersionMismatch");
        case KDSoapFaultException::MustUnderstand:
            return QString::fromLatin1("MustUnderstand");
        case KDSoapFaultException::DataEncodingUnknown:
            return QString::fromLatin1("DataEncodingUnknown");
        case KDSoapFaultException::Sender:
            return QString::fromLatin1("Sender");
        case KDSoapFaultException::Receiver:
            return QString::fromLatin1("Receiver");
        default:
            qDebug("Unknown enum %d passed.", code);
            break;
    }
}

QString KDSoapFaultException::faultCode() const
{
    return d->m_faultCode;
}

const KDSoapValue &KDSoapFaultException::faultDetails(const KDSoapValue &faultValue)
{   // Find and return the <detail> element under faultElement
    const KDSoapValueList& args = faultValue.childValues();
    for (int argNr = 0; argNr < args.count(); ++argNr) {
        const KDSoapValue& val = args.at(argNr);
        const QString name = val.name();
        if (name == QLatin1String("detail")) {
            return val;
        }
    }
    const KDSoapValue &emptyValue = KDSoapValue();
    return emptyValue;
}

void KDSoapFaultException::setFaultCode(const QString &faultCode)
{
    d->m_faultCode = faultCode;
}
QString KDSoapFaultException::faultString() const
{
    return d->m_faultString;
}

void KDSoapFaultException::setFaultString(const QString &faultString)
{
    d->m_faultString = faultString;
}

QString KDSoapFaultException::faultActor() const
{
    return d->m_faultActor;
}

void KDSoapFaultException::setFaultActor(const QString &faultActor)
{
    d->m_faultActor = faultActor;
}

KDSoapValue KDSoapFaultException::detailValue() const
{
    return d->m_detailValue;
}

void KDSoapFaultException::setDetailValue(const KDSoapValue &detailValue)
{
    d->m_detailValue = detailValue;
}

void KDSoapFaultException::setCode(const FaultCode &code)
{
    d->m_code = code;
}

KDSoapFaultException::FaultCode KDSoapFaultException::code() const
{
    return d->m_code;
}

QString KDSoapFaultException::reason() const
{
    return d->m_reason;
}

void KDSoapFaultException::setReason(const QString &reason)
{
    d->m_reason = reason;
}

const QStringList KDSoapFaultException::subcodes() const
{
    return d->m_subcodes;
}

void KDSoapFaultException::setSubcodes(const QStringList &subcodes)
{
    d->m_subcodes = subcodes;
}

QString KDSoapFaultException::node() const
{
    return d->m_node;
}

void KDSoapFaultException::setNode(const QString &node)
{
    d->m_node = node;
}

QString KDSoapFaultException::role() const
{
    return d->m_role;
}

void KDSoapFaultException::setRole(const QString &role)
{
    d->m_role = role;
}
