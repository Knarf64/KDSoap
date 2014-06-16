#include "converter.h"
#include <libkode/style.h>
#include "settings.h"
#include <QDebug>
#include <QCoreApplication>

using namespace KWSDL;

static const char s_jobFaultMemberName[] = "faultReply";
static const char s_exceptionThrowerMethod[] = "throwExceptionFault";
static const char s_genericException[] = "KDSoapFaultException";

SoapBinding::Style Converter::soapStyle( const Binding& binding ) const
{
    if ( binding.type() == Binding::SOAPBinding ) {
        const SoapBinding soapBinding( binding.soapBinding() );
        return soapBinding.binding().style();
    }
    return SoapBinding::RPCStyle;
}

static Part::List selectedParts( const Binding& binding, const Message& message, const Operation& operation, bool input )
{
    if ( binding.type() == Binding::SOAPBinding ) {
        const SoapBinding soapBinding = binding.soapBinding();
        const SoapBinding::Operation op = soapBinding.operations().value( operation.name() );
        const QString selectedPart = input ? op.input().part() : op.output().part();
        if (!selectedPart.isEmpty()) {
            Part::List selected;
            Q_FOREACH( const Part& part, message.parts() ) {
                if ( part.name() == selectedPart ) { // support for <soap:body parts="MoveFolderResult"/> (msexchange)
                    selected << part;
                }
            }
            return selected;
        }
    }

    return message.parts();
}

static QString fullyQualified( const KODE::Class& c ) {
    if ( c.nameSpace().isEmpty() )
        return c.name();
    else
        return c.nameSpace() + QLatin1String("::") + c.name();
}

static SoapBinding::Headers getInputHeaders( const Binding& binding, const QString& operationName )
{
    if ( binding.type() == Binding::SOAPBinding ) {
        const SoapBinding soapBinding( binding.soapBinding() );
        const SoapBinding::Operation op = soapBinding.operations().value( operationName );
        return op.inputHeaders();
    }
    return SoapBinding::Headers();
}

static SoapBinding::Headers getOutputHeaders( const Binding& binding, const QString& operationName )
{
    if ( binding.type() == Binding::SOAPBinding ) {
        const SoapBinding soapBinding( binding.soapBinding() );
        const SoapBinding::Operation op = soapBinding.operations().value( operationName );
        return op.outputHeaders();
    }
    return SoapBinding::Headers();
}

void Converter::addFaultExceptionThrower()
{
    KODE::Function exceptionThrower(QLatin1String(s_exceptionThrowerMethod), QLatin1String("void"), KODE::Function::Public, true /*static*/);
    KODE::Code code;
    if ( !mFaultExceptionsSorted.isEmpty()) {
        code += "// helper function returning the <detail> tag of the fault element";
        code += "const KDSoapValue &detail = KDSoapFaultException::faultDetails(val);";
        code += "if (!detail.childValues().isEmpty()) { // specific exception";
        code.indent();
        code += "QPair<QString,QString> entryQName; //first name , second namespace";
        code += "QList< QPair<QString,QString> > detailEntries;";
        code += "Q_FOREACH(const KDSoapValue& detailEntry, detail.childValues()) {";
        code.indent();
        code += "entryQName.first = detailEntry.name();";
        code += "entryQName.second = detailEntry.namespaceUri();";
        code += "detailEntries.append(entryQName);";
        code.unindent();
        code += "}";

        Q_FOREACH (const QString& specificFaultExceptionClass, mFaultExceptionsSorted) {
            code += "if (" + specificFaultExceptionClass + "::detailEntriesMatching(detailEntries)) {";
            code.indent();
            code += specificFaultExceptionClass + " specificFault;";
            code += "specificFault.deserialize(val);";
            //code += "qDebug() << \"client recognized an exception:\"\" " + specificFaultExceptionClass + "\";";
            code += "throw specificFault;";
            code.unindent();
            code += "}"; // end of one specific fault throwing
        }
        code.unindent();
        code += "}"; // end of specific faults block
    }
    code += "Q_UNUSED(val);";
    code += "//NOTE: we could throw an exception for all other faults here, including connection errors and malformed replies.";
    code += "//However this type of error handling is unusual in Qt applications and would constitute a behavior change compared";
    code += "//to earlier KDSoap releases. However if you would like exceptions to be thrown in all error cases, ";
    code += "//please contact us and we'll consider adding a command-line flag to kdwsdl2cpp for this.";
    code += "return;";

    exceptionThrower.addArgument( QString("const KDSoapValue &val") );
    exceptionThrower.setBody(code);
    mFileFunctions.append(exceptionThrower);
}

void Converter::sortFaultExceptions(const QString &exception, QStringList &newPartsType)
{
    // this function make sure the order of the exception recognition in the client throw
    // allows us to always throw the right one, don't let an exception cover an other one

    Q_ASSERT(newPartsType.size() > 0);
    Q_ASSERT(!mFaultExceptionsSorted.contains(exception));

    // check this fault exception isn't conflicting with previous ones
    int minIndex = 1000; // huge amount to loose the min fight
    newPartsType.sort();
    newPartsType.removeDuplicates(); // take off duplicates
    mFaultExceptionsParts[exception].sort(); // easier for the comparing logic

    Q_FOREACH(QStringList existingParts, mFaultExceptionsParts) {
        QString key = mFaultExceptionsParts.key(existingParts);
        existingParts.removeDuplicates();
        if ( existingParts.size() > newPartsType.size() )
            continue;  // cannot be equals nor a sublist : no conflict possible
        if (existingParts == newPartsType) {
            qWarning("Fault ambiguity : Be Carrefull the content of these two fault exceptions %s - %s: ", qPrintable(key), qPrintable(exception));
            continue;
        }
        else { // sublist of the new one, need to be resolved
            bool sublist = true;
            Q_FOREACH(const QString& existingPart, existingParts) {
                sublist = sublist && newPartsType.contains(existingPart);
            }
            if (sublist) {
                minIndex = qMin(minIndex, mFaultExceptionsSorted.indexOf(key));
            }
        }
    }
    mFaultExceptionsSorted.insert( (minIndex > 0) ? minIndex-1 : 0 , exception );
}

