#pragma once

#include "Interfaces/OnlineIdentityInterface.h"

class FOnlineSubsystemAtto;
class FUniqueNetIdAtto;

class FOnlineIdentityAtto final : public IOnlineIdentity
{
	FOnlineSubsystemAtto& Subsystem;

	TMap<int32, TSharedRef<const FUniqueNetIdAtto>> LocalUsers;
	TMap<TSharedRef<const FUniqueNetId>, TSharedRef<FUserOnlineAccount>> Accounts;

public:
	explicit FOnlineIdentityAtto(FOnlineSubsystemAtto& Subsystem)
	    : Subsystem(Subsystem)
	{
	}

	virtual bool Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials) override;

	virtual bool Logout(int32 LocalUserNum) override;

	virtual bool AutoLogin(int32 LocalUserNum) override;

	virtual TSharedPtr<FUserOnlineAccount> GetUserAccount(const FUniqueNetId& UserId) const override;

	virtual TArray<TSharedPtr<FUserOnlineAccount>> GetAllUserAccounts() const override;

	virtual TSharedPtr<const FUniqueNetId> GetUniquePlayerId(int32 LocalUserNum) const override;

	virtual TSharedPtr<const FUniqueNetId> CreateUniquePlayerId(uint8* Bytes, int32 Size) override;

	virtual TSharedPtr<const FUniqueNetId> CreateUniquePlayerId(const FString& Str) override;

	virtual ELoginStatus::Type GetLoginStatus(int32 LocalUserNum) const override;

	virtual ELoginStatus::Type GetLoginStatus(const FUniqueNetId& UserId) const override;

	virtual FString GetPlayerNickname(int32 LocalUserNum) const override;

	virtual FString GetPlayerNickname(const FUniqueNetId& UserId) const override;

	virtual FString GetAuthToken(int32 LocalUserNum) const override;

	virtual void RevokeAuthToken(const FUniqueNetId& LocalUserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate) override;

	virtual void GetUserPrivilege(const FUniqueNetId& LocalUserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate, EShowPrivilegeResolveUI ShowResolveUI = EShowPrivilegeResolveUI::Default) override;

	virtual FPlatformUserId GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) const override;

	virtual FString GetAuthType() const override;

	int32 GetLocalUserNumFromUniqueNetId(const FUniqueNetId& UniqueNetId) const;
};
