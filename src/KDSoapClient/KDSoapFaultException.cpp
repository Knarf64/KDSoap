#include "KDSoapFaultException.h"

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
    // application specific tag
    KDSoapValue m_detailValue;
};

KDSoapFaultException::Private::Private()
{
}

static const struct { const char* name; KDSoapFaultException::FaultCode value; } s_faultCodeEnumValues[6] =
{
    { "VersionMismatch", KDSoapFaultException::VersionMismatch          },
    { "MustUnderstand", KDSoapFaultException::MustUnderstand            },
    { "DataEncodingUnknown", KDSoapFaultException::DataEncodingUnknown  },
    { "Sender", KDSoapFaultException::Sender                            },
    { "Receiver", KDSoapFaultException::Receiver                        },
    { "UnknownFaultCode", KDSoapFaultException::UnknownFaultCode        }
};

static int s_numFaultCodes = sizeof(s_faultCodeEnumValues) / sizeof(*s_faultCodeEnumValues);

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

KDSoapFaultException::KDSoapFaultException(KDSoapFaultException::FaultCode code, const QString &reason, const QStringList &subcodes, const QString &node, const QString &role):
    d(new Private())
{
    Q_ASSERT(!reason.isNull());
    d->m_code = code;
    d->m_reason = reason;
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
    const KDSoapValueList& args = mainValue.childValues();
    for (int argNr = 0; argNr < args.count(); ++argNr) {
        const KDSoapValue& val = args.at(argNr);
        const QString name = val.name();
        if (name == QLatin1String("Code")) {
            storeCode(val);
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

void KDSoapFaultException::storeCode(const KDSoapValue &val) {

    const KDSoapValueList& codeChildren = val.childValues();
    for (int codeAttNr = 0; codeAttNr < codeChildren.count(); ++codeAttNr ) {
        const KDSoapValue& codeChild = codeChildren.at(codeAttNr);
        if (codeChild.name() == QLatin1String("Value")) {
            d->m_code = faultCodeStringToEnum(codeChild.value().toString());
        }
        else if ((codeChild.name() == QLatin1String("Subcode"))) {
            d->m_subcodes.clear();
            storeSubcodes(codeChild);
        }
    }
}

// recursive method helper to extract all subcodes
void KDSoapFaultException::storeSubcodes(const KDSoapValue &subcode)
{
    const KDSoapValueList& args = subcode.childValues();
    for (int argNr = 0; argNr < args.count(); ++argNr) {
        const KDSoapValue& val = args.at(argNr);
        const QString name = val.name();
        if (name == QLatin1String("Value")) {
            d->m_subcodes << val.value().value<QString>();
        }
        else if (name == QLatin1String("Subcode")) {
            storeSubcodes(val);
        }
    }
}

void KDSoapFaultException::deserialize( const KDSoapValue& mainValue )
{
    Q_ASSERT(mainValue.name() == QLatin1String("Fault"));
    const QString firstChildName = mainValue.childValues().first().name();
    if ( firstChildName == QLatin1String("faultcode") ){
        deserializeSoap1(mainValue);
    }
    else if ( firstChildName == QLatin1String("Code") ) {
        deserializeSoap2(mainValue);
    }
}

KDSoapFaultException::FaultCode KDSoapFaultException::faultCodeStringToEnum(const QString& str)
{
    for ( int i = 0; i < s_numFaultCodes; ++i ) {
        if (str == QLatin1String(s_faultCodeEnumValues[i].name)) {
            return s_faultCodeEnumValues[i].value;
        }
    }
    return UnknownFaultCode;
}

QString KDSoapFaultException::faultCodeEnumToString(KDSoapFaultException::FaultCode code)
{
    int rank = (int) code;
    return ( rank >= 0 && rank < s_numFaultCodes ) ? QLatin1String( s_faultCodeEnumValues[rank].name) : QString();
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
        if (name == QLatin1String("detail") || name == QLatin1String("Detail")) { // soap 1.1 and 1.2
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

void KDSoapFaultException::setCode(FaultCode code)
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