bool Converter::convertClientService()
{
    KODE::Class::List bindingClasses;
    Q_FOREACH( const Service& service, mWSDL.definitions().services() ) {
        Q_ASSERT(!service.name().isEmpty());

        QSet<QName> uniqueBindings = mWSDL.uniqueBindings( service );
        //qDebug() << "Looking at" << service.name() << uniqueBindings;

        Q_FOREACH( const QName& bindingName, uniqueBindings ) {
            const Binding binding = mWSDL.findBinding( bindingName );

            QString className = KODE::Style::className(service.name());
            QString nameSpace;
            if (uniqueBindings.count() > 1) {
                // Multiple bindings: use Service::Binding as classname.
                nameSpace = className;
                className = KODE::Style::className(bindingName.localName());
            }

            if (!Settings::self()->nameSpace().isEmpty()) {
                if (nameSpace.isEmpty())
                    nameSpace = Settings::self()->nameSpace();
                else
                    nameSpace = Settings::self()->nameSpace() + QLatin1String("::") + nameSpace;
            }

            KODE::Class newClass( className, nameSpace );
            if (!Settings::self()->exportDeclaration().isEmpty())
                newClass.setExportDeclaration(Settings::self()->exportDeclaration());
            newClass.setUseDPointer( true, QLatin1String("d_ptr") /*avoid clash with possible d() method*/ );
            newClass.addBaseClass( mQObject );
            newClass.setDocs(service.documentation());

            // Files included in the header
            newClass.addHeaderInclude(QLatin1String("QtCore/QObject"));
            newClass.addHeaderInclude(QLatin1String("QtCore/QString"));
            newClass.addHeaderInclude(QLatin1String("KDSoapClient/KDSoapClientInterface.h"));

            // Files included in the impl, with optional forward-declarations in the header
            newClass.addInclude(QLatin1String("KDSoapClient/KDSoapMessage.h"), QLatin1String("KDSoapMessage"));
            newClass.addInclude(QLatin1String("KDSoapClient/KDSoapValue.h"), QLatin1String("KDSoapValue"));
            newClass.addInclude(QLatin1String("KDSoapClient/KDSoapPendingCallWatcher.h"), QLatin1String("KDSoapPendingCallWatcher"));
            newClass.addInclude(QLatin1String("KDSoapClient/KDSoapNamespaceManager.h"));

            // Variables (which will go into the d pointer)
            KODE::MemberVariable clientInterfaceVar(QLatin1String("m_clientInterface"), QLatin1String("KDSoapClientInterface*"));
            clientInterfaceVar.setInitializer(QLatin1String("NULL"));
            newClass.addMemberVariable(clientInterfaceVar);

            KODE::MemberVariable lastReply(QLatin1String("m_lastReply"), QLatin1String("KDSoapMessage"));
            newClass.addMemberVariable(lastReply);

            KODE::MemberVariable endPoint(QLatin1String("m_endPoint"), QLatin1String("QString"));
            newClass.addMemberVariable(endPoint);

            // Ctor and dtor
            {
                KODE::Function ctor( newClass.name() );
                ctor.addArgument(KODE::Function::Argument(QLatin1String("QObject* parent"), QLatin1String("0")));
                ctor.addInitializer(QLatin1String("QObject(parent)"));
                KODE::Function dtor( QLatin1Char('~') + newClass.name() );
                KODE::Code ctorCode, dtorCode;

                ctor.setBody( ctorCode );
                newClass.addFunction( ctor );

                dtorCode += "delete d_ptr->m_clientInterface;";

                dtor.setBody( dtorCode );
                newClass.addFunction( dtor );
            }
            // ignoreSslErrors() method
            {
                KODE::Function ignoreSslErrors(QLatin1String("ignoreSslErrors"), QLatin1String("void"));
                KODE::Code code;
                code += "clientInterface()->ignoreSslErrors();";
                ignoreSslErrors.setBody(code);
                ignoreSslErrors.setDocs(QLatin1String("Asks Qt to ignore ssl errors in https requests. Use this for testing only!"));
                newClass.addFunction(ignoreSslErrors);
            }
            // setEndPoint() method
            {
                KODE::Function setEndPoint(QLatin1String("setEndPoint"), QLatin1String("void"));
                setEndPoint.addArgument( QLatin1String("const QString& endPoint") );
                KODE::Code code;
                code += "d_ptr->m_endPoint = endPoint;";
                code += "if (d_ptr->m_clientInterface)";
                code.indent();
                code += "d_ptr->m_clientInterface->setEndPoint( endPoint );";
                code.unindent();
                setEndPoint.setBody(code);
                setEndPoint.setDocs(QLatin1String("Overwrite the end point defined in the .wsdl file, with another http/https URL."));
                newClass.addFunction(setEndPoint);
            }
            //setSoapVersion() method
            {
                KODE::Function setSoapVersion(QLatin1String("setSoapVersion"), QLatin1String("void"));
                setSoapVersion.addArgument(QLatin1String("KDSoapClientInterface::SoapVersion soapVersion"));
                KODE::Code code;
                code += "clientInterface()->setSoapVersion(soapVersion);";
                setSoapVersion.setBody(code);
                setSoapVersion.setDocs(QLatin1String("Overwrite the soap version defined in the .wsdl file, with another version. \n"
                "version can be KDSoapClientInterface::SOAP1_1 or KDSoapClientInterface::SOAP1_2"));
                newClass.addFunction(setSoapVersion);
            }
            // lastError() method
            {
                KODE::Function lastError(QLatin1String("lastError"), QLatin1String("QString"));
                lastError.setConst(true);
                KODE::Code code;
                code += "if (d_ptr->m_lastReply.isFault())";
                code.indent();
                code += "return d_ptr->m_lastReply.faultAsString();";
                code.unindent();
                code += "return QString();";
                lastError.setBody(code);
                lastError.setDocs(QLatin1String("Return the error from the last blocking call.\nEmpty if no error."));
                newClass.addFunction(lastError);
            }
            //soapError() signal
            {
                KODE::Function errorSignal( QLatin1String("soapError"), QLatin1String("void"), KODE::Function::Signal );
                errorSignal.addArgument( QLatin1String("const QString& method") );
                errorSignal.addArgument( QLatin1String("const KDSoapMessage& fault") );
                errorSignal.setDocs( QLatin1String("This signal is emitted whenever a SOAP call failed, for a central processing of all SOAP errors.\nmethod is the name of the method (or operation) that returned the fault, for instance \"addContact\".") );
                newClass.addFunction(errorSignal);
            }

            QUrl webserviceLocation;

            if ( binding.type() == Binding::SOAPBinding ) {
                const SoapBinding soapBinding( binding.soapBinding() );
                const SoapBinding::Address address = soapBinding.address();
                if ( address.location().isValid() )
                    webserviceLocation = address.location();
            } else {
                // ignore non-SOAP bindings, like HTTP GET and HTTP POST
                continue;
            }

            // clientInterface() methods
            {
                KODE::Function clientInterface(QLatin1String("clientInterface"), QLatin1String("const KDSoapClientInterface*"), KODE::Function::Public);
                clientInterface.setConst(true);
                KODE::Code code;
                code += "if (!d_ptr->m_clientInterface) {";
                code.indent();
                const QByteArray encoded = webserviceLocation.toEncoded();
                code += QLatin1String("const QString endPoint = !d_ptr->m_endPoint.isEmpty() ? d_ptr->m_endPoint : QString::fromLatin1(\"") + QString::fromLatin1( encoded.data(), encoded.size() ) + QLatin1String("\");");
                code += QLatin1String("const QString messageNamespace = QString::fromLatin1(\"") + mWSDL.definitions().targetNamespace() + QLatin1String("\");");
                code += "d_ptr->m_clientInterface = new KDSoapClientInterface(endPoint, messageNamespace);";
                if ( soapStyle(binding) == SoapBinding::DocumentStyle ) {
                    code += "d_ptr->m_clientInterface->setStyle( KDSoapClientInterface::DocumentStyle );";
                }
                if (binding.version() == Binding::SOAP_1_2)
                    code += "d_ptr->m_clientInterface->setSoapVersion( KDSoapClientInterface::SOAP1_2 );";
                else
                    code += "d_ptr->m_clientInterface->setSoapVersion( KDSoapClientInterface::SOAP1_1 );";
                code.unindent();
                code += "}";
                code += "return d_ptr->m_clientInterface;";
                clientInterface.setBody(code);
                clientInterface.setDocs(QLatin1String("Returns the underlying KDSoapClientInterface instance, which allows to access setCookieJar, lastResponseHeaders, etc."));
                newClass.addFunction(clientInterface);
            }
            {
                KODE::Function mutableClientInterface(QLatin1String("clientInterface"), QLatin1String("KDSoapClientInterface*"), KODE::Function::Public);
                KODE::Code code;
                code += QLatin1String("return const_cast<KDSoapClientInterface*>( const_cast< const ") + newClass.name() + QLatin1String("*>( this )->clientInterface() );");
                mutableClientInterface.setBody( code );
                newClass.addFunction(mutableClientInterface);
            }

            SoapBinding::Headers soapHeaders;

            PortType portType = mWSDL.findPortType( binding.portTypeName() );
            //qDebug() << portType.name();
            const Operation::List operations = portType.operations();
            Q_FOREACH( const Operation& operation, operations ) {
                Operation::OperationType opType = operation.operationType();
                switch(opType) {
                case Operation::OneWayOperation: // fall-through
                case Operation::RequestResponseOperation: // the standard case
                    // sync method
                    if (!convertClientCall( operation, binding, newClass )) {
                        return false;
                    }
                    // async method
                    convertClientInputMessage( operation, binding, newClass );
                    convertClientOutputMessage( operation, binding, newClass );
                    convertFaultException(operation);
                    break;
                case Operation::SolicitResponseOperation:
                    convertClientOutputMessage( operation, binding, newClass );
                    convertClientInputMessage( operation, binding, newClass );
                    convertFaultException(operation);
                    break;
                case Operation::NotificationOperation:
                    convertClientOutputMessage( operation, binding, newClass );
                    break;
                }

                // Collect message parts used as headers
                Q_FOREACH( const SoapBinding::Header& header, getInputHeaders(binding, operation.name()) ) {
                    if ( !soapHeaders.contains(header) )
                        soapHeaders.append( header );
                }
            } // end of for each operation

            Q_FOREACH( const SoapBinding::Header& header, soapHeaders ) {
                createHeader( header, newClass );
            }
            bindingClasses.append(newClass);
            mHeaderMethods.clear();

            QString jobsNamespace = nameSpace;
            if (uniqueBindings.count() > 1) {
                // Multiple bindings: use <Service>::<Binding>Jobs as namespace for the job classes
                jobsNamespace += "::" + KODE::Style::className(bindingName.localName()) + "Jobs";
            }

            // for each operation, create a job class
            Q_FOREACH( const Operation& operation, operations ) {
                Operation::OperationType opType = operation.operationType();
                if ( opType != Operation::SolicitResponseOperation && opType != Operation::RequestResponseOperation )
                    continue;

                const QString operationName = operation.name();
                const bool handleFault = (!operation.faults().isEmpty());
                KODE::Class jobClass( upperlize( operation.name() ) + QLatin1String("Job"), jobsNamespace );
                jobClass.addInclude( QString(), fullyQualified( newClass ) );
                jobClass.addHeaderInclude( QLatin1String("KDSoapClient/KDSoapJob.h") );
                if ( !Settings::self()->exportDeclaration().isEmpty() )
                    jobClass.setExportDeclaration( Settings::self()->exportDeclaration() );

                jobClass.addBaseClass( KODE::Class( QLatin1String("KDSoapJob") ) );

                KODE::MemberVariable serviceVar( QLatin1String("service"), fullyQualified(newClass) + QLatin1Char('*') );
                jobClass.addMemberVariable( serviceVar );

                KODE::Function ctor( jobClass.name() );
                ctor.addArgument( KODE::Function::Argument( QString::fromLatin1("%1* service").arg( fullyQualified(newClass) ) ) );
                ctor.addArgument( KODE::Function::Argument( QLatin1String("QObject* parent"), QLatin1String("0") ) );
                ctor.addInitializer( QLatin1String("KDSoapJob(parent)") );
                ctor.addInitializer( QLatin1String("mService(service)") );

                const Message message = mWSDL.findMessage( operation.input().message() );
                Q_FOREACH( const Part& part, selectedParts( binding, message, operation, true /*input*/ ) ) {
                    const QString partName = part.name();
                    ctor.addInitializer( KODE::MemberVariable::memberVariableName( partName ) + "()" );
                }

                const Message outputMsg = mWSDL.findMessage( operation.output().message() );

                Q_FOREACH( const Part& part, selectedParts( binding, outputMsg, operation, false /*output*/ ) ) {
                    const QString varName = mNameMapper.escape( QLatin1String("result") + upperlize( part.name() ) );
                    ctor.addInitializer( KODE::MemberVariable::memberVariableName( varName ) + "()" );
                }

                jobClass.addFunction( ctor );

                QStringList inputGetters;

                Q_FOREACH( const Part& part, selectedParts( binding, message, operation, true /*input*/ ) ) {
                    const QString varType = mTypeMap.localType( part.type(), part.element() );
                    const KODE::MemberVariable member( part.name(), varType );
                    jobClass.addMemberVariable( member );

                    KODE::Function setter( QLatin1String("set") + mNameMapper.escape( upperlize( part.name() ) ), QLatin1String("void") );
                    setter.addArgument( mTypeMap.localInputType( part.type(), part.element() ) + QLatin1String(" arg0") );
                    KODE::Code sc;
                    sc += QString::fromLatin1("%1 = arg0;").arg( member.name() );
                    setter.setBody( sc );
                    jobClass.addFunction( setter );

                    const QString getterName = mNameMapper.escape( lowerlize( part.name() ) );
                    inputGetters.append( getterName );
                    KODE::Function getter( getterName, varType );
                    getter.setConst( true );
                    KODE::Code gc;
                    gc += QString::fromLatin1("return %1;").arg( member.name() );
                    getter.setBody( gc );
                    jobClass.addFunction( getter );
                }

                KODE::Function doStart( QLatin1String("doStart"), QLatin1String("void"), KODE::Function::Protected );
                KODE::Code doStartCode;
                const bool hasAction = clientAddAction( doStartCode, binding, operationName );
                clientGenerateMessage( doStartCode, binding, message, operation, /*use members=*/true );

                QString callLine = QString::fromLatin1("KDSoapPendingCall pendingCall = mService->clientInterface()->asyncCall(QLatin1String(\"%1\"), message").arg(operationName);
                if (hasAction) {
                    callLine += QLatin1String(", action");
                }
                callLine += QLatin1String(");");
                doStartCode += callLine;

                doStartCode += "KDSoapPendingCallWatcher *watcher = new KDSoapPendingCallWatcher(pendingCall, this);";
                doStartCode += "QObject::connect(watcher, SIGNAL(finished(KDSoapPendingCallWatcher*)),\n"
                "                 this, SLOT(slotFinished(KDSoapPendingCallWatcher*)));";
                doStart.setBody( doStartCode );
                jobClass.addFunction( doStart );

                KODE::Function slot( QLatin1String("slotFinished"), QLatin1String("void"), KODE::Function::Private|KODE::Function::Slot );
                slot.addArgument( QLatin1String("KDSoapPendingCallWatcher* watcher") );
                KODE::Code slotCode;
                slotCode += QLatin1String("watcher->deleteLater();");
                slotCode += QLatin1String("const KDSoapMessage reply = watcher->returnMessage();");
                slotCode += QLatin1String("if (!reply.isFault()) {") + COMMENT;
                slotCode.indent();

                Q_FOREACH( const Part& part, selectedParts( binding, outputMsg, operation, false /*input*/ ) ) {
                    const QString varName = mNameMapper.escape( QLatin1String("result") + upperlize( part.name() ) );
                    const KODE::MemberVariable member( varName, QString() );
                    slotCode.addBlock( deserializeRetVal(part, QLatin1String("reply"), mTypeMap.localType( part.type(), part.element() ), member.name() ) );
                    addJobResultMember(jobClass, part, varName, inputGetters, handleFault);
                }
                Q_FOREACH( const SoapBinding::Header& header, getOutputHeaders( binding, operationName ) ) {
                    const QName messageName = header.message();
                    const QString partName = header.part();
                    const Message message = mWSDL.findMessage( messageName );
                    const Part part = message.partByName( partName );

                    const QString varName = QLatin1String("resultHeader") + upperlize( part.name() );
                    const KODE::MemberVariable member( varName, QString() );
                    const QString getHeader = QString::fromLatin1("watcher->returnHeaders().header(QLatin1String(\"%1\"), QLatin1String(\"%2\"))").arg(part.element().localName(), part.element().nameSpace());
                    slotCode.addBlock( deserializeRetVal(part, getHeader, mTypeMap.localType(part.type(), part.element() ), member.name() ) );
                    addJobResultMember(jobClass, part, varName, inputGetters, handleFault);
                }
                slotCode.unindent();
                slotCode += "}"; // end of the isFault() condition

                if (!operation.faults().isEmpty()) {
                    slotCode += QLatin1String("else {");
                    slotCode.indent();
                    const QString faultVarName = mNameMapper.escape( QLatin1String(s_jobFaultMemberName) );
                    const KODE::MemberVariable member( faultVarName, QString("KDSoapMessage") );
                    jobClass.addMemberVariable(member);
                    slotCode += member.name() + " = " + "reply;" + COMMENT;
                    slotCode.unindent();
                    slotCode += QLatin1String("}");
                }
                slotCode += QLatin1String("emitFinished(reply, watcher->returnHeaders());");
                slot.setBody( slotCode );
                jobClass.addFunction( slot );

                mClasses.addClass(jobClass);

            } // end of for each operation (job creation)
        } // end of for each port
    } // end of for each service

    // Add the file static function that throws fault exceptions
    addFaultExceptionThrower();

    // First sort all classes so that the order compiles
    QStringList excludedClasses;
    excludedClasses << QLatin1String("KDSoapServerObjectInterface");
    excludedClasses << QLatin1String("KDSoapJob");
    excludedClasses << QLatin1String("KDSoapFaultException");
    mClasses.sortByDependencies(excludedClasses);
    // Then add the service, at the end

    mClasses += bindingClasses;
    return true;
}

