#include "OnlineSessionAtto.h"
#include "AttoClient.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineIdentityAtto.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemAtto.h"
#include "OnlineSubsystemUtils.h"
#include "SocketSubsystem.h"
#include "UniqueNetIdAtto.h"

class FOnlineSessionInfoAtto final : public FOnlineSessionInfo
{
public:
	const TSharedRef<const FUniqueNetIdAtto> SessionId;
	const TSharedRef<const FInternetAddr> HostAddress;

	explicit FOnlineSessionInfoAtto(const TSharedRef<const FUniqueNetIdAtto>& SessionId, const TSharedRef<FInternetAddr>& HostAddress)
	    : SessionId(SessionId)
	    , HostAddress(HostAddress)
	{}

	virtual const uint8* GetBytes() const override
	{
		ensure(false);
		return nullptr;
	}

	virtual int32 GetSize() const override
	{
		ensure(false);
		return 0;
	}
	virtual bool IsValid() const override
	{
		return true;
	}

	virtual FString ToString() const override
	{
		return SessionId->ToString();
	}

	virtual FString ToDebugString() const override
	{
		return FString::Printf(
		    TEXT("HostAddress: %s SessionId: %s"),
		    *HostAddress->ToString(true),
		    *SessionId->ToDebugString());
	}

	virtual const FUniqueNetId& GetSessionId() const override
	{
		return *SessionId;
	}
};

FNamedOnlineSession* FOnlineSessionAtto::AddNamedSession(const FName SessionName, const FOnlineSessionSettings& SessionSettings)
{
	return &Sessions.Add(SessionName, FNamedOnlineSession{SessionName, SessionSettings});
}

FNamedOnlineSession* FOnlineSessionAtto::AddNamedSession(const FName SessionName, const FOnlineSession& Session)
{
	return &Sessions.Add(SessionName, FNamedOnlineSession{SessionName, Session});
}

TSharedPtr<const FUniqueNetId> FOnlineSessionAtto::CreateSessionIdFromString(const FString& SessionIdStr)
{
	// TODO: Use separate types for user ids and session ids?
	return FUniqueNetIdAtto::Create(SessionIdStr);
}

FNamedOnlineSession* FOnlineSessionAtto::GetNamedSession(const FName SessionName)
{
	return Sessions.Find(SessionName);
}

void FOnlineSessionAtto::RemoveNamedSession(const FName SessionName)
{
	Sessions.Remove(SessionName);
}

bool FOnlineSessionAtto::HasPresenceSession()
{
	for (const auto& [_, Session] : Sessions)
	{
		if (Session.SessionSettings.bUsesPresence)
		{
			return true;
		}
	}

	return false;
}

EOnlineSessionState::Type FOnlineSessionAtto::GetSessionState(const FName SessionName) const
{
	if (const auto* Session = Sessions.Find(SessionName))
	{
		return Session->SessionState;
	}

	return EOnlineSessionState::NoSession;
}

