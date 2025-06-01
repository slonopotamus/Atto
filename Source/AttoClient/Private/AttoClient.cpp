#include "AttoClient.h"

#include "AttoCommon.h"
#include "WebSocketsModule.h"

FAttoClient::FAttoClient(const FString& Url)
    : WebSocket{FWebSocketsModule::Get().CreateWebSocket(Url, Atto::Protocol)}
{
	WebSocket->OnConnected().AddLambda([&]
	                                   { OnConnected.Broadcast(); });
	WebSocket->OnClosed().AddLambda([&](int32 StatusCode, const FString& Reason, bool bWasClean)
	                                { OnDisconnected.Broadcast(Reason, bWasClean); });
}

void FAttoClient::Connect()
{
	WebSocket->Connect();
}

void FAttoClient::Close()
{
	WebSocket->Close();
}

bool FAttoClient::IsConnected() const
{
	return WebSocket->IsConnected();
}