void Converter::clientAddOneArgument( KODE::Function& callFunc, const Part& part, KODE::Class &newClass )
{
    const QString lowerName = lowerlize( part.name() );
    const QString argType = mTypeMap.localInputType( part.type(), part.element() );
    //qDebug() << "localInputType" << part.type().qname() << part.element().qname() << "->" << argType;
    if ( argType != QLatin1String("void") ) {
        QString def;
        if ( part.type().isEmpty() ) { // element (document style)
            // If the complex type is empty, we need it for serialization,
            // but we don't need the app to see it, so give it a default value.
            // Example:
            // TNS__LogoutResponse logout( const TNS__Logout& parameters = TNS__Logout() );
            const XSD::Element element = mWSDL.findElement( part.element() );
            const XSD::ComplexType ctype = mWSDL.findComplexType( element.type() );
            if ( !ctype.isNull() && ctype.isEmpty() ) {
                def = mTypeMap.localType( part.type(), part.element() ) + QLatin1String("()");
                //def += "/* " + element.type().qname() + " is empty */";
            }
        }

        KODE::Function::Argument arg( argType + QLatin1Char(' ') + mNameMapper.escape( lowerName ), def );
        callFunc.addArgument( arg );
    }
    newClass.addHeaderIncludes( mTypeMap.headerIncludes( part.type() ) );
}