bool FOnlineSessionAtto::CreateSession(const int32 HostingPlayerNum, const FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	if (GetNamedSession(SessionName))
	{
		UE_LOG_ONLINE_SESSION(Warning, TEXT("Cannot create session '%s': session already exists"), *SessionName.ToString());
	}
	else if (const auto OwningUserId = StaticCastSharedPtr<const FUniqueNetIdAtto>(Subsystem.IdentityInterface->GetUniquePlayerId(HostingPlayerNum)))
	{
		const FGuid Guid = FGuid::NewGuid();
		// TODO: Who is responsible for generating session ids?
		const auto SessionId = FUniqueNetIdAtto::Create(CityHash64(reinterpret_cast<const char*>(&Guid), sizeof(Guid)));

		// TODO: Maybe this should better be passed via session settings instead of command-line args?
		const auto& HostAddress = DetermineSessionPublicAddress();
		HostAddress->SetPort(DetermineSessionPublicPort());

		const auto Session = AddNamedSession(SessionName, NewSessionSettings);
		Session->bHosting = true;
		Session->HostingPlayerNum = HostingPlayerNum;
		Session->NumOpenPrivateConnections = NewSessionSettings.NumPrivateConnections;
		Session->NumOpenPublicConnections = NewSessionSettings.NumPublicConnections;
		Session->OwningUserId = OwningUserId;
		Session->OwningUserName = Subsystem.IdentityInterface->GetPlayerNickname(HostingPlayerNum);
		Session->SessionInfo = MakeShared<FOnlineSessionInfoAtto>(SessionId, HostAddress);
		Session->SessionSettings = NewSessionSettings;
		Session->SessionState = EOnlineSessionState::Pending;

		Subsystem.AttoClient
		    ->Send<FAttoCreateSessionRequest>(FAttoSessionInfoEx{
		        .OwningUserId = OwningUserId->Value,
		        .SessionInfo = FAttoSessionInfo{
		            .SessionId = SessionId->Value,
		            .HostAddress = HostAddress->GetRawIp(),
		            .Port = HostAddress->GetPort(),
		            .BuildUniqueId = Session->SessionSettings.BuildUniqueId,
		            .UpdatableInfo = FAttoSessionUpdatableInfo{*Session},
		            .bIsDedicated = Session->SessionSettings.bIsDedicated,
		            .bAntiCheatProtected = Session->SessionSettings.bAntiCheatProtected,
		        }})
		    .Next([=, this](auto&& Response) {
			    TriggerOnCreateSessionCompleteDelegates(SessionName, Response.IsOk() && Response.GetOkValue().bSuccess);
		    });

		return true;
	}
	else
	{
		UE_LOG_ONLINE_SESSION(Warning, TEXT("Cannot create session '%s': no logged in user found for LocalUserNum=%d"), *SessionName.ToString(), HostingPlayerNum);
	}

	TriggerOnCreateSessionCompleteDelegates(SessionName, false);
	return false;
}

bool FOnlineSessionAtto::CreateSession(const FUniqueNetId& HostingPlayerId, const FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	const auto LocalUserNum = Subsystem.IdentityInterface->GetLocalUserNumFromUniqueNetId(HostingPlayerId);
	return CreateSession(LocalUserNum, SessionName, NewSessionSettings);
}

bool FOnlineSessionAtto::StartSession(const FName SessionName)
{
	bool bSuccess = false;

	if (auto* Session = Sessions.Find(SessionName))
	{
		if (Session->SessionState == EOnlineSessionState::Pending || Session->SessionState == EOnlineSessionState::Ended)
		{
			Session->SessionState = EOnlineSessionState::InProgress;
			// TODO: Wait until async call completes
			UpdateSession(SessionName, Session->SessionSettings);
			bSuccess = true;
		}
		else
		{
			UE_LOG_ONLINE_SESSION(Warning, TEXT("Can't start an online session (%s) in state %s"), *SessionName.ToString(), EOnlineSessionState::ToString(Session->SessionState));
		}
	}
	else
	{
		UE_LOG_ONLINE_SESSION(Warning, TEXT("Can't start an online game for session (%s) that hasn't been created"), *SessionName.ToString());
	}

	TriggerOnStartSessionCompleteDelegates(SessionName, bSuccess);
	return bSuccess;
}

bool FOnlineSessionAtto::EndSession(const FName SessionName)
{
	bool bSuccess = false;

	if (auto* Session = Sessions.Find(SessionName))
	{
		if (Session->SessionState == EOnlineSessionState::InProgress)
		{
			Session->SessionState = EOnlineSessionState::Ended;
			// TODO: Wait until async call completes
			UpdateSession(SessionName, Session->SessionSettings);
			bSuccess = true;
		}
		else
		{
			UE_LOG_ONLINE_SESSION(Warning, TEXT("Can't end session (%s) in state %s"), *SessionName.ToString(), EOnlineSessionState::ToString(Session->SessionState));
		}
	}
	else
	{
		UE_LOG_ONLINE_SESSION(Warning, TEXT("Can't end an online game for session (%s) that hasn't been created"), *SessionName.ToString());
	}

	TriggerOnEndSessionCompleteDelegates(SessionName, bSuccess);
	return bSuccess;
}

