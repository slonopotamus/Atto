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
	else
	{
		if (const auto OwningUserId = StaticCastSharedPtr<const FUniqueNetIdAtto>(Subsystem.IdentityInterface->GetUniquePlayerId(HostingPlayerNum)))
		{
			const FGuid Guid = FGuid::NewGuid();
			// TODO: Who is responsible for generating session ids?
			const auto SessionId = FUniqueNetIdAtto::Create(CityHash64(reinterpret_cast<const char*>(&Guid), sizeof(Guid)));

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

			HostAddress->SetPort(GetPortFromNetDriver(Subsystem.GetInstanceName()));

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

			// TODO: Wait until call completes?
			Subsystem.AttoClient->CreateSessionAsync(FAttoSessionInfo{
			    .HostAddress = HostAddress->GetRawIp(),
			    .Port = HostAddress->GetPort(),
			    .SessionId = SessionId->Value,
			    .BuildUniqueId = Session->SessionSettings.BuildUniqueId,
			    .NumPublicConnections = Session->SessionSettings.NumPublicConnections,
			    .NumOpenPublicConnections = Session->NumOpenPublicConnections,
			});

			TriggerOnCreateSessionCompleteDelegates(SessionName, true);
			return true;
		}

		UE_LOG_ONLINE_SESSION(Warning, TEXT("Cannot create session '%s': o logged in user found for LocalUserNum=%d"), *SessionName.ToString(), HostingPlayerNum);
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
			// TODO: Send update to AttoServer
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
			// TODO: Send update to AttoServer
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
	const bool bSuccess = Sessions.Remove(SessionName) > 0;

	if (bSuccess)
	{
		// TODO: Wait until call completes?
		Subsystem.AttoClient->DestroySessionAsync();
	}
	else
	{
		UE_LOG_ONLINE_SESSION(Warning, TEXT("Can't destroy a null online session (%s)"), *SessionName.ToString());
	}

	CompletionDelegate.ExecuteIfBound(SessionName, bSuccess);
	TriggerOnDestroySessionCompleteDelegates(SessionName, bSuccess);
	return bSuccess;
}

bool FOnlineSessionAtto::UpdateSession(const FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData)
{
	bool bSuccess = false;

	if (auto* Session = GetNamedSession(SessionName))
	{
		Session->SessionSettings = UpdatedSessionSettings;

		if (bShouldRefreshOnlineData)
		{
			// TODO: Send update to AttoServer
		}

		bSuccess = true;
	}
	else
	{
	}

	TriggerOnUpdateSessionCompleteDelegates(SessionName, bSuccess);
	return bSuccess;
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
	    // TODO: Also subscribe to disconnect?
	    .OnFindSessionsResponseHandle = Subsystem.AttoClient->OnFindSessionsResponse.AddRaw(this, &ThisClass::OnFindSessionsResponse),
	};

	Subsystem.AttoClient->FindSessionsAsync(*SearchSettings);
	return true;
}

void FOnlineSessionAtto::OnFindSessionsResponse(const FAttoFindSessionsResponse& Message)
{
	if (const auto* Search = CurrentSessionSearch.GetPtrOrNull())
	{
		if (const auto& CurrentSearchId = Search->SearchSettings->PlatformHash; CurrentSearchId != Message.RequestId)
		{
			UE_LOG_ONLINE_SESSION(Warning, TEXT("Ignoring game search response %d because current one is %d"), Message.RequestId, CurrentSearchId);
			return;
		}

		Subsystem.AttoClient->OnFindSessionsResponse.Remove(Search->OnFindSessionsResponseHandle);

		const auto BuildUniqueId = GetBuildUniqueId();

		for (const auto& Session : Message.Sessions)
		{
			if (Session.Info.BuildUniqueId != BuildUniqueId)
			{
				continue;
			}

			const auto SessionAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
			SessionAddress->SetRawIp(Session.Info.HostAddress);
			SessionAddress->SetPort(Session.Info.Port);

			// TODO: Use separate types for user ids and session ids?
			const auto SessionId = FUniqueNetIdAtto::Create(Session.Info.SessionId);

			auto& ResultSession = Search->SearchSettings->SearchResults.Emplace_GetRef();
			ResultSession.Session.OwningUserId = FUniqueNetIdAtto::Create(Session.OwningUserId);
			ResultSession.Session.SessionInfo = MakeShared<FOnlineSessionInfoAtto>(SessionId, SessionAddress);
			ResultSession.Session.SessionSettings.BuildUniqueId = Session.Info.BuildUniqueId;
			ResultSession.Session.SessionSettings.NumPublicConnections = Session.Info.NumPublicConnections;
			ResultSession.Session.NumOpenPublicConnections = Session.Info.NumOpenPublicConnections;
			// TODO: Fill other fields
		}

		Search->SearchSettings->SortSearchResults();

		CurrentSessionSearch.Reset();
	}

	TriggerOnFindSessionsCompleteDelegates(true);
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

	if (const auto* Search = CurrentSessionSearch.GetPtrOrNull())
	{
		Subsystem.AttoClient->OnFindSessionsResponse.Remove(Search->OnFindSessionsResponseHandle);
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

	Session->NumOpenPublicConnections = FMath::Clamp(Session->NumOpenPublicConnections - NumAdded, 0, Session->SessionSettings.NumPublicConnections);
	// TODO: Send update to AttoServer

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

	Session->NumOpenPublicConnections = FMath::Clamp(Session->NumOpenPublicConnections + NumRemoved, 0, Session->SessionSettings.NumPublicConnections);
	// TODO: Send update to AttoServer

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