void Converter::clientAddArguments( KODE::Function& callFunc, const Message& message, KODE::Class &newClass, const Operation &operation, const Binding &binding )
{
    const Part::List parts = selectedParts( binding, message, operation, true /*input*/ );
    Q_FOREACH( const Part& part, parts ) {
        clientAddOneArgument( callFunc, part, newClass );
    }
}

bool Converter::clientAddAction( KODE::Code& code, const Binding &binding, const QString& operationName )
{
    bool hasAction = false;
    if ( binding.type() == Binding::SOAPBinding ) {
        const SoapBinding soapBinding( binding.soapBinding() );
        const SoapBinding::Operation op = soapBinding.operations().value( operationName );
        if (!op.action().isEmpty()) {
            code += QLatin1String("const QString action = QString::fromLatin1(\"") + op.action() + QLatin1String("\");");
            hasAction = true;
        }
    }
    return hasAction;
}

// Maybe the "qualified" bool isn't useful after all, now that we make sure
// that all messages are qualified (it's really only configurable for child elements)
QName Converter::elementNameForPart(const Part& part, bool* qualified, bool *nillable) const
{
    if (part.type().isEmpty()) { // element (document style)
        XSD::Element element = mWSDL.findElement(part.element());
        *qualified = element.isQualified();
        *nillable = element.nillable();
        return element.qualifiedName();
    } else { // type (rpc style)
        *qualified = false;
        *nillable = false;
        return QName(part.nameSpace(), part.name());
    }
}

