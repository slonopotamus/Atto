#include "OnlineIdentityAtto.h"
#include "AttoClient.h"
#include "OnlineError.h"
#include "OnlineSubsystemAtto.h"
#include "UniqueNetIdAtto.h"
#include "UserOnlineAccountAtto.h"

bool FOnlineIdentityAtto::AutoLogin(const int32 LocalUserNum)
{
	// TODO: Do not autologin if we are a non-PIE AttoServer, so we do not connect to ourselves

	// TODO: Fill credentials
	return Login(LocalUserNum, FOnlineAccountCredentials());
}

bool FOnlineIdentityAtto::Login(const int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	if (LocalUserNum < 0 || LocalUserNum > MAX_LOCAL_PLAYERS)
	{
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, FUniqueNetIdAtto::Invalid, FString::Printf(TEXT("Invalid LocalUserNum=%d"), LocalUserNum));
		return false;
	}

	if (const auto* UserId = LocalUsers.Find(LocalUserNum))
	{
		TriggerOnLoginCompleteDelegates(LocalUserNum, true, **UserId, FString::Printf(TEXT("Local user %d is already logged in"), LocalUserNum));
		return true;
	}

	Subsystem.AttoClient
	    ->ConnectAsync()
	    .Next([=, this](auto&& ConnectResult) {
		    if (!ConnectResult)
		    {
			    TriggerOnLoginCompleteDelegates(LocalUserNum, false, FUniqueNetIdAtto::Invalid, TEXT("Connect failed"));
			    return;
		    }

		    Subsystem.AttoClient
		        ->Send<FAttoLoginRequest>(AccountCredentials.Id, AccountCredentials.Token)
		        .Next([=, this](auto&& LoginResult) {
			        if (!LoginResult.IsOk())
			        {
				        TriggerOnLoginCompleteDelegates(LocalUserNum, false, FUniqueNetIdAtto::Invalid, TEXT(""));
				        return;
			        }

			        // TODO: Rewrite this using Visit
			        if (const auto* UserIdPtr = LoginResult.GetOkValue().template TryGet<uint64>())
			        {
				        const auto UserId = FUniqueNetIdAtto::Create(*UserIdPtr);
				        Accounts.Add(UserId, MakeShared<FUserOnlineAccountAtto>(UserId));
				        LocalUsers.Add(LocalUserNum, UserId);

				        UE_LOG_ONLINE(Log, TEXT("Successfully logged into Atto server, userId=%s"), *UserId->ToDebugString());
				        TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UserId, TEXT(""));
			        }
			        else
			        {
				        const auto& Error = LoginResult.GetOkValue().template Get<FString>();
				        UE_LOG_ONLINE(Warning, TEXT("Failed to login to Atto server: %s"), *Error);
				        TriggerOnLoginCompleteDelegates(LocalUserNum, false, FUniqueNetIdAtto::Invalid, Error);
			        }
		        });
	    });

	return true;
}

bool FOnlineIdentityAtto::Logout(const int32 LocalUserNum)
{
	const auto* UserIdPtr = LocalUsers.Find(LocalUserNum);
	if (!UserIdPtr)
	{
		UE_LOG_ONLINE_IDENTITY(Warning, TEXT("No logged in user found for LocalUserNum=%d"), LocalUserNum);
		TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
		return false;
	}

	const auto UserId = *UserIdPtr;

	ensure(Accounts.Remove(UserId));
	ensure(LocalUsers.Remove(LocalUserNum));

	Subsystem.AttoClient
	    ->Send<FAttoLogoutRequest>(UserId->Value)
	    .Next([=, this](auto&& Response) {
		    const auto bSuccess = Response.IsOk() && Response.GetOkValue().bSuccess;
		    TriggerOnLogoutCompleteDelegates(LocalUserNum, bSuccess);
	    });

	return true;
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
	Delegate.ExecuteIfBound(LocalUserId, FOnlineError{LocalUserId.IsValid()});
}

void FOnlineIdentityAtto::GetUserPrivilege(const FUniqueNetId& LocalUserId, const EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate, const EShowPrivilegeResolveUI ShowResolveUI)
{
	Delegate.ExecuteIfBound(LocalUserId, Privilege, static_cast<std::underlying_type_t<EPrivilegeResults>>(EPrivilegeResults::NoFailures));
}

FPlatformUserId FOnlineIdentityAtto::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) const
{
	const auto LocalUserNum = GetLocalUserNumFromUniqueNetId(UniqueNetId);
	return GetPlatformUserIdFromLocalUserNum(LocalUserNum);
}

FString FOnlineIdentityAtto::GetAuthType() const
{
	return TEXT("");
}

int32 FOnlineIdentityAtto::GetLocalUserNumFromUniqueNetId(const FUniqueNetId& UniqueNetId) const
{
	for (const auto& [LocalUserNum, LocalUserNetId] : LocalUsers)
	{
		if (*LocalUserNetId == UniqueNetId)
		{
			return LocalUserNum;
		}
	}

	return INDEX_NONE;
}
