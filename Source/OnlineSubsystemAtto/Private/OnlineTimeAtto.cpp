#include "OnlineTimeAtto.h"
#include "AttoClient.h"
#include "OnlineSubsystemAtto.h"

bool FOnlineTimeAtto::QueryServerUtcTime()
{
	Subsystem.AttoClient->Send<FAttoQueryServerUtcTimeRequest>()
	    .Next([=, this](auto&& Result) {
		    if (Result.IsError())
		    {
			    // TODO: Log error?
			    TriggerOnQueryServerUtcTimeCompleteDelegates(false, GetLastServerUtcTime(), TEXT(""));
			    return;
		    }

		    LastServerUtcTime = Result.GetOkValue().ServerTime;
		    TriggerOnQueryServerUtcTimeCompleteDelegates(true, GetLastServerUtcTime(), TEXT(""));
	    });

	return true;
}

FString FOnlineTimeAtto::GetLastServerUtcTime()
{
	// TODO: Docs do not say what to return when we don't know server time yet
	return LastServerUtcTime ? LastServerUtcTime->ToIso8601() : TEXT("");
}