void Converter::addMessageArgument( KODE::Code& code, const SoapBinding::Style& bindingStyle, const Part& part, const QString& localVariableName, const QByteArray& messageName, bool varIsMember )
{
    const QString partname = varIsMember ? QLatin1Char('m') + upperlize( localVariableName ) :  lowerlize( localVariableName );
    // In document style, the "part" is directly added as arguments
    // See http://www.ibm.com/developerworks/webservices/library/ws-whichwsdl/
    if ( bindingStyle == SoapBinding::DocumentStyle )
        code.addBlock( serializePart(part, partname, messageName, false ) );
    else {
        const QString argType = mTypeMap.localType( part.type(), part.element() );
        if ( argType != QLatin1String("void") ) {
            code.addBlock( serializePart(part, partname, messageName + ".childValues()", true) );
        }
    }
}

void Converter::clientGenerateMessage( KODE::Code& code, const Binding& binding, const Message& message, const Operation& operation, bool varsAreMembers )
{
    code += "KDSoapMessage message;";

    if ( binding.type() == Binding::SOAPBinding ) {
        const SoapBinding soapBinding = binding.soapBinding();
        const SoapBinding::Operation op = soapBinding.operations().value( operation.name() );
        if ( op.input().use() == SoapBinding::EncodedUse )
            code += "message.setUse(KDSoapMessage::EncodedUse);";
        else
            code += "message.setUse(KDSoapMessage::LiteralUse);";
        //qDebug() << "input headers:" << op.inputHeaders().count();
    }

    Q_FOREACH( const Part& part, selectedParts( binding, message, operation, true /*input*/ ) ) {
        addMessageArgument( code, soapStyle(binding), part, part.name(), "message", varsAreMembers );
    }
}

KODE::Code Converter::deserializeRetVal(const KWSDL::Part& part, const QString& replyMsgName, const QString& qtRetType, const QString& varName) const
{
    // This is the opposite logic as appendElementArg, which does:
    // if builtin -> xml element with basic contents
    // if complex -> serialize and add result
    // else -> serialize

    KODE::Code code;
    const bool isBuiltin = mTypeMap.isBuiltinType( part.type(), part.element() );
    const bool isComplex = mTypeMap.isComplexType( part.type(), part.element() );
    const bool isPolymorphic = mTypeMap.isPolymorphic( part.type(), part.element() );
    if ( isBuiltin ) {
        code += varName + QLatin1String(" = ") + mTypeMap.deserializeBuiltin( part.type(), part.element(), replyMsgName + QLatin1String(".value()"), qtRetType ) + QLatin1String(";")+ COMMENT;
    } else if ( isComplex ) {
        const QString op = isPolymorphic ? "->" : ".";
        code += varName + op + QLatin1String("deserialize(") + replyMsgName + QLatin1String(");") + COMMENT;
    } else {
        // testcase: MyWsdlDocument::sendTelegram
        code += varName + QLatin1String(".deserialize(") + replyMsgName + QLatin1String(".value());") + COMMENT;
    }
    return code;
}

// Generate synchronous call
bool Converter::convertClientCall( const Operation &operation, const Binding &binding, KODE::Class &newClass )
{
  const QString methodName = lowerlize( operation.name() );
  KODE::Function callFunc( mNameMapper.escape( methodName ), QLatin1String("void"), KODE::Function::Public );
  callFunc.setDocs(QString::fromLatin1("Blocking call to %1.\nNot recommended in a GUI thread.").arg(operation.name()));
  const Message inputMessage = mWSDL.findMessage( operation.input().message() );
  Message outputMessage;
  if (operation.operationType() != Operation::OneWayOperation) {
      outputMessage = mWSDL.findMessage( operation.output().message() );
  }
  clientAddArguments( callFunc, inputMessage, newClass, operation, binding );
  KODE::Code code;
  const bool hasAction = clientAddAction( code, binding, operation.name() );
  clientGenerateMessage( code, binding, inputMessage, operation );
  QString callLine = QLatin1String("d_ptr->m_lastReply = clientInterface()->call(QLatin1String(\"") + operation.name() + QLatin1String("\"), message");
  if (hasAction) {
      callLine += QLatin1String(", action");
  }
  callLine += QLatin1String(");");
  code += callLine;

  // Return value(s) :
  const Part::List outParts = selectedParts( binding, outputMessage, operation, false /*output*/ );
  const int numReturnValues = outParts.count();

  if (numReturnValues) {
      const Part retPart = outParts.first();
      const QString retType = mTypeMap.localType( retPart.type(), retPart.element() );
      if ( retType.isEmpty() ) {
          qWarning("Could not generate operation '%s'", qPrintable(operation.name()));
          return false;
      }
      callFunc.setReturnType( retType );

      code += "if (d_ptr->m_lastReply.isFault())";
      code.indent();
      //code += QLatin1String("return ") + retType + QLatin1String("();"); // default-constructed value
      code += QLatin1String(s_exceptionThrowerMethod) + QLatin1String("( d_ptr->m_lastReply );");
      code.unindent();

      // WARNING: if you change the logic below, also adapt the result parsing for async calls

      if ( retType != QLatin1String("void") ) {
          if ( soapStyle(binding) == SoapBinding::DocumentStyle /*no wrapper*/ ) {
              code += retType + QLatin1String(" ret;"); // local var
              code.addBlock(deserializeRetVal(retPart, QLatin1String("d_ptr->m_lastReply"), retType, QLatin1String("ret")));
              code += QLatin1String("return ret;") + COMMENT;
          } else { // RPC style (adds a wrapper), or simple value
              // Protect the call to .first() below
              code += "if (d_ptr->m_lastReply.childValues().isEmpty()) {";
              code.indent();
              code += "d_ptr->m_lastReply.setFault(true);";
              code += "d_ptr->m_lastReply.addArgument(QString::fromLatin1(\"faultcode\"), QString::fromLatin1(\"Server.EmptyResponse\"));";
              code += QLatin1String("return ") + retType +QLatin1String( "();"); // default-constructed value
              code.unindent();
              code += "}";

              code += retType + QLatin1String(" ret;"); // local var
              code += QLatin1String("const KDSoapValue val = d_ptr->m_lastReply.childValues().first();") + COMMENT;
              code += demarshalVar( retPart.type(), retPart.element(), QLatin1String("ret"), retType, "val", false );
              code += "return ret;";
          }
      }

  } else if (numReturnValues > 1){
      // Add each output argument as a non-const ref to the method. A bit ugly but no other way.

      code += "if (d_ptr->m_lastReply.isFault())";
      code.indent();
      code += QLatin1String("return;")+ COMMENT;
      code.unindent();
      Q_ASSERT(soapStyle(binding) == SoapBinding::DocumentStyle); // RPC with multiple return values? impossible, we generate a single wrapper

      Q_FOREACH(const Part& part, outParts) {
          const QString argType = mTypeMap.localType( part.type(), part.element() );
          Q_ASSERT(!argType.isEmpty());
          const QString lowerName = lowerlize( part.name() );
          KODE::Function::Argument arg( argType + QLatin1String("& ") + mNameMapper.escape( lowerName ) );
          callFunc.addArgument( arg );
          newClass.addHeaderIncludes( mTypeMap.headerIncludes( part.type() ) );

          code.addBlock(deserializeRetVal(part, QLatin1String("d_ptr->m_lastReply"), argType, lowerName));
      }
  }

  callFunc.setBody( code );

  newClass.addFunction( callFunc );
  return true;
}

