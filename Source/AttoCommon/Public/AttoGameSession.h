#pragma once

#include "GameFramework/GameSession.h"
#include "AttoGameSession.generated.h"

class FOnlineSessionSettings;

/**
 * Basic implementation of Game Session that creates online session on map load and destroys online session on EndPlay
 */
UCLASS()
class ATTOCOMMON_API AAttoGameSession : public AGameSession
{
	GENERATED_BODY()

	FDelegateHandle OnCreateSessionCompleteDelegateHandle;

public:
	virtual FOnlineSessionSettings BuildSessionSettings() const;

	virtual void RegisterServer() override;

	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	virtual void OnCreateSessionComplete(FName InSessionName, bool bWasSuccessful);
};