bool FOnlineSessionAtto::DestroySession(const FName SessionName, const FOnDestroySessionCompleteDelegate& CompletionDelegate)
{
	const auto CallDelegates = [=, this](const bool bSuccess) {
		CompletionDelegate.ExecuteIfBound(SessionName, bSuccess);
		TriggerOnDestroySessionCompleteDelegates(SessionName, bSuccess);
	};

	if (const auto* Session = Sessions.Find(SessionName))
	{
		Subsystem.AttoClient
		    ->Send<FAttoDestroySessionRequest>(StaticCastSharedPtr<const FUniqueNetIdAtto>(Session->OwningUserId)->Value)
		    .Next([=, this](auto&& Response) {
			    const bool bSuccess = Response.IsOk() && Response.GetOkValue().bSuccess;
			    CallDelegates(bSuccess);
		    });

		Sessions.Remove(SessionName);
		return true;
	}

	UE_LOG_ONLINE_SESSION(Warning, TEXT("Can't destroy a null online session (%s)"), *SessionName.ToString());

	CallDelegates(false);
	return false;
}

bool FOnlineSessionAtto::UpdateSession(const FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, const bool bShouldRefreshOnlineData)
{
	if (auto* Session = GetNamedSession(SessionName))
	{
		// TODO: We're updating here what's not actually updatable via FAttoSessionUpdatableInfo...
		Session->SessionSettings = UpdatedSessionSettings;

		if (bShouldRefreshOnlineData)
		{
			if (const auto& SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoAtto>(Session->SessionInfo); ensure(SessionInfo))
			{
				Subsystem.AttoClient
				    ->Send<FAttoUpdateSessionRequest>(StaticCastSharedPtr<const FUniqueNetIdAtto>(Session->OwningUserId)->Value, SessionInfo->SessionId->Value, FAttoSessionUpdatableInfo{*Session})
				    .Next([=, this](auto&& Response) {
					    TriggerOnUpdateSessionCompleteDelegates(SessionName, Response.IsOk() && Response.GetOkValue().bSuccess);
				    });
				return true;
			}
		}
	}

	TriggerOnUpdateSessionCompleteDelegates(SessionName, false);
	return false;
}

bool FOnlineSessionAtto::IsPlayerInSession(const FName SessionName, const FUniqueNetId& UniqueId)
{
	return IsPlayerInSessionImpl(this, SessionName, UniqueId);
}

bool FOnlineSessionAtto::StartMatchmaking(const TArray<TSharedRef<const FUniqueNetId>>& LocalPlayers, const FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	TriggerOnMatchmakingCompleteDelegates(SessionName, false);
	return false;
}

bool FOnlineSessionAtto::CancelMatchmaking(int32 SearchingPlayerNum, const FName SessionName)
{
	TriggerOnCancelMatchmakingCompleteDelegates(SessionName, false);
	return false;
}

bool FOnlineSessionAtto::CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, const FName SessionName)
{
	TriggerOnCancelMatchmakingCompleteDelegates(SessionName, false);
	return false;
}

bool FOnlineSessionAtto::FindSessions(const int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	if (const auto& UserId = Subsystem.IdentityInterface->GetUniquePlayerId(SearchingPlayerNum))
	{
		return FindSessions(*UserId, SearchSettings);
	}

	TriggerOnFindSessionsCompleteDelegates(false);
	return false;
}