// Generate async call method
void Converter::convertClientInputMessage( const Operation &operation,
                                           const Binding &binding, KODE::Class &newClass )
{
  QString operationName = operation.name();
  KODE::Function asyncFunc( QLatin1String("async") + upperlize( operationName ), QLatin1String("void"), KODE::Function::Public );
  asyncFunc.setDocs(QString::fromLatin1("Asynchronous call to %1.\n"
                            "Remember to connect to %2 and %3.")
                    .arg(operation.name())
                    .arg(lowerlize(operationName) + QLatin1String("Done"))
                    .arg(lowerlize(operationName) + QLatin1String("Error")));
  const Message message = mWSDL.findMessage( operation.input().message() );
  clientAddArguments( asyncFunc, message, newClass, operation, binding );
  KODE::Code code;
  const bool hasAction = clientAddAction( code, binding, operation.name() );
  clientGenerateMessage( code, binding, message, operation );

  QString callLine = QLatin1String("KDSoapPendingCall pendingCall = clientInterface()->asyncCall(QLatin1String(\"") + operationName + QLatin1String("\"), message");
  if (hasAction) {
      callLine += QLatin1String(", action");
  }
  callLine += QLatin1String(");");
  code += callLine;

  if (operation.operationType() == Operation::RequestResponseOperation ||
      operation.operationType() == Operation::OneWayOperation) {
      const QString finishedSlotName = QLatin1String("_kd_slot") + upperlize(operationName) + QLatin1String("Finished");

      code += "KDSoapPendingCallWatcher *watcher = new KDSoapPendingCallWatcher(pendingCall, this);";
      code += QLatin1String("QObject::connect(watcher, SIGNAL(finished(KDSoapPendingCallWatcher*)),\n"
              "                 this, SLOT(") + finishedSlotName + QLatin1String("(KDSoapPendingCallWatcher*)));");
      asyncFunc.setBody( code );
      newClass.addFunction( asyncFunc );
  }
}

// Generate signals and the result slot, for async calls
void Converter::convertClientOutputMessage( const Operation &operation,
                                            const Binding &binding, KODE::Class &newClass )
{
  // result signal
  QString operationName = lowerlize( operation.name() );
  KODE::Function doneSignal( operationName + QLatin1String("Done"), QLatin1String("void"), KODE::Function::Signal );
  doneSignal.setDocs( QLatin1String("This signal is emitted whenever the call to ") + operationName+ QLatin1String("() succeeded.") );

  // error signal
  KODE::Function errorSignal( operationName + QLatin1String("Error"), QLatin1String("void"), KODE::Function::Signal );
  errorSignal.addArgument( QLatin1String("const KDSoapMessage& fault") );
  errorSignal.setDocs( QLatin1String("This signal is emitted whenever the call to ") + operationName + QLatin1String("() failed.") );

  // finished slot
  const QString finishedSlotName = QLatin1String("_kd_slot") + upperlize(operationName) + QLatin1String("Finished");
  KODE::Function finishedSlot( finishedSlotName, QLatin1String("void"), KODE::Function::Slot | KODE::Function::Private );
  finishedSlot.addArgument( QLatin1String("KDSoapPendingCallWatcher* watcher") );

  // If one output message is used by two input messages, don't define
  // it twice.
  // DF: what if the arguments are different? ...
  //if ( newClass.hasFunction( respSignal.name() ) )
  //  return;

  KODE::Code slotCode;
  slotCode += "const KDSoapMessage reply = watcher->returnMessage();";
  slotCode += "if (reply.isFault()) {";
  slotCode.indent();
  slotCode += QLatin1String("Q_EMIT ") + errorSignal.name() + QLatin1String("(reply);") + COMMENT;
  slotCode += QLatin1String("Q_EMIT soapError(QLatin1String(\"") + operationName + QLatin1String("\"), reply);");
  slotCode.unindent();
  slotCode += "} else {";
  slotCode.indent();

  QStringList partNames;

  if (operation.operationType() != Operation::OneWayOperation) {
      const Message message = mWSDL.findMessage( operation.output().message() );

      const Part::List parts = selectedParts( binding, message, operation, false /*output*/ );
      Q_FOREACH( const Part& part, parts ) {
          const QString partType = mTypeMap.localType( part.type(), part.element() );
          if ( partType.isEmpty() ) {
              qWarning("Skipping part '%s'", qPrintable(part.name()));
              continue;
          }

          if ( partType == QLatin1String("void") )
              continue;

          QString lowerName = mNameMapper.escape( lowerlize( part.name() ) );
          doneSignal.addArgument( mTypeMap.localInputType( part.type(), part.element() ) + QLatin1Char(' ') + lowerName );

          // WARNING: if you change the logic below, also adapt the result parsing for sync calls, above

          if ( soapStyle(binding) == SoapBinding::DocumentStyle /*no wrapper*/ ) {
              slotCode += partType + QLatin1String(" ret;"); // local var
              slotCode.addBlock(deserializeRetVal(part, QLatin1String("reply"), partType, QLatin1String("ret")));
              partNames << QLatin1String("ret");
          } else { // RPC style (adds a wrapper) or simple value
              QString value = QLatin1String("reply.childValues().child(QLatin1String(\"") + part.name() + QLatin1String("\"))");
              const bool isBuiltin = mTypeMap.isBuiltinType( part.type(), part.element() );
              if ( isBuiltin ) {
                  partNames << value + QLatin1String(".value().value<") + partType + QLatin1String(">()");
              } else {
                  slotCode += partType + QLatin1String(" ret;"); // local var. TODO ret1/ret2 etc. if more than one.
                  const bool isComplex = mTypeMap.isComplexType( part.type(), part.element() );
                  if (!isComplex)
                      value += QLatin1String(".value()");
                  slotCode += QLatin1String("ret.deserialize(") + value + QLatin1String(");") + COMMENT;
                  partNames << QLatin1String("ret");
              }
          }

          // Forward declaration of element class
          //newClass.addIncludes( QStringList(), mTypeMap.forwardDeclarationsForElement( part.element() ) );
      }
  }

  newClass.addFunction( doneSignal );
  newClass.addFunction( errorSignal );

  slotCode += QLatin1String("Q_EMIT ") + doneSignal.name() + QLatin1String("( ") + partNames.join( QLatin1String(",") ) + QLatin1String(" );");
  slotCode.unindent();
  slotCode += '}';
  slotCode += "watcher->deleteLater();";

  finishedSlot.setBody(slotCode);

  newClass.addFunction(finishedSlot);
}

