#include "AttoServerGameMode.h"

AAttoServerGameMode::AAttoServerGameMode()
{
	DefaultPawnClass = nullptr;
}

void AAttoServerGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	ErrorMessage = TEXT("Do not connect to Atto server using Unreal networking");
}
