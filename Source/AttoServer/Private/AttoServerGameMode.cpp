#include "AttoServerGameMode.h"

#include "AttoCommon.h"
#include "AttoServer.h"
#include "Kismet/GameplayStatics.h"

AAttoServerGameMode::AAttoServerGameMode()
{
	DefaultPawnClass = nullptr;
}

void AAttoServerGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	if (GetWorld()->IsPlayInEditor())
	{
		Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	}
	else
	{
		ErrorMessage = TEXT("not allowed to connect");
	}
}

void AAttoServerGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	AttoServer = FAttoServer().WithOptions(OptionsString).Create();
}

void AAttoServerGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	AttoServer.Reset();
}