// Generate the exceptions class for each fault
void Converter::generateFaultCtorsDtors(KODE::Class& exceptionClass)
{
    const QString exceptionClassName = exceptionClass.name();
    KODE::Function ctor( exceptionClassName );
    KODE::Code ctorEmptyCode; KODE::Code dtorEmptyCode;
    ctor.setBody(ctorEmptyCode);

    KODE::Function ctorSOAP1( exceptionClassName );
    ctorSOAP1.addArgument(QLatin1String("const QString &faultCode"));
    ctorSOAP1.addArgument(QLatin1String("const QString &faultString"));
    ctorSOAP1.addArgument(QLatin1String("const QString &faultActor"));
    //ctorSOAP1.addArgument("const " + varType + " &faultType");
    QLatin1String genericSoap1Init("KDSoapFaultException(faultCode, faultString, faultActor)");
    ctorSOAP1.addInitializer(genericSoap1Init);
    KODE::Code ctorSoapCode;
    //ctorSoapCode += QLatin1String("d_ptr->m_faultType = faultType;");
    ctorSOAP1.setBody(ctorSoapCode);

    KODE::Function ctorSOAP1_2( exceptionClassName );
    ctorSOAP1_2.addArgument(QLatin1String("KDSoapFaultException::FaultCode code"));
    ctorSOAP1_2.addArgument(QLatin1String("const QString &reason"));
    ctorSOAP1_2.addArgument(QLatin1String("const QStringList &subcodes"));
    ctorSOAP1_2.addArgument(QLatin1String("const QString &node"));
    ctorSOAP1_2.addArgument(QLatin1String("const QString &role"));
    QLatin1String genericSOAP1_2Init("KDSoapFaultException(code, reason, subcodes, node, role)");
    ctorSOAP1_2.addInitializer(genericSOAP1_2Init);
    ctorSOAP1_2.setBody(ctorSoapCode);

    KODE::Function dtor( QLatin1Char('~') + exceptionClassName );
    dtor.setVirtualMode( KODE::Function::Virtual );
    dtor.setBody(dtorEmptyCode);

    exceptionClass.addFunction(ctor);
    exceptionClass.addFunction(ctorSOAP1);
    exceptionClass.addFunction(ctorSOAP1_2);
    exceptionClass.addFunction(dtor);
}

void Converter::convertFaultException(const Operation& operation)
{
    if (operation.faults().isEmpty())
        return;

    Q_FOREACH(const Fault& operationfault,  operation.faults())
    {
        KODE::Class exceptionClass;
        Q_ASSERT(!operationfault.name().isEmpty());
        const QString exceptionClassName = upperlize(operationfault.name() + "Exception");
        if (mFaultExceptionClasses.contains(exceptionClassName)) {
            //qDebug() << "already in cached so no being treated" << exceptionClassName;
            continue; // we skip only this turn
        }
        exceptionClass.setName(exceptionClassName);
        exceptionClass.setUseSharedData( true, QLatin1String("d_ptr"));
        exceptionClass.addHeaderInclude("KDSoapClient/KDSoapFaultException.h");
        exceptionClass.addBaseClass( KODE::Class( QLatin1String(s_genericException) ) );
        const Message message = mWSDL.findMessage(operationfault.message());

        generateFaultCtorsDtors(exceptionClass);
        generateFaultSerialization(exceptionClass, operationfault, message.parts());

        mClasses.append(exceptionClass);
        mFaultExceptionClasses.append(exceptionClassName); // used as cache
    }
}

