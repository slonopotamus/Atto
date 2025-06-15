#include "OnlineTimeAtto.h"
#include "AttoClient.h"
#include "OnlineSubsystemAtto.h"

FOnlineTimeAtto::FOnlineTimeAtto(FOnlineSubsystemAtto& Subsystem)
    : Subsystem(Subsystem)
{
	Subsystem.AttoClient->OnServerUtcTime.AddLambda([&](const FDateTime& ServerUtcTime) {
		LastServerUtcTime = ServerUtcTime;
		TriggerOnQueryServerUtcTimeCompleteDelegates(true, ServerUtcTime.ToIso8601(), TEXT(""));
	});
}

bool FOnlineTimeAtto::QueryServerUtcTime()
{
	// TODO: Subscribe to disconnect?
	Subsystem.AttoClient->QueryServerUtcTimeAsync();
	return true;
}

FString FOnlineTimeAtto::GetLastServerUtcTime()
{
	// TODO: Docs do not say what to return when we don't know server time yet
	return LastServerUtcTime ? LastServerUtcTime->ToIso8601() : TEXT("");
}
