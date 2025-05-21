#pragma once
#include "IWebSocket.h"

class FAttoClient final : FNoncopyable
{
	TSharedRef<IWebSocket> WebSocket;

	FAttoClient();

	bool Connect(const FString& Url);

	void Close();
};
