#pragma once

#include "OnlineSubsystemImpl.h"

extern ONLINESUBSYSTEMATTO_API const FName ATTO_SUBSYSTEM;

class FAttoClient;
class FOnlineAsyncTaskManagerAtto;
class FOnlineIdentityAtto;
class FOnlineSessionAtto;
class FOnlineTimeAtto;

class ONLINESUBSYSTEMATTO_API FOnlineSubsystemAtto final : public FOnlineSubsystemImpl
{
	typedef FOnlineSubsystemImpl Super;

	TSharedPtr<FRunnableThread> OnlineAsyncTaskThread;

public:
	explicit FOnlineSubsystemAtto(const FName InInstanceName)
	    : FOnlineSubsystemImpl(ATTO_SUBSYSTEM, InInstanceName)
	{
	}

	virtual bool Init() override;

	virtual bool Shutdown() override;

	virtual bool Tick(float DeltaSeconds) override;

	virtual FString GetAppId() const override;

	virtual IOnlineFriendsPtr GetFriendsInterface() const override;

	virtual TSharedPtr<IOnlineIdentity> GetIdentityInterface() const override;

	virtual FText GetOnlineServiceName() const override;

	virtual TSharedPtr<IOnlineSession> GetSessionInterface() const override;

	virtual TSharedPtr<IOnlineTime> GetTimeInterface() const override;

	TSharedPtr<FAttoClient> AttoClient;

	TSharedPtr<FOnlineAsyncTaskManagerAtto> TaskManager;

	TSharedPtr<FOnlineIdentityAtto> IdentityInterface;

	TSharedPtr<FOnlineTimeAtto> TimeInterface;

	TSharedPtr<FOnlineSessionAtto> SessionInterface;
};