bool FOnlineSessionAtto::FindSessions(const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	if (CurrentSessionSearch.IsSet())
	{
		UE_LOG_ONLINE_SESSION(Warning, TEXT("Ignoring game search request while one is pending"));

		TriggerOnFindSessionsCompleteDelegates(false);
		return false;
	}

	SearchSettings->PlatformHash = FMath::Rand32();

	CurrentSessionSearch = {
	    .SearchSettings = SearchSettings,
	};

	Subsystem.AttoClient->FindSessionsAsync(*SearchSettings)
	    .Next([=, this](auto&& Result) {
		    if (!Result.IsOk())
		    {
			    // TODO: Log error?
			    CurrentSessionSearch.Reset();
			    TriggerOnFindSessionsCompleteDelegates(false);
			    return;
		    }

		    const auto& Message = Result.GetOkValue();
		    if (const auto* Search = CurrentSessionSearch.GetPtrOrNull())
		    {
			    if (const auto& CurrentSearchId = Search->SearchSettings->PlatformHash; CurrentSearchId != Message.RequestId)
			    {
				    UE_LOG_ONLINE_SESSION(Warning, TEXT("Ignoring game search response %d because current one is %d"), Message.RequestId, CurrentSearchId);
				    return;
			    }

			    const auto BuildUniqueId = GetBuildUniqueId();

			    for (const auto& Session : Result.GetOkValue().Sessions)
			    {
				    if (Session.SessionInfo.BuildUniqueId != BuildUniqueId)
				    {
					    continue;
				    }

				    const auto SessionAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
				    SessionAddress->SetRawIp(Session.SessionInfo.HostAddress);
				    SessionAddress->SetPort(Session.SessionInfo.Port);

				    // TODO: Use separate types for user ids and session ids?
				    const auto SessionId = FUniqueNetIdAtto::Create(Session.SessionInfo.SessionId);

				    auto& ResultSession = Search->SearchSettings->SearchResults.Emplace_GetRef();
				    ResultSession.Session.OwningUserId = FUniqueNetIdAtto::Create(Session.OwningUserId);
				    ResultSession.Session.SessionInfo = MakeShared<FOnlineSessionInfoAtto>(SessionId, SessionAddress);
				    ResultSession.Session.SessionSettings.BuildUniqueId = Session.SessionInfo.BuildUniqueId;
				    Session.SessionInfo.UpdatableInfo.CopyTo(ResultSession.Session, nullptr);
				    // TODO: Fill other fields
			    }

			    Search->SearchSettings->SortSearchResults();

			    CurrentSessionSearch.Reset();
			    TriggerOnFindSessionsCompleteDelegates(true);
		    }
	    });

	return true;
}

TSharedRef<FInternetAddr> FOnlineSessionAtto::DetermineSessionPublicAddress()
{
	if (FString PublicAddress; FParse::Value(FCommandLine::Get(), TEXT("AttoPublicAddress="), PublicAddress))
	{
		if (const auto& Result = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetAddressFromString(PublicAddress))
		{
			return Result.ToSharedRef();
		}

		UE_LOG_ONLINE_SESSION(Warning, TEXT("Failed to parse -AttoPublicAddress=... as FInternetAddress: %s"), *PublicAddress);
	}

	bool bCanBindAll;
	const auto HostAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBindAll);

	// The below is a workaround for systems that set hostname to a distinct address from 127.0.0.1 on a loopback interface.
	// See e.g. https://www.debian.org/doc/manuals/debian-reference/ch05.en.html#_the_hostname_resolution
	// and http://serverfault.com/questions/363095/why-does-my-hostname-appear-with-the-address-127-0-1-1-rather-than-127-0-0-1-in
	// Since we bind to 0.0.0.0, we won't answer on 127.0.1.1, so we need to advertise ourselves as 127.0.0.1 for any other loopback address we may have.
	uint32 HostIp = 0;
	HostAddress->GetIp(HostIp); // will return in host order

	// if this address is on loopback interface, advertise it as 127.0.0.1
	if ((HostIp & 0xff000000) == 0x7f000000)
	{
		HostAddress->SetIp(0x7f000001); // 127.0.0.1
	}

	return HostAddress;
}

int32 FOnlineSessionAtto::DetermineSessionPublicPort() const
{
	if (int HostPort = 0; FParse::Value(FCommandLine::Get(), TEXT("AttoListenPort="), HostPort))
	{
		return HostPort;
	}

	return GetPortFromNetDriver(Subsystem.GetInstanceName());
}

bool FOnlineSessionAtto::FindSessionById(const FUniqueNetId& SearchingUserId, const FUniqueNetId& SessionId, const FUniqueNetId& FriendId, const FOnSingleSessionResultCompleteDelegate& CompletionDelegate)
{
	// TODO: Implement this
	static const FOnlineSessionSearchResult EmptyResult;
	CompletionDelegate.ExecuteIfBound(0, false, EmptyResult);
	return false;
}

