#include "OnlineSessionAtto.h"
#include "AttoClient.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineIdentityAtto.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemAtto.h"
#include "OnlineSubsystemUtils.h"
#include "SocketSubsystem.h"
#include "UniqueNetIdAtto.h"

template<typename T>
static TOptional<FAttoSessionSettingValue> GetValueFromVariantData(const FVariantData& Data)
{
	T Result{};
	Data.GetValue(Result);
	return {FAttoSessionSettingValue{TInPlaceType<T>{}, MoveTemp(Result)}};
}

// FVariantData is just brain-damaged
static TOptional<FAttoSessionSettingValue> ConvertVariantData(const FVariantData& Data)
{
	switch (Data.GetType())
	{
		case EOnlineKeyValuePairDataType::Empty:
			// TODO: How to handle this?
			return {};
		case EOnlineKeyValuePairDataType::Int32:
			return GetValueFromVariantData<int32>(Data);
		case EOnlineKeyValuePairDataType::UInt32:
			return GetValueFromVariantData<uint32>(Data);
		case EOnlineKeyValuePairDataType::Int64:
			return GetValueFromVariantData<int64>(Data);
		case EOnlineKeyValuePairDataType::UInt64:
			return GetValueFromVariantData<uint64>(Data);
		case EOnlineKeyValuePairDataType::Double:
			return GetValueFromVariantData<double>(Data);
		case EOnlineKeyValuePairDataType::String:
			return GetValueFromVariantData<FString>(Data);
		case EOnlineKeyValuePairDataType::Float:
			return GetValueFromVariantData<float>(Data);
		case EOnlineKeyValuePairDataType::Blob:
			return GetValueFromVariantData<TArray<uint8>>(Data);
		case EOnlineKeyValuePairDataType::Bool:
			return GetValueFromVariantData<bool>(Data);
		case EOnlineKeyValuePairDataType::Json:
			// TODO: Add support
			ensureMsgf(false, TEXT("JSON is not supported"));
			return {};
		case EOnlineKeyValuePairDataType::MAX: [[fallthrough]];
		default:
			ensureMsgf(false, TEXT("Unknown EOnlineKeyValuePairDataType: %d"), Data.GetType());
			return {};
	}
}

static TMap<FString, FAttoFindSessionsParam> ConvertSearchParams(const FSearchParams& Params)
{
	TMap<FString, FAttoFindSessionsParam> Result;

	for (const auto& [Name, Value] : Params)
	{
		if (const auto& ParamValue = ConvertVariantData(Value.Data))
		{
			Result.Add(Name.ToString(), FAttoFindSessionsParam{Value.ComparisonOp, *ParamValue, Value.ID});
		}
	}

	return Result;
}

