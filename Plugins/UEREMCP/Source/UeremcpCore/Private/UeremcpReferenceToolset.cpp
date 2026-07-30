#include "UeremcpReferenceToolset.h"



#include "UeremcpMinimalEnvelope.h"



FString UUeremcpReferenceToolset::Ping()

{

	return Ueremcp::MinimalEnvelope::MakeResponse(

		/*RequestId*/ FString(),

		TEXT("no_change_required"),

		FString::Printf(

			TEXT("UEREMCP reference toolset alive, protocol %s."),

			*Ueremcp::MinimalEnvelope::GetProtocolVersion()));

}



FString UUeremcpReferenceToolset::Echo(const FString& RequestJson)

{

	Ueremcp::MinimalEnvelope::FParsedRequest Request;

	FString ParseError;



	if (!Ueremcp::MinimalEnvelope::ParseRequest(RequestJson, Request, ParseError))

	{

		return Ueremcp::MinimalEnvelope::MakeRejection(

			FString(),

			FString::Printf(TEXT("Malformed request envelope: %s"), *ParseError));

	}



	if (!Ueremcp::MinimalEnvelope::IsProtocolCompatible(Request.ProtocolVersion))

	{

		return Ueremcp::MinimalEnvelope::MakeRejection(

			Request.RequestId,

			FString::Printf(

				TEXT("Unsupported protocol_version '%s'; this server speaks %s."),

				*Request.ProtocolVersion,

				*Ueremcp::MinimalEnvelope::GetProtocolVersion()));

	}



	return Ueremcp::MinimalEnvelope::MakeResponse(

		Request.RequestId,

		TEXT("no_change_required"),

		FString::Printf(

			TEXT("Echoed request for action '%s'. No editor state was touched."),

			*Request.Action),

		Request.Action,

		Request.TargetAssetPath);

}


