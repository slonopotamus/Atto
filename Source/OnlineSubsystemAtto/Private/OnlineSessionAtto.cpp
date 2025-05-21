#include "OnlineSessionAtto.h"

#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemAtto.h"

FNamedOnlineSession* FOnlineSessionAtto::AddNamedSession(const FName SessionName, const FOnlineSessionSettings& SessionSettings)
{
	return &Sessions.Add(SessionName, FNamedOnlineSession{SessionName, SessionSettings});
}

FNamedOnlineSession* FOnlineSessionAtto::AddNamedSession(const FName SessionName, const FOnlineSession& Session)
{
	return &Sessions.Add(SessionName, FNamedOnlineSession{SessionName, Session});
}

void FOnlineSessionAtto::Tick(float DeltaSeconds)
{
	// TODO: Atto
}

TSharedPtr<const FUniqueNetId> FOnlineSessionAtto::CreateSessionIdFromString(const FString& SessionIdStr)
{
	// TODO: Atto
	return nullptr;
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

bool FOnlineSessionAtto::CreateSession(int32 HostingPlayerNum, const FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	// TODO: Atto
	TriggerOnCreateSessionCompleteDelegates(SessionName, false);
	return false;
}

bool FOnlineSessionAtto::CreateSession(const FUniqueNetId& HostingPlayerId, const FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	// TODO: Atto
	TriggerOnCreateSessionCompleteDelegates(SessionName, false);
	return false;
}

bool FOnlineSessionAtto::StartSession(const FName SessionName)
{
	bool bSuccess = false;
	if (auto* Session = Sessions.Find(SessionName))
	{
		if (Session->SessionState == EOnlineSessionState::Pending || Session->SessionState == EOnlineSessionState::Ended)
		{
			Session->SessionState = EOnlineSessionState::InProgress;
			bSuccess = true;
		}
	}

	TriggerOnStartSessionCompleteDelegates(SessionName, bSuccess);
	return bSuccess;
}

bool FOnlineSessionAtto::UpdateSession(const FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData)
{
	bool bSuccess = false;

	if (auto* Session = GetNamedSession(SessionName))
	{
		Session->SessionSettings = UpdatedSessionSettings;
		bSuccess = true;
	}

	TriggerOnUpdateSessionCompleteDelegates(SessionName, bSuccess);
	return bSuccess;
}

bool FOnlineSessionAtto::EndSession(const FName SessionName)
{
	bool bSuccess = false;

	if (auto* Session = Sessions.Find(SessionName); Session && Session->SessionState == EOnlineSessionState::InProgress)
	{
		Session->SessionState = EOnlineSessionState::Ended;
		bSuccess = true;
	}

	TriggerOnEndSessionCompleteDelegates(SessionName, bSuccess);
	return bSuccess;
}

bool FOnlineSessionAtto::DestroySession(const FName SessionName, const FOnDestroySessionCompleteDelegate& CompletionDelegate)
{
	const bool bSuccess = Sessions.Remove(SessionName) > 0;

	CompletionDelegate.ExecuteIfBound(SessionName, bSuccess);
	TriggerOnDestroySessionCompleteDelegates(SessionName, bSuccess);
	return bSuccess;
}

bool FOnlineSessionAtto::IsPlayerInSession(const FName SessionName, const FUniqueNetId& UniqueId)
{
	if (const auto* Session = GetNamedSession(SessionName); Session && ensure(UniqueId.IsValid()))
	{
		return Session->OwningUserId == UniqueId.AsShared() || Session->RegisteredPlayers.Contains(UniqueId.AsShared());
	}

	return false;
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
	if (const auto& UserId = Subsystem.GetIdentityInterface()->GetUniquePlayerId(SearchingPlayerNum))
	{
		return FindSessions(*UserId, SearchSettings);
	}

	TriggerOnFindSessionsCompleteDelegates(false);
	return false;
}

bool FOnlineSessionAtto::FindSessions(const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	// TODO: Atto
	TriggerOnFindSessionsCompleteDelegates(false);
	return false;
}

bool FOnlineSessionAtto::FindSessionById(const FUniqueNetId& SearchingUserId, const FUniqueNetId& SessionId, const FUniqueNetId& FriendId, const FOnSingleSessionResultCompleteDelegate& CompletionDelegate)
{
	static const FOnlineSessionSearchResult EmptyResult;
	CompletionDelegate.ExecuteIfBound(0, false, EmptyResult);
	return false;
}

bool FOnlineSessionAtto::CancelFindSessions()
{
	// TODO: Atto
	TriggerOnCancelFindSessionsCompleteDelegates(false);
	return false;
}

bool FOnlineSessionAtto::PingSearchResults(const FOnlineSessionSearchResult& SearchResult)
{
	return false;
}

bool FOnlineSessionAtto::JoinSession(const int32 LocalUserNum, const FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	if (const auto& UserId = Subsystem.GetIdentityInterface()->GetUniquePlayerId(LocalUserNum))
	{
		return JoinSession(*UserId, SessionName, DesiredSession);
	}

	TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::SessionDoesNotExist);
	return false;
}

bool FOnlineSessionAtto::JoinSession(const FUniqueNetId& LocalUserId, const FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	// TODO: Atto
	TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::SessionDoesNotExist);
	return false;
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

bool FOnlineSessionAtto::GetResolvedConnectString(const FName SessionName, FString& ConnectInfo, FName PortType)
{
	// TODO: Atto
	return false;
}

bool FOnlineSessionAtto::GetResolvedConnectString(const FOnlineSessionSearchResult& SearchResult, FName PortType, FString& ConnectInfo)
{
	// TODO: Atto
	return false;
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

	for (const auto& Player : Players)
	{
		Session->RegisteredPlayers.AddUnique(Player);
	}

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
		TriggerOnRegisterPlayersCompleteDelegates(SessionName, Players, false);
		return false;
	}

	int32 NumRemoved = 0;
	for (const auto& Player : Players)
	{
		NumRemoved += Session->RegisteredPlayers.Remove(Player);
	}

	Session->NumOpenPublicConnections = FMath::Clamp(Session->NumOpenPublicConnections + NumRemoved, 0, Session->SessionSettings.NumPublicConnections);
	TriggerOnRegisterPlayersCompleteDelegates(SessionName, Players, true);

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
