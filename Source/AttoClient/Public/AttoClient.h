#pragma once

#include "AttoProtocol.h"
#include "IWebSocket.h"

class FOnlineSessionSearch;

class ATTOCLIENT_API FAttoClient final : FNoncopyable
{
	typedef FAttoClient ThisClass;

	TSharedRef<IWebSocket> WebSocket;

	void operator()(const FAttoLoginResponse& Message);

	void operator()(const FAttoLogoutResponse& Message);

	void operator()(const FAttoCreateSessionResponse& Message);

	void operator()(const FAttoUpdateSessionResponse& Message);

	void operator()(const FAttoDestroySessionResponse& Message);

	void operator()(const FAttoFindSessionsResponse& Message);

	void Send(FAttoC2SProtocol&& Message);

	template<
	    typename V,
	    typename... ArgTypes
	        UE_REQUIRES(std::is_constructible_v<V, ArgTypes...>)>
	void Send(ArgTypes&&... Args)
	{
		Send(FAttoC2SProtocol{TInPlaceType<V>(), Forward<ArgTypes>(Args)...});
	}

public:
	explicit FAttoClient(const FString& Url);

	void ConnectAsync();

	[[nodiscard]] bool IsConnected() const;

	void Disconnect();

	void LoginAsync(FString Username, FString Password);

	void LogoutAsync();

	void FindSessionsAsync(const FOnlineSessionSearch& Search);

	void FindSessionsAsync(TMap<FName, FAttoFindSessionsParam>&& Params, int32 RequestId, int32 MaxResults);

	void CreateSessionAsync(const FAttoSessionInfo& SessionInfo);

	void UpdateSessionAsync(uint64 SessionId, const FAttoSessionUpdatableInfo& SessionInfo);

	void DestroySessionAsync();

	DECLARE_EVENT(FAttoClient, FAttoConnectedEvent);
	FAttoConnectedEvent OnConnected;

	DECLARE_EVENT_OneParam(FAttoClient, FAttoLoginEvent, const FAttoLoginResponse&);
	FAttoLoginEvent OnLoginResponse;

	DECLARE_EVENT(FAttoClient, FAttoLogoutEvent);
	FAttoLogoutEvent OnLogoutResponse;

	DECLARE_EVENT_OneParam(FAttoClient, FAttoSessionCreatedEvent, bool /* bSuccess */);
	FAttoSessionCreatedEvent OnCreateSessionResponse;

	DECLARE_EVENT_OneParam(FAttoClient, FAttoSessionUpdatedEvent, bool /* bSuccess */);
	FAttoSessionUpdatedEvent OnUpdateSessionResponse;

	DECLARE_EVENT_OneParam(FAttoClient, FAttoSessionDestroyedEvent, bool /* bSuccess */);
	FAttoSessionDestroyedEvent OnDestroySessionResponse;

	DECLARE_EVENT_OneParam(FAttoClient, FAttoFindSessionsEvent, const FAttoFindSessionsResponse& /* Response */);
	FAttoFindSessionsEvent OnFindSessionsResponse;

	DECLARE_EVENT_TwoParams(FAttoClient, FAttoDisconnectedEvent, const FString& /* Reason */, bool /* bWasClean */);
	FAttoDisconnectedEvent OnDisconnected;

	DECLARE_EVENT_OneParam(FAttoClient, FAttoConnectionErrorEvent, const FString& /* Error */);
	FAttoConnectionErrorEvent OnConnectionError;
};