bool FOnlineSessionAtto::CancelFindSessions()
{
	bool bSuccess = false;

	if (CurrentSessionSearch.GetPtrOrNull())
	{
		CurrentSessionSearch.Reset();
		bSuccess = true;
	}
	else
	{
		UE_LOG_ONLINE_SESSION(Warning, TEXT("Can't cancel a search that isn't in progress"));
	}

	TriggerOnCancelFindSessionsCompleteDelegates(bSuccess);
	return bSuccess;
}

bool FOnlineSessionAtto::PingSearchResults(const FOnlineSessionSearchResult& SearchResult)
{
	return false;
}

bool FOnlineSessionAtto::JoinSession(const int32 LocalUserNum, const FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	if (LocalUserNum < 0 || LocalUserNum >= MAX_LOCAL_PLAYERS)
	{
		UE_LOG_ONLINE_SESSION(Warning, TEXT("Cannot join session (%s), invalid LocalUserNum=%d"), *SessionName.ToString(), LocalUserNum);
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::UnknownError);
		return false;
	}

	if (GetNamedSession(SessionName))
	{
		UE_LOG_ONLINE_SESSION(Warning, TEXT("Session (%s) already exists, can't join twice"), *SessionName.ToString());
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::AlreadyInSession);
		return false;
	}

	if (!DesiredSession.Session.SessionInfo.IsValid())
	{
		UE_LOG_ONLINE_SESSION(Warning, TEXT("Cannot join session(%s): session info is invalid"), *SessionName.ToString());
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::CouldNotRetrieveAddress);
		return false;
	}

	auto* Session = AddNamedSession(SessionName, DesiredSession.Session);
	Session->HostingPlayerNum = LocalUserNum;
	Session->SessionInfo = DesiredSession.Session.SessionInfo;
	Session->SessionSettings.bShouldAdvertise = false;

	TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::Success);
	return true;
}

bool FOnlineSessionAtto::JoinSession(const FUniqueNetId& LocalUserId, const FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	const auto LocalUserNum = Subsystem.IdentityInterface->GetLocalUserNumFromUniqueNetId(LocalUserId);
	return JoinSession(LocalUserNum, SessionName, DesiredSession);
}

bool FOnlineSessionAtto::FindFriendSession(const int32 LocalUserNum, const FUniqueNetId& Friend)
{
	static const TArray<FOnlineSessionSearchResult> EmptySearchResult;
	TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, false, EmptySearchResult);
	return false;
}

bool FOnlineSessionAtto::FindFriendSession(const FUniqueNetId& LocalUserId, const FUniqueNetId& Friend)
{
	return FindFriendSession(INDEX_NONE, Friend);
}

bool FOnlineSessionAtto::FindFriendSession(const FUniqueNetId& LocalUserId, const TArray<TSharedRef<const FUniqueNetId>>& FriendList)
{
	static const TArray<FOnlineSessionSearchResult> EmptySearchResult;
	TriggerOnFindFriendSessionCompleteDelegates(INDEX_NONE, false, EmptySearchResult);
	return false;
}

bool FOnlineSessionAtto::SendSessionInviteToFriend(int32 LocalUserNum, FName SessionName, const FUniqueNetId& Friend)
{
	return false;
}

bool FOnlineSessionAtto::SendSessionInviteToFriend(const FUniqueNetId& LocalUserId, FName SessionName, const FUniqueNetId& Friend)
{
	return false;
}

bool FOnlineSessionAtto::SendSessionInviteToFriends(int32 LocalUserNum, FName SessionName, const TArray<TSharedRef<const FUniqueNetId>>& Friends)
{
	return false;
}

bool FOnlineSessionAtto::SendSessionInviteToFriends(const FUniqueNetId& LocalUserId, FName SessionName, const TArray<TSharedRef<const FUniqueNetId>>& Friends)
{
	return false;
}

