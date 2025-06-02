#include "OnlineIdentityAtto.h"

#include "AttoClient.h"
#include "AttoCommon.h"
#include "OnlineAsyncTaskManagerAtto.h"
#include "OnlineError.h"
#include "OnlineSubsystemAtto.h"
#include "UniqueNetIdAtto.h"
#include "UserOnlineAccountAtto.h"

bool FOnlineIdentityAtto::Login(const int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	if (LocalUserNum < 0)
	{
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, FUniqueNetIdAtto::Invalid, TEXT(""));
		return false;
	}

	if (const auto* UserId = LocalUsers.Find(LocalUserNum))
	{
		TriggerOnLoginCompleteDelegates(LocalUserNum, true, **UserId, TEXT(""));
		return false;
	}

	if (!Subsystem.AttoClient)
	{
		Subsystem.AttoClient = MakeShared<FAttoClient>(FString::Printf(TEXT("ws://localhost:%d"), Atto::DefaultPort));
		Subsystem.AttoClient->OnConnectionError.AddLambda([](const FString& Error)
		                                                  { UE_LOG(LogAtto, Error, TEXT("%s"), *Error); });
	}

	Subsystem.TaskManager->AddGenericToInQueue([this, LocalUserNum]
	                                           {
		TSharedPtr<FDelegateHandle> DelegateHandle = MakeShared<FDelegateHandle>();
		auto OnConnected = [this, LocalUserNum, DelegateHandle]
		{
			// TODO: Fill user id
			const auto UserId = FUniqueNetIdAtto::Create(1);
			Accounts.Add(UserId, MakeShared<FUserOnlineAccountAtto>(UserId));
			TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UserId, TEXT(""));
			Subsystem.AttoClient->OnConnected.Remove(*DelegateHandle);
		};
		
		if (Subsystem.AttoClient->IsConnected())
		{
			OnConnected();
		}
		else
		{
			DelegateHandle = MakeShared<FDelegateHandle>(Subsystem.AttoClient->OnConnected.AddLambda(OnConnected));
			Subsystem.AttoClient->Connect();
		} });

	return true;
}

bool FOnlineIdentityAtto::Logout(const int32 LocalUserNum)
{
	const auto* UserId = LocalUsers.Find(LocalUserNum);
	if (!UserId)
	{
		TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
		return false;
	}

	Accounts.Remove(*UserId);
	LocalUsers.Remove(LocalUserNum);
	TriggerOnLogoutCompleteDelegates(LocalUserNum, true);

	return true;
}

bool FOnlineIdentityAtto::AutoLogin(const int32 LocalUserNum)
{
	return Login(LocalUserNum, FOnlineAccountCredentials());
}

TSharedPtr<FUserOnlineAccount> FOnlineIdentityAtto::GetUserAccount(const FUniqueNetId& UserId) const
{
	if (const auto* Result = Accounts.Find(UserId.AsShared()))
	{
		return *Result;
	}

	return nullptr;
}

TArray<TSharedPtr<FUserOnlineAccount>> FOnlineIdentityAtto::GetAllUserAccounts() const
{
	TArray<TSharedPtr<FUserOnlineAccount>> Result;

	for (const auto& [UserId, Account] : Accounts)
	{
		Result.Add(Account);
	}

	return Result;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityAtto::GetUniquePlayerId(const int32 LocalUserNum) const
{
	if (auto* Result = LocalUsers.Find(LocalUserNum))
	{
		return *Result;
	}

	return nullptr;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityAtto::CreateUniquePlayerId(uint8* Bytes, const int32 Size)
{
	return FUniqueNetIdAtto::Create(Bytes, Size);
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityAtto::CreateUniquePlayerId(const FString& Str)
{
	return FUniqueNetIdAtto::Create(Str);
}

ELoginStatus::Type FOnlineIdentityAtto::GetLoginStatus(const int32 LocalUserNum) const
{
	return GetUniquePlayerId(LocalUserNum) ? ELoginStatus::LoggedIn : ELoginStatus::NotLoggedIn;
}

ELoginStatus::Type FOnlineIdentityAtto::GetLoginStatus(const FUniqueNetId& UserId) const
{
	return GetUserAccount(UserId) ? ELoginStatus::LoggedIn : ELoginStatus::NotLoggedIn;
}

FString FOnlineIdentityAtto::GetPlayerNickname(const int32 LocalUserNum) const
{
	if (const auto& UserId = GetUniquePlayerId(LocalUserNum))
	{
		return GetPlayerNickname(*UserId);
	}

	return TEXT("");
}

FString FOnlineIdentityAtto::GetPlayerNickname(const FUniqueNetId& UserId) const
{
	if (const auto* Account = Accounts.Find(UserId.AsShared()))
	{
		return (*Account)->GetDisplayName();
	}

	return TEXT("");
}

FString FOnlineIdentityAtto::GetAuthToken(const int32 LocalUserNum) const
{
	return TEXT("");
}

void FOnlineIdentityAtto::RevokeAuthToken(const FUniqueNetId& LocalUserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate)
{
	static const FOnlineError Error{false};
	Delegate.ExecuteIfBound(LocalUserId, Error);
}

void FOnlineIdentityAtto::GetUserPrivilege(const FUniqueNetId& LocalUserId, const EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate, const EShowPrivilegeResolveUI ShowResolveUI)
{
	Delegate.ExecuteIfBound(LocalUserId, Privilege, static_cast<std::underlying_type_t<EPrivilegeResults>>(EPrivilegeResults::NoFailures));
}

FPlatformUserId FOnlineIdentityAtto::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) const
{
	return PLATFORMUSERID_NONE;
}

FString FOnlineIdentityAtto::GetAuthType() const
{
	return TEXT("");
}
