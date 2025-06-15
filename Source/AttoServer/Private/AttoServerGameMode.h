#pragma once

#include "GameFramework/GameModeBase.h"
#include "AttoServerGameMode.generated.h"

UCLASS()
class ATTOSERVER_API AAttoServerGameMode : public AGameModeBase
{
	GENERATED_BODY()

	AAttoServerGameMode();

	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
};
