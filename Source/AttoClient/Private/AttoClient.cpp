#include "AttoClient.h"
#include "AttoCommon.h"
#include "AttoProtocol.h"
#include "WebSocketsModule.h"

FAttoClient::FAttoClient(const FString& Url)
    : WebSocket{FWebSocketsModule::Get().CreateWebSocket(Url, Atto::Protocol)}
{
	WebSocket->OnConnected().AddLambda([=, this] {
		for (const auto& Promise : ConnectPromises)
		{
			Promise->SetValue(true);
		}

		ConnectPromises.Empty();
	});

	WebSocket->OnConnectionError().AddLambda([=, this](const FString& Error) {
		// TODO: Log error?

		for (const auto& Promise : ConnectPromises)
		{
			Promise->SetValue(false);
		}

		ConnectPromises.Empty();
	});

	WebSocket->OnClosed().AddLambda([=, this](int32 StatusCode, const FString& Reason, const bool bWasClean) {
		for (const auto& [_, Promise] : RequestPromises)
		{
			Promise->HandleDisconnect();
		}

		RequestPromises.Empty();
		OnDisconnected.Broadcast(Reason, bWasClean);
	});

	WebSocket->OnBinaryMessage().AddLambda([=, this](const void* Data, const size_t Size, const bool bIsLastFragment) {
		if (!ensure(bIsLastFragment))
		{
			return;
		}

		FBitReader Ar{static_cast<const uint8*>(Data), static_cast<int64>(Size * 8)};

		int64 RequestId = SERVER_PUSH_REQUEST_ID;
		Ar << RequestId;

		if (RequestId == SERVER_PUSH_REQUEST_ID)
		{
			FAttoServerPushProtocol Message;
			Ar << Message;

			if (ensure(!Ar.IsError()))
			{
				OnServerPush.Broadcast(Message);
			}

			return;
		}

		FAttoServerResponseProtocol Message;
		Ar << Message;

		if (ensure(!Ar.IsError()))
		{
			if (TUniquePtr<IRequestPromise> RequestPromise; ensure(RequestPromises.RemoveAndCopyValue(RequestId, RequestPromise)))
			{
				RequestPromise->Handle(MoveTemp(Message));
			}
		}
	});
}

FAttoClient::~FAttoClient()
{
	WebSocket->OnConnected().Clear();
	WebSocket->OnConnectionError().Clear();
	WebSocket->OnClosed().Clear();
	WebSocket->OnBinaryMessage().Clear();
	WebSocket->Close();

	for (const auto& Promise : ConnectPromises)
	{
		Promise->SetValue(false);
	}
}

TFuture<bool> FAttoClient::ConnectAsync()
{
	if (IsConnected())
	{
		return MakeFulfilledPromise<bool>(true).GetFuture();
	}

	const auto Promise = ConnectPromises.Add_GetRef(MakeShared<TPromise<bool>>());
	WebSocket->Connect();
	return Promise->GetFuture();
}

void FAttoClient::Disconnect()
{
	WebSocket->Close();
}

void FAttoClient::Send(int64 RequestId, FAttoClientRequestProtocol&& Message)
{
	// TODO: Store writer between calls to reduce allocations?
	FBitWriter Ar{0, true};

	Ar << RequestId;
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
