#pragma once

#include "OnlineSubsystemImpl.h"

extern ONLINESUBSYSTEMATTO_API const FName ATTO_SUBSYSTEM;

class FOnlineSessionAtto;
class FOnlineIdentityAtto;
class FAttoClient;

class ONLINESUBSYSTEMATTO_API FOnlineSubsystemAtto final : public FOnlineSubsystemImpl
{
	typedef FOnlineSubsystemImpl Super;

	TSharedPtr<FOnlineIdentityAtto> IdentityInterface;

	TSharedPtr<FOnlineSessionAtto> SessionInterface;

	TSharedPtr<FAttoClient> AttoClient;

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
};
