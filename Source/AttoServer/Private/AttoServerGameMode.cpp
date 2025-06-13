#include "AttoServerGameMode.h"

void AAttoServerGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	ErrorMessage = TEXT("No connections are not allowed");
}
