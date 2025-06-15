#include "AttoClient.h"
#include "AttoCommon.h"
#include "AttoProtocol.h"
#include "OnlineSessionSettings.h"
#include "WebSocketsModule.h"

FAttoClient::FAttoClient(const FString& Url)
    : WebSocket{FWebSocketsModule::Get().CreateWebSocket(Url, Atto::Protocol)}
{
	WebSocket->OnConnected().AddLambda([&] {
		OnConnected.Broadcast();
	});

	WebSocket->OnClosed().AddLambda([&](int32 StatusCode, const FString& Reason, const bool bWasClean) { OnDisconnected.Broadcast(Reason, bWasClean); });

	WebSocket->OnConnectionError().AddLambda([&](const FString& Error) { OnConnectionError.Broadcast(Error); });

	WebSocket->OnBinaryMessage().AddLambda([&](const void* Data, const size_t Size, const bool bIsLastFragment) {
		if (!ensure(bIsLastFragment))
		{
			return;
		}

		FBitReader Ar{static_cast<const uint8*>(Data), Size * 8};

		FAttoS2CProtocol Message;
		Ar << Message;

		if (ensure(!Ar.IsError()))
		{
			Visit([&](auto& Variant) { this->operator()(Variant); }, Message);
		}
	});
}

void FAttoClient::ConnectAsync()
{
	WebSocket->Connect();
}

void FAttoClient::Disconnect()
{
	WebSocket->Close();
}

void FAttoClient::Send(FAttoC2SProtocol&& Message)
{
	// TODO: Store writer between calls to reduce allocations?
	FBitWriter Ar{0, true};

	Ar << Message;

	if (ensure(!Ar.IsError()))
	{
		WebSocket->Send(Ar.GetData(), Ar.GetNumBytes(), true);
	}
}

bool FAttoClient::IsConnected() const
{
	return WebSocket->IsConnected();
}

void FAttoClient::LoginAsync(FString Username, FString Password)
{
	Send<FAttoLoginRequest>(MoveTemp(Username), MoveTemp(Password));
}

void FAttoClient::LogoutAsync()
{
	Send<FAttoLogoutRequest>();
}

void FAttoClient::FindSessionsAsync(const FOnlineSessionSearch& Search)
{
	Send<FAttoFindSessionsRequest>(Search.PlatformHash, Search.MaxSearchResults);
}

void FAttoClient::CreateSessionAsync(const FAttoSessionInfo& SessionInfo)
{
	Send<FAttoCreateSessionRequest>(SessionInfo);
}

void FAttoClient::UpdateSessionAsync(uint64 SessionId, const FAttoSessionUpdatableInfo& SessionInfo)
{
	Send<FAttoUpdateSessionRequest>(SessionId, SessionInfo);
}

void FAttoClient::DestroySessionAsync()
{
	Send<FAttoDestroySessionRequest>();
}

void FAttoClient::operator()(const FAttoLoginResponse& Message)
{
	OnLoginResponse.Broadcast(Message);
}

void FAttoClient::operator()(const FAttoLogoutResponse& Message)
{
	OnLogoutResponse.Broadcast();
}

void FAttoClient::operator()(const FAttoCreateSessionResponse& Message)
{
	OnCreateSessionResponse.Broadcast(Message.bSuccess);
}

void FAttoClient::operator()(const FAttoUpdateSessionResponse& Message)
{
	OnUpdateSessionResponse.Broadcast(Message.bSuccess);
}

void FAttoClient::operator()(const FAttoDestroySessionResponse& Message)
{
	OnDestroySessionResponse.Broadcast(Message.bSuccess);
}

void FAttoClient::operator()(const FAttoFindSessionsResponse& Message)
{
	OnFindSessionsResponse.Broadcast(Message);
}
