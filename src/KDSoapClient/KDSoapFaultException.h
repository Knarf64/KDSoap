#ifndef KDSOAPFAULTEXCEPTION_H
#define KDSOAPFAULTEXCEPTION_H

#include <QString>
#include <QStringList>
#include <KDSoapValue.h>

/**
 * KDSoapFaultException is the base class for exceptions representing a fault element.
 * So far supporting only SOAP 1.1 version which mean the following attributes :
 * <faultcode>, <faultstring>, <faultfactor>, <detail>
 *
 * The <detail> tag optionally contains specific serialized fault information.
 * represented as an exception in the client side.
 */
class KDSOAP_EXPORT KDSoapFaultException
{
public:
    enum FaultCode {

    /** The faulting node found an invalid element information item instead of the expected Envelope
    *  element information item. The namespace, local name or both did not match the Envelope element
    *  information item required by this recommendation.
    */
    VersionMismatch = 0,

    /** An immediate child element information item of the SOAP Header element information item
     *  targeted at the faulting node that was not understood by the faulting node contained
     *  a SOAP mustUnderstand attribute information item with a value of "true"
     */
    MustUnderstand,

    /** A SOAP header block or SOAP body child element information item targeted at the faulting
     *  SOAP node is scoped with a data encoding that the faulting node does not support.
     */
    DataEncodingUnknown,

    /** The message was incorrectly formed or did not contain the appropriate information in order to succeed.
     *  For example, the message could lack the proper authentication or payment information. It is generally
     * an indication that the message is not to be resent without change
     */
    Sender,

    /** The message could not be processed for reasons attributable to the processing of the message rather
     * than to the contents of the message itself. For example, processing could include communicating with
     * an upstream SOAP node, which did not respond. The message could succeed if resent at a later point in time
     */
    Receiver,

    /** Returned when the conversion from QString cannot be done */
    UnknownFaultCode
    };

    /**
     * Reconstructs the KDSoapFaultException object from a KDSoapValue.
     * \param mainValue KDSoapValue represents the fault tag element
     */
    void deserialize( const KDSoapValue& mainValue );

    /**
     * Returns the KDSoapValue representing the <detail> tag from the parameter.
     * \param faultValue Represent the <fault> tag in the SOAP protocol
     */
    static const KDSoapValue& faultDetails(const KDSoapValue& faultValue);

    /**
     * Returns the fault code.
     */
    QString faultCode() const;

    /**
     * Set the code of the fault.
     * \param faultCode Code of the fault
     */
    void setFaultCode(const QString &faultCode);

    /**
     * Returns the human readable fault string of the fault.
     */
    QString faultString() const;

    /**
     * Set the human-readable QString of the fault.
     * \param faultString Human-readable message
     */
    void setFaultString(const QString &faultString);

    /**
     * Returns the fault actor of the fault as a QString
     */
    QString faultActor() const;

    /**
     * Set the fault actor with a string.
     * \param faultActor information about who caused the fault to happen
     */
    void setFaultActor(const QString &faultActor);

    /**
     * Returns the <detail> tag of the fault element as a KDSoapValue.
     * When the fault is generic, return an empty KDSoapValue.
     * When the fault is specific the KDSoapValue optionally contains specific serialized fault information.
     */
    KDSoapValue detailValue() const;

    /**
     * Set the KDSoapValue of the fault.
     * \param detailValue KDSoapValue representing the <detail> tag of a fault message
     */
    void setDetailValue(const KDSoapValue &detailValue);

    /**
     * Constructor
     */
    KDSoapFaultException();

    /**
     * Copy constructor
     */
    KDSoapFaultException(const KDSoapFaultException& cpy);

    /**
     * Constructs a KDSoapFaultException according to the SOAP 1.1 fault attributes.
     *
     * \param faultCode a code for identifying the fault
     * \param faultString a human readable explanation of the fault
     * \param faultActor information about who caused the fault to happen
     */
    KDSoapFaultException(const QString &faultCode, const QString &faultString, const QString &faultActor = QString());

    /**
     * Constructs a KDSoapFaultException according to the SOAP 1.2 fault attributes.
     * \param code a code for identifying the fault
     * \param reason Human-readable description of fault.
     * \param subcodes values that provides more information about the fault.
     * \param node Information regarding the actor (SOAP node) that caused the fault.
     * \param role Role being performed by actor at the time of the fault.
     */
    KDSoapFaultException(FaultCode code, const QString &reason, const QStringList &subcodes = QStringList(), const QString &node = QString(), const QString &role = QString());

    /**
     * Assignment operator
     */
    KDSoapFaultException& operator=(const KDSoapFaultException &other);

    /**
     * Destructor
     */
    ~KDSoapFaultException();

    /**
     * Set the code of the fault exception, used in SOAP 1.2 specification.
     * \param code a code for identifying the fault, given as a FaultCode enumeration
     */
    void setCode(FaultCode code);

    /**
     * Returns the fault code as a FaultCode enum, used in SOAP 1.2
     */
    FaultCode code() const;

    /**
     * Returns the fault reason as a QString, used in SOAP 1.2
     */
    QString reason() const;

    /**
     * Set the reason of the fault exception, used in SOAP 1.2 specification.
     * \param reason Human-readable description of fault, given as QString.
     */
    void setReason(const QString &reason);

    /**
     * Returns the subcodes (under the Code tag) as a QStringList, used in SOAP 1.2
     */
    const QStringList subcodes() const;

    /**
     * Set the subcodes of the fault exception, used in SOAP 1.2 specification.
     * \param subcodes values that provides more information about the fault, given as a QStringList these items will be serialize following the specification hierarchy
     */
    void setSubcodes(const QStringList &subcodes);

    /**
     * Returns the node of the fault as a QString, used in SOAP 1.2
     */
    QString node() const;

    /**
     * Set the reason of the fault exception, used in SOAP 1.2 specification.
     * \param node Information regarding the actor (SOAP node) that caused the fault.
     */
    void setNode(const QString &node);

    /**
     * Returns the fault role as a QString, used in SOAP 1.2
     */
    QString role() const;

    /**
     * Set the reason of the fault exception, used in SOAP 1.2 specification.
     * \param role Role being performed by actor at the time of the fault.
     */
    void setRole(const QString &role);

    /**
     * Conversion static function that change a QString into a FaultCode enum item. Usefull for serialization
     * \param str QString that will be converted to a FaultCode enum
     * If the conversion fail, returns an KDSoapFaultCode::UnknownFaultCode item.
     */
    static KDSoapFaultException::FaultCode faultCodeStringToEnum(const QString &str);

    /**
     * Conversion static function that change a FaultCode enum item into a QString. Usefull for deserialization
     * \param code FaultCode enum that will be converted to QString
     * If the conversion fail, returns QString()
     */
    static QString faultCodeEnumToString(KDSoapFaultException::FaultCode code);

private:
    /**
     * Internal method that handle the deserialization following SOAP 1.1 specification.
     */
    void deserializeSoap1(const KDSoapValue& mainValue);
    /**
     * Internal method that handle the deserialization following SOAP 1.2 specification.
     */
    void deserializeSoap2(const KDSoapValue& mainValue);
    /**
     * Internal method that handle the storage of the code attribute while deserializing
     */
    void storeCode(const KDSoapValue &val);
    /**
     * Internal method that handle the parsing of the subcodes attributes while deserializing.
     */
    void storeSubcodes(const KDSoapValue &subcode);
    class Private;
    QSharedDataPointer<Private> d;
};

#endif // KDSOAPFAULTEXCEPTION_H
