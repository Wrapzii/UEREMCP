#include "UeremcpHonestyContract.h"

#include "UeremcpEnvelope.h"

namespace UeremcpHonestyContract
{
	bool IsValidatedStatus(const FString& Status)
	{
		return Status.Equals(TEXT("created_and_validated"), ESearchCase::CaseSensitive)
			|| Status.Equals(TEXT("modified_and_validated"), ESearchCase::CaseSensitive);
	}

	FString ResolveStatusHonoringValidateFlag(
		const FUeremcpRequest& Request,
		const FString& ProposedStatus)
	{
		if (!Request.bValidate && IsValidatedStatus(ProposedStatus))
		{
			return TEXT("partially_completed");
		}
		return ProposedStatus;
	}

	void ApplyValidateFlagToResponse(
		const FUeremcpRequest& Request,
		FUeremcpResponse& Response)
	{
		const FString Resolved = ResolveStatusHonoringValidateFlag(Request, Response.Status);
		if (!Resolved.Equals(Response.Status, ESearchCase::CaseSensitive))
		{
			Response.CapabilityNotes.Add(
				TEXT("options.validate=false: envelope contract forbids *_validated status"));
			Response.Status = Resolved;
		}
	}

	bool IsHonestFailureStatus(const FString& Status)
	{
		return Status.Equals(TEXT("failed_validation"), ESearchCase::CaseSensitive)
			|| Status.Equals(TEXT("rejected"), ESearchCase::CaseSensitive);
	}

	bool HasActionableDiagnostics(const FUeremcpResponse& Response)
	{
		if (!Response.Summary.IsEmpty())
		{
			return true;
		}
		for (const FString& Note : Response.CapabilityNotes)
		{
			if (!Note.IsEmpty())
			{
				return true;
			}
		}
		return false;
	}
}