static TMap<FString, FAttoSessionSettingValue> ConvertSessionSettings(const FSessionSettings& SessionSettings)
{
	TMap<FString, FAttoSessionSettingValue> Result;

	for (const auto& [Name, Value] : SessionSettings)
	{
		if (const auto& ParamValue = ConvertVariantData(Value.Data))
		{
			Result.Add(Name.ToString(), *ParamValue);
		}
	}

	return Result;
}

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
		Session->HostingPlayerNum = HostingPlayerNum;
		Session->bHosting = true;
		Session->NumOpenPrivateConnections = NewSessionSettings.NumPrivateConnections;
		Session->NumOpenPublicConnections = NewSessionSettings.NumPublicConnections;
		Session->OwningUserId = OwningUserId;
		Session->OwningUserName = Subsystem.IdentityInterface->GetPlayerNickname(HostingPlayerNum);
		Session->SessionInfo = MakeShared<FOnlineSessionInfoAtto>(SessionId, HostAddress);
		Session->SessionSettings = NewSessionSettings;
		Session->SessionState = EOnlineSessionState::Pending;

		Subsystem.AttoClient
		    ->Send<FAttoCreateSessionRequest>(
		        OwningUserId->Value,
		        FAttoSessionInfo{
		            .SessionId = SessionId->Value,
		            .HostAddress = FAttoSessionAddress{HostAddress.Get()},
		            .BuildUniqueId = Session->SessionSettings.BuildUniqueId,
		            .Settings = ConvertSessionSettings(Session->SessionSettings.Settings),
		            .UpdatableInfo = FAttoSessionUpdatableInfo{*Session},
		            .bIsDedicated = Session->SessionSettings.bIsDedicated,
		            .bAntiCheatProtected = Session->SessionSettings.bAntiCheatProtected,
		        })
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
	if (CurrentSessionSearch.IsValid())
	{
		UE_LOG_ONLINE_SESSION(Warning, TEXT("Ignoring matchmaking request while one is pending"));
		return false;
	}

	if (LocalPlayers.IsEmpty())
	{
		TriggerOnMatchmakingCompleteDelegates(SessionName, false);
		return false;
	}

	const auto SearchingPlayerIdRef = LocalPlayers[0]->AsShared();
	if (!SearchingPlayerIdRef->IsValid())
	{
		TriggerOnMatchmakingCompleteDelegates(SessionName, false);
		return false;
	}

	TArray<uint64> UserIds;
	for (const auto& UserId : LocalPlayers)
	{
		if (const auto& AttoUserId = StaticCastSharedRef<const FUniqueNetIdAtto>(UserId); AttoUserId->IsValid())
		{
			UserIds.Add(AttoUserId->Value);
		}
		else
		{
			TriggerOnMatchmakingCompleteDelegates(SessionName, false);
			return false;
		}
	}

	CurrentSessionSearch = SearchSettings;
	CurrentSessionSearch->PlatformHash = FMath::Rand32();
	CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::InProgress;

	Subsystem.AttoClient
	    ->Send<FAttoStartMatchmakingRequest>(UserIds, ConvertSearchParams(SearchSettings->QuerySettings.SearchParams), FTimespan::FromSeconds(SearchSettings->TimeoutInSeconds))
	    .Next([=, this](auto&& Response) {
		    if (!Response.IsOk())
		    {
			    if (CurrentSessionSearch)
			    {
				    CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Failed;
				    CurrentSessionSearch.Reset();
			    }
			    TriggerOnMatchmakingCompleteDelegates(SessionName, false);
			    return;
		    }

		    // TODO: Rewrite this using Visit
		    if (const auto* SessionInfo = Response.GetOkValue().template TryGet<FAttoSessionInfo>())
		    {
			    const auto SessionId = FUniqueNetIdAtto::Create(SessionInfo->SessionId);

			    auto* Session = AddNamedSession(SessionName, FOnlineSession{});
			    Session->OwningUserId = SearchingPlayerIdRef;
			    Session->SessionInfo = MakeShared<FOnlineSessionInfoAtto>(SessionId, SessionInfo->HostAddress.ToInternetAddr());
			    SessionInfo->CopyTo(*Session);

			    if (CurrentSessionSearch)
			    {
				    CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Done;
				    CurrentSessionSearch.Reset();
			    }

			    TriggerOnMatchmakingCompleteDelegates(SessionName, true);
		    }
		    else if (const auto* Timeout = Response.GetOkValue().template TryGet<FAttoStartMatchmakingRequest::FTimeout>())
		    {
			    UE_LOG_ONLINE(Warning, TEXT("Matchmaking failed due to timeout"));

			    if (CurrentSessionSearch)
			    {
				    CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Failed;
				    CurrentSessionSearch.Reset();
			    }

			    TriggerOnMatchmakingCompleteDelegates(SessionName, false);
		    }
		    else if (const auto* Error = Response.GetOkValue().template TryGet<FString>())
		    {
			    UE_LOG_ONLINE(Warning, TEXT("Matchmaking failed: %s"), **Error);

			    if (CurrentSessionSearch)
			    {
				    CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Failed;
				    CurrentSessionSearch.Reset();
			    }

			    TriggerOnMatchmakingCompleteDelegates(SessionName, false);
		    }
		    else
		    {
			    (void)Response.GetOkValue().template Get<FAttoStartMatchmakingRequest::FCanceled>();
			    TriggerOnMatchmakingCompleteDelegates(SessionName, false);
		    }
	    });

	return true;
}

bool FOnlineSessionAtto::CancelMatchmaking(const int32 SearchingPlayerNum, const FName SessionName)
{
	const auto& UserId = Subsystem.IdentityInterface->GetUniquePlayerId(SearchingPlayerNum);
	return CancelMatchmaking(UserId ? *UserId : FUniqueNetIdAtto::Invalid, SessionName);
}

bool FOnlineSessionAtto::CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, const FName SessionName)
{
	if (!CurrentSessionSearch)
	{
		return false;
	}

	const auto& UserId = static_cast<const FUniqueNetIdAtto&>(SearchingPlayerId);
	if (!UserId.IsValid())
	{
		TriggerOnCancelMatchmakingCompleteDelegates(SessionName, false);
		return false;
	}

	CurrentSessionSearch.Reset();

	Subsystem.AttoClient
	    ->Send<FAttoCancelMatchmakingRequest>(UserId.Value)
	    .Next([=, this](auto&& Response) {
		    TriggerOnCancelMatchmakingCompleteDelegates(SessionName, Response.IsOk());
	    });

	return true;
}

bool FOnlineSessionAtto::FindSessions(const int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	const auto& UserId = Subsystem.IdentityInterface->GetUniquePlayerId(SearchingPlayerNum);
	return FindSessions(*UserId, SearchSettings);
}

