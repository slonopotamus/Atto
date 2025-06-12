#pragma once

#include "AttoProtocol.h"
#include "IWebSocket.h"

class ATTOCLIENT_API FAttoClient final : FNoncopyable
{
	typedef FAttoClient ThisClass;

	TSharedRef<IWebSocket> WebSocket;

	void operator()(const FAttoLoginResponse& Message);

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

	void LoginAsync();

	DECLARE_EVENT(FAttoClient, FAttoClientConnectedEvent);
	FAttoClientConnectedEvent OnConnected;

	DECLARE_EVENT_OneParam(FAttoClient, FAttoClientLoginEvent, const FAttoLoginResponse&);
	FAttoClientLoginEvent OnLoginResponse;

	DECLARE_EVENT_TwoParams(FAttoClient, FAttoClientDisconnectedEvent, const FString& /* Reason */, bool /* bWasClean */);
	FAttoClientDisconnectedEvent OnDisconnected;

	DECLARE_EVENT_OneParam(FAttoClient, FAttoConnectionErrorEvent, const FString& /* Error */);
	FAttoConnectionErrorEvent OnConnectionError;
};
