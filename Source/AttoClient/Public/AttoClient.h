#pragma once

#include "IWebSocket.h"

class ATTOCLIENT_API FAttoClient final : FNoncopyable
{
	typedef FAttoClient ThisClass;

	TSharedRef<IWebSocket> WebSocket;

	void OnMessage(const FString& Message);

	void Send(const FString& Message);

public:
	explicit FAttoClient(const FString& Url);

	void Connect();

	[[nodiscard]] bool IsConnected() const;

	void Disconnect();

	DECLARE_EVENT(FAttoClient, FAttoClientConnectedEvent);
	FAttoClientConnectedEvent OnConnected;

	DECLARE_EVENT_TwoParams(FAttoClient, FAttoClientDisconnectedEvent, const FString& /* Reason */, bool /* bWasClean */);
	FAttoClientDisconnectedEvent OnDisconnected;

	DECLARE_EVENT_OneParam(FAttoClient, FAttoConnectionErrorEvent, const FString& /* Error */);
	FAttoConnectionErrorEvent OnConnectionError;
};
