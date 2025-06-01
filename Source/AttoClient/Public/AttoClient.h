#pragma once
#include "IWebSocket.h"

class FAttoClient final : FNoncopyable
{
	typedef FAttoClient ThisClass;
	TSharedRef<IWebSocket> WebSocket;

public:
	explicit FAttoClient(const FString& Url);

	void Connect();

	bool IsConnected() const;

	void Close();

	DECLARE_EVENT(FAttoClient, FAttoClientConnectedEvent);
	FAttoClientConnectedEvent OnConnected;

	DECLARE_EVENT_TwoParams(FAttoClient, FAttoClientDisconnectedEvent, const FString& /* Reason */, bool /* bWasClean */);
	FAttoClientDisconnectedEvent OnDisconnected;
};