void Converter::generateFaultSerialization(KODE::Class& exceptionClass, const Fault& fault, const QList<Part>& parts)
{
    // preparing variables for later on
    const QLatin1String argName("mainValue");
    const QString detailTag = "detailTag";
    QStringList newPartsType;
    int indexLoop = 0;
    QString boolInit;
    for(int i= 0; i < parts.size()-1; i++)
        boolInit += "false << ";

    KODE::Function serialize(QLatin1String("serialize"));
    serialize.addArgument(QLatin1String("const QString& valueName"));
    serialize.setReturnType(QLatin1String("KDSoapValue"));
    serialize.setConst(true);
    KODE::Code serializeBody;

    KODE::Function deserialize(QLatin1String("deserialize"));
    deserialize.addArgument("const KDSoapValue& " + argName);
    deserialize.setReturnType(QLatin1String("void"));
    KODE::Code deserializeBody;

    KODE::Function detailEntries(QLatin1String("detailEntriesMatching"));
    detailEntries.setStatic(true);
    detailEntries.setReturnType("bool");

    exceptionClass.addTypedef(KODE::Typedef(QString("QList< QPair<QString, QString> >"), QString("DetailEntries")));
    exceptionClass.addTypedef(KODE::Typedef(QString("QPair<QString, QString>"), QString("DetailEntry")));

    detailEntries.setArgumentString(QString("const DetailEntries &detailEntriesElements"));
    KODE::Code detailEntriesBody;

    Q_FOREACH(const Part& part, parts) {
        QName partElementTypeName = part.element();
        if (partElementTypeName.isEmpty()) {
            qWarning("fault without xsd element defined in <part> NOT SUPPORTED");
            continue;
        }
        const QString varType = mTypeMap.localType( part.type(), partElementTypeName );
        const bool isComplexType = mTypeMap.isComplexType( part.type(), partElementTypeName );
        const QString memberName = KODE::MemberVariable::memberVariableName(part.name());
        newPartsType.append(partElementTypeName.localName());

        //QString debugType = (isComplexType) ? "complex" : "simple";
        //qDebug() << "Fault conversion" << fault.name() << " : part " << part.name() << "of this "+ debugType +" element " << partElementTypeName.localName() << " ns "<< partElementTypeName.nameSpace();

        QString localInputType = mTypeMap.localInputType( mWSDL.findElement(partElementTypeName).type(), QName() );
        generateMemberVariable(part.name(), varType, localInputType, exceptionClass, XSD::Attribute::Required, false /*no polymorphic*/ );

        // serialization
        {
            if (serializeBody.isEmpty()) {
                serializeBody += "KDSoapValue " + detailTag + "(valueName, QVariant(), " + "\"" + fault.nameSpace() + "\"" +", QString() /* set only if one complex type*/);";
                serializeBody += "KDSoapValueList& args = " + detailTag + ".childValues();";
            }
            serializeBody.addBlock(serializePart(part, "d_ptr->"+memberName, "args", true));
        }

        // deserilization
        {
            if (deserializeBody.isEmpty()) {
                deserializeBody += QString(s_genericException) + "::" + "deserialize(" + argName + ");";
                deserializeBody += "const KDSoapValue& "+ detailTag +" = " + QString(s_genericException) + "::faultDetails(" + argName + ");";
                deserializeBody += "if (" + detailTag + ".isNull()) return;";
                deserializeBody += "const KDSoapValueList& args = " + detailTag +".childValues();";
                deserializeBody += "for (int argNr = 0; argNr < args.count(); ++argNr) {";
                deserializeBody.indent();
                deserializeBody += "const KDSoapValue& val = args.at(argNr);";
                deserializeBody += "const QString name = val.name();";
            }
            deserializeBody += "if (name == QLatin1String(\"" + partElementTypeName.localName() + "\")) {";
            deserializeBody.indent();
            QString ptr = exceptionClass.dPointerName() + "->";

            if (isComplexType) {
                deserializeBody += ptr + memberName +".deserialize(val);";
                exceptionClass.addInclude(QString(), varType); // since we are here let's add the dependency
            } else {
                deserializeBody += ptr + memberName + QLatin1String(" = ") + mTypeMap.deserializeBuiltin(QName(), QName(), "val"+ QLatin1String(".value()"), varType) + ";" + COMMENT ;
            }
            deserializeBody.unindent();
            deserializeBody += "}";
        }

        // exception identification function
        {
            if (detailEntriesBody.isEmpty()) {
                detailEntriesBody += "bool match = true;";
                detailEntriesBody += "QList<bool> matchTab = QList<bool>();";
                detailEntriesBody += "matchTab << " + boolInit + "false;" ;
                detailEntriesBody += "Q_FOREACH( const DetailEntry &entryElement, detailEntriesElements) {";
                detailEntriesBody.indent();
            }
            //detailEntriesBody += QString("matchTab = matchTab && ( entryElement.first == \"%1\"  && entryElement.second == \"%2\" );")
                    //.arg(partElementTypeName.localName()).arg(partElementTypeName.nameSpace());

            detailEntriesBody += QString("matchTab[%3] = matchTab[%3] || ( entryElement.first == \"%1\"  && entryElement.second == \"%2\" );")
                      .arg(partElementTypeName.localName()).arg(partElementTypeName.nameSpace()).arg(indexLoop);
        }
        indexLoop ++;
    }   //end of foreach

    if (!serializeBody.isEmpty()) {
        serializeBody += "return " + detailTag + ";";
    }
    if (!deserializeBody.isEmpty()) {
        deserializeBody.unindent();
        deserializeBody += "}";
    }
    if (detailEntriesBody.isEmpty()) {
        detailEntriesBody += "// ERROR : no xsd:element type provided to build the fault structure";
        detailEntriesBody += "return false;";
    } else {
        detailEntriesBody.unindent();
        detailEntriesBody += "}";
        // we make sure all exception elements were there to return true
        detailEntriesBody += QString("Q_FOREACH(const bool &matchOnce, matchTab)  ");
        detailEntriesBody.indent();
        detailEntriesBody += QString("match = match && matchOnce;");
        detailEntriesBody.unindent();
        detailEntriesBody += "return match;";
    }
    sortFaultExceptions( exceptionClass.name(), newPartsType );

    // let's add them since they are completed !
    deserialize.setBody(deserializeBody);
    exceptionClass.addFunction(deserialize);

    serialize.setBody(serializeBody);
    exceptionClass.addFunction(serialize);

    detailEntries.setBody(detailEntriesBody);
    exceptionClass.addFunction(detailEntries);
}

void Converter::createHeader( const SoapBinding::Header& header, KODE::Class &newClass )
{
    const QName messageName = header.message();
    const QString partName = header.part();
    const Message message = mWSDL.findMessage( messageName );
    const Part part = message.partByName( partName );

    {
        QString methodName = QLatin1String("set") + upperlize( partName );
        if (!methodName.endsWith(QLatin1String("Header")))
            methodName += QLatin1String("Header");

        const QString argType = mTypeMap.localInputType( part.type(), part.element() );
        const QString methodSig = methodName + QLatin1Char('(') + argType + QLatin1Char(')');
        if (mHeaderMethods.contains(methodSig))
            return; // already have it (testcase: Services.wsdl)

        mHeaderMethods.insert(methodSig);
        KODE::Function headerSetter( methodName, QLatin1String("void"), KODE::Function::Public );
        headerSetter.setDocs(QString::fromLatin1("Sets the header '%1', for all subsequent method calls.").arg(partName));
        clientAddOneArgument( headerSetter, part, newClass );
        KODE::Code code;
        code += "KDSoapMessage message;";
        if ( header.use() == SoapBinding::EncodedUse )
            code += "message.setUse(KDSoapMessage::EncodedUse);";
        else
            code += "message.setUse(KDSoapMessage::LiteralUse);";
        addMessageArgument( code, SoapBinding::RPCStyle, part, partName, "message" );
        code += QLatin1String("clientInterface()->setHeader( QLatin1String(\"") + partName + QLatin1String("\"), message );");
        headerSetter.setBody(code);
        newClass.addFunction(headerSetter);
    }

    // Let's also generate a clear method
    {
        QString methodName = QLatin1String("clear") + upperlize( partName );
        if (!methodName.endsWith(QLatin1String("Header")))
            methodName += QLatin1String("Header");
        KODE::Function headerClearer( methodName, QLatin1String("void"), KODE::Function::Public );
        headerClearer.setDocs(QString::fromLatin1("Removes the header '%1', for all subsequent method calls.").arg(partName));
        KODE::Code code;
        code += QLatin1String("clientInterface()->setHeader( QLatin1String(\"") + partName + QLatin1String("\"), KDSoapMessage() );");
        headerClearer.setBody(code);
        newClass.addFunction(headerClearer);
    }
}

void Converter::addJobResultMember(KODE::Class& jobClass, const Part& part, const QString& varName, const QStringList& inputGetters, bool handleFault)
{
    const QString varType = mTypeMap.localType( part.type(), part.element() );
    const KODE::MemberVariable member( varName, varType );
    jobClass.addMemberVariable( member );

    QString getterName = mNameMapper.escape( lowerlize( part.name() ) );
    while ( inputGetters.contains( getterName ) )
        getterName = mNameMapper.escape( QLatin1String("result") + upperlize( getterName ) );

    KODE::Function getter( getterName, varType );
    getter.setConst( true );
    KODE::Code gc;

    if (handleFault) {
        gc += "if (" + KODE::MemberVariable::memberVariableName(s_jobFaultMemberName) + ".isFault())";
        gc.indent();
        gc += QString(s_exceptionThrowerMethod) + "(m" + upperlize(QString(s_jobFaultMemberName)) + ");";
        gc.unindent();
    }
    gc += QString::fromLatin1("return %1;").arg( member.name() );
    getter.setBody( gc );
    jobClass.addFunction( getter );

}
