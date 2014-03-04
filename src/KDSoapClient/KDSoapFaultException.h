#ifndef KDSOAPFAULTEXCEPTION_H
#define KDSOAPFAULTEXCEPTION_H

#include <QString>
#include <QStringList>
#include <QUrl>
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
        MustUnderstand = 1,

        /** A SOAP header block or SOAP body child element information item targeted at the faulting
         *  SOAP node is scoped with a data encoding that the faulting node does not support.
         */
        DataEncodingUnknown = 2,

        /** The message was incorrectly formed or did not contain the appropriate information in order to succeed.
         *  For example, the message could lack the proper authentication or payment information. It is generally
         * an indication that the message is not to be resent without change
         */
        Sender = 3,

        /** The message could not be processed for reasons attributable to the processing of the message rather
         * than to the contents of the message itself. For example, processing could include communicating with
         * an upstream SOAP node, which did not respond. The message could succeed if resent at a later point in time
         */
        Receiver = 4
    };

    /**
     * Reconstructs the KDSoapFaultException object from a KDSoapValue.
     * \param mainValue KDSoapValue represents the fault tag element
     */
    void deserialize( const KDSoapValue& mainValue );

    /**
     * Returns the KDSoapValue representing the <detail> tVag from the parameter.
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
     * Constructs a KDSoapFaultException according to the SOAP 1.1 attriutes.
     *
     * \param faultCode a code for identifying the fault
     * \param faultString a human readable explanation of the fault
     * \param faultActor information about who caused the fault to happen
     */
    KDSoapFaultException(const QString &faultCode, const QString &faultString, const QString &faultActor = QString());

    /**
     * Constructs a KDSoapFaultException according to the SOAP 1.2 attriutes.
     * TODO: finish this doc
     * \param code a code for identifying the fault
     * \param reason a human readable explanation of the fault
     * \param subCodes all the application specific string code
     * \param node URL but given as string here
     * \param role URL but given as string here
     */
    KDSoapFaultException(const FaultCode &code, const QString &reason, const QStringList &subcodes = QStringList(), const QString &node = QString(), const QString &role = QString());

    /**
     * Assignment operator
     */
    KDSoapFaultException& operator=(const KDSoapFaultException &other);

    /**
     * Destructor
     */
    ~KDSoapFaultException();

    void setCode(const FaultCode &code);
    FaultCode code() const;

    QString reason() const;
    void setReason(const QString &reason);

    const QStringList subcodes() const;
    void setSubcodes(const QStringList &subcodes);

    QString node() const;
    void setNode(const QString &node);
    QString role() const;
    void setRole(const QString &role);

    // change it to static
    KDSoapFaultException::FaultCode faultCodeStringToEnum(const QString &str) const;
    QString faultCodeEnumToString(const FaultCode &code) const;

private:
    void deserializeSoap1( const KDSoapValue& mainValue );
    void deserializeSoap2( const KDSoapValue& mainValue );
    void storeSubcodes(const KDSoapValue &code);

    class Private;
    QSharedDataPointer<Private> d;
};

#endif // KDSOAPFAULTEXCEPTION_H