bool FOnlineSessionAtto::GetResolvedConnectString(const FName SessionName, FString& ConnectInfo, const FName PortType)
{
	if (!ensure(PortType == NAME_GamePort))
	{
		return false;
	}

	const auto* Session = GetNamedSession(SessionName);
	if (!Session)
	{
		UE_LOG_ONLINE_SESSION(Warning, TEXT("Unknown session name (%s) specified to GetResolvedConnectString()"), *SessionName.ToString());
		return false;
	}

	const auto& SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoAtto>(Session->SessionInfo);
	if (!SessionInfo)
	{
		UE_LOG_ONLINE_SESSION(Warning, TEXT("Invalid session info for session %s in GetResolvedConnectString()"), *SessionName.ToString());
		return false;
	}

	ConnectInfo = SessionInfo->HostAddress->ToString(true);
	return true;
}

bool FOnlineSessionAtto::GetResolvedConnectString(const FOnlineSessionSearchResult& SearchResult, const FName PortType, FString& ConnectInfo)
{
	if (!ensure(PortType == NAME_GamePort))
	{
		return false;
	}

	const auto& SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoAtto>(SearchResult.Session.SessionInfo);
	if (!SessionInfo)
	{
		UE_LOG_ONLINE_SESSION(Warning, TEXT("Invalid session info in search result to GetResolvedConnectString()"));
		return false;
	}

	ConnectInfo = SessionInfo->HostAddress->ToString(true);
	return true;
}

FOnlineSessionSettings* FOnlineSessionAtto::GetSessionSettings(const FName SessionName)
{
	if (auto* Session = GetNamedSession(SessionName))
	{
		return &Session->SessionSettings;
	}

	return nullptr;
}

bool FOnlineSessionAtto::RegisterPlayer(const FName SessionName, const FUniqueNetId& PlayerId, const bool bWasInvited)
{
	return RegisterPlayers(SessionName, {PlayerId.AsShared()}, bWasInvited);
}

bool FOnlineSessionAtto::RegisterPlayers(const FName SessionName, const TArray<TSharedRef<const FUniqueNetId>>& Players, bool bWasInvited)
{
	auto* Session = GetNamedSession(SessionName);
	if (!Session)
	{
		TriggerOnRegisterPlayersCompleteDelegates(SessionName, Players, false);
		return false;
	}

	const auto NumBefore = Session->RegisteredPlayers.Num();
	for (const auto& Player : Players)
	{
		Session->RegisteredPlayers.AddUnique(Player);
	}
	const auto NumAdded = Session->RegisteredPlayers.Num() - NumBefore;

	Session->NumOpenPublicConnections = Session->NumOpenPublicConnections - NumAdded;
	UpdateSession(SessionName, Session->SessionSettings);

	TriggerOnRegisterPlayersCompleteDelegates(SessionName, Players, true);
	return true;
}

bool FOnlineSessionAtto::UnregisterPlayer(const FName SessionName, const FUniqueNetId& PlayerId)
{
	return UnregisterPlayers(SessionName, {PlayerId.AsShared()});
}

bool FOnlineSessionAtto::UnregisterPlayers(const FName SessionName, const TArray<TSharedRef<const FUniqueNetId>>& Players)
{
	auto* Session = GetNamedSession(SessionName);
	if (!Session)
	{
		TriggerOnUnregisterPlayersCompleteDelegates(SessionName, Players, false);
		return false;
	}

	int32 NumRemoved = 0;
	for (const auto& Player : Players)
	{
		NumRemoved += Session->RegisteredPlayers.RemoveSwap(Player);
	}

	Session->NumOpenPublicConnections = Session->NumOpenPublicConnections + NumRemoved;
	UpdateSession(SessionName, Session->SessionSettings);

	TriggerOnUnregisterPlayersCompleteDelegates(SessionName, Players, true);
	return true;
}

void FOnlineSessionAtto::RegisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnRegisterLocalPlayerCompleteDelegate& Delegate)
{
	Delegate.ExecuteIfBound(PlayerId, EOnJoinSessionCompleteResult::SessionDoesNotExist);
}

void FOnlineSessionAtto::UnregisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnUnregisterLocalPlayerCompleteDelegate& Delegate)
{
	Delegate.ExecuteIfBound(PlayerId, false);
}

int32 FOnlineSessionAtto::GetNumSessions()
{
	return Sessions.Num();
}

void FOnlineSessionAtto::DumpSessionState()
{
	for (const auto& [_, Session] : Sessions)
	{
		DumpNamedSession(&Session);
	}
}
