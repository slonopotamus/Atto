#include "AttoClient.h"

#include "AttoCommon.h"
#include "WebSocketsModule.h"

FAttoClient::FAttoClient()
    : WebSocket{FWebSocketsModule::Get().CreateWebSocket(Url, Atto::Protocol)}
{
}

void FAttoClient::Connect()
{
	WebSocket->Connect();
}

void FAttoClient::Close()
{
	WebSocket->Close();
}
