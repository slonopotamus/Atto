#pragma once

#include "AttoServer.h"
#include "GameFramework/GameModeBase.h"

#include "AttoServerGameMode.generated.h"

UCLASS()
class ATTOSERVER_API AAttoServerGameMode : public AGameModeBase
{
	GENERATED_BODY()

	TUniquePtr<FAttoServer> AttoServer;

protected:
	AAttoServerGameMode();

	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
};