bool FOnlineSessionAtto::FindSessions(const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	if (CurrentSessionSearch)
	{
		UE_LOG_ONLINE_SESSION(Warning, TEXT("Ignoring game search request while one is pending"));
		return false;
	}

	if (!SearchingPlayerId.IsValid())
	{
		SearchSettings->SearchState = EOnlineAsyncTaskState::Failed;
		TriggerOnFindSessionsCompleteDelegates(false);
		return false;
	}

	CurrentSessionSearch = SearchSettings;
	CurrentSessionSearch->PlatformHash = FMath::Rand32();
	CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::InProgress;

	const auto SearchingPlayerIdRef = SearchingPlayerId.AsShared();

	Subsystem.AttoClient->Send<FAttoFindSessionsRequest>(
	                        ConvertSearchParams(SearchSettings->QuerySettings.SearchParams),
	                        StaticCastSharedRef<const FUniqueNetIdAtto>(SearchingPlayerIdRef)->Value,
	                        SearchSettings->PlatformHash,
	                        SearchSettings->MaxSearchResults)
	    .Next([=, this](auto&& Result) {
		    if (!Result.IsOk())
		    {
			    // TODO: Log error?
			    if (CurrentSessionSearch)
			    {
				    CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Failed;
				    CurrentSessionSearch.Reset();
			    }

			    // TODO: Should we fire delegates if CancelFindSessions was called?
			    TriggerOnFindSessionsCompleteDelegates(false);
			    return;
		    }

		    if (!CurrentSessionSearch)
		    {
			    // The search was canceled
			    return;
		    }

		    const auto& Message = Result.GetOkValue();
		    if (const auto& CurrentSearchId = CurrentSessionSearch->PlatformHash; CurrentSearchId != Message.RequestId)
		    {
			    CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Failed;
			    CurrentSessionSearch.Reset();
			    UE_LOG_ONLINE_SESSION(Warning, TEXT("Ignoring game search response %d because current one is %d"), Message.RequestId, CurrentSearchId);
			    return;
		    }

		    const auto BuildUniqueId = GetBuildUniqueId();

		    for (const auto& Session : Result.GetOkValue().Sessions)
		    {
			    if (Session.BuildUniqueId != BuildUniqueId)
			    {
				    continue;
			    }

			    // TODO: Use separate types for user ids and session ids?
			    const auto SessionId = FUniqueNetIdAtto::Create(Session.SessionId);

			    auto& ResultSession = CurrentSessionSearch->SearchResults.Emplace_GetRef();
			    ResultSession.Session.OwningUserId = SearchingPlayerIdRef;
			    ResultSession.Session.SessionInfo = MakeShared<FOnlineSessionInfoAtto>(SessionId, Session.HostAddress.ToInternetAddr());
			    Session.CopyTo(ResultSession.Session);
		    }

		    CurrentSessionSearch->SortSearchResults();
		    CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Done;
		    CurrentSessionSearch.Reset();

		    TriggerOnFindSessionsCompleteDelegates(true);
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

	if (CurrentSessionSearch)
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
		if (Session->RegisteredPlayers.Contains(Player))
		{
			UE_LOG_ONLINE_SESSION(Warning, TEXT("Player %s already registered in session %s"), *Player->ToDebugString(), *SessionName.ToString());
		}
		else
		{
			Session->RegisteredPlayers.AddUnique(Player);
			UE_LOG_ONLINE_SESSION(Log, TEXT("Registered player %s in session %s"), *Player->ToDebugString(), *SessionName.ToString());
		}
	}
	const auto NumAdded = Session->RegisteredPlayers.Num() - NumBefore;

	Session->NumOpenPublicConnections = FMath::Clamp(Session->NumOpenPublicConnections - NumAdded, 0, Session->SessionSettings.NumPublicConnections);
	UE_LOG_ONLINE_SESSION(Log, TEXT("Open connections in session %s: %d"), *SessionName.ToString(), Session->NumOpenPublicConnections);

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

	const auto NumBefore = Session->RegisteredPlayers.Num();
	for (const auto& Player : Players)
	{
		if (Session->RegisteredPlayers.RemoveSwap(Player) > 0)
		{
			UE_LOG_ONLINE_SESSION(Log, TEXT("Unregistered player %s from session %s"), *Player->ToDebugString(), *SessionName.ToString());
		}
		else
		{
			UE_LOG_ONLINE_SESSION(Warning, TEXT("Player %s was not registered in session %s"), *Player->ToDebugString(), *SessionName.ToString());
		}
	}
	const auto NumRemoved = NumBefore - Session->RegisteredPlayers.Num();

	Session->NumOpenPublicConnections = FMath::Clamp(Session->NumOpenPublicConnections + NumRemoved, 0, Session->SessionSettings.NumPublicConnections);
	UE_LOG_ONLINE_SESSION(Log, TEXT("Open connections in session %s: %d"), *SessionName.ToString(), Session->NumOpenPublicConnections);

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
