#include "AttoClient.h"

#include "AttoCommon.h"
#include "WebSocketsModule.h"

FAttoClient::FAttoClient(const FString& Url)
    : WebSocket{FWebSocketsModule::Get().CreateWebSocket(Url, Atto::Protocol)}
{
	WebSocket->OnConnected().AddLambda([&]
	                                   {
		OnConnected.Broadcast();
		WebSocket->Send(TEXT("Ping")); });

	WebSocket->OnClosed().AddLambda([&](int32 StatusCode, const FString& Reason, bool bWasClean)
	                                { OnDisconnected.Broadcast(Reason, bWasClean); });

	WebSocket->OnConnectionError().AddLambda([&](const FString& Error)
	                                         { OnConnectionError.Broadcast(Error); });

	WebSocket->OnMessage().AddRaw(this, &ThisClass::OnMessage);
}

void FAttoClient::Connect()
{
	WebSocket->Connect();
}

void FAttoClient::Disconnect()
{
	WebSocket->Close();
}

bool FAttoClient::IsConnected() const
{
	return WebSocket->IsConnected();
}

void FAttoClient::OnMessage(const FString& Message)
{
	UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
}

void FAttoClient::Send(const FString& Message)
{
	WebSocket->Send(Message);
}
