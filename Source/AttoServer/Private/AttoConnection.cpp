#include "AttoConnection.h"
#include "AttoServer.h"

// Work around a conflict between a UI namespace defined by engine code and a typedef in OpenSSL
#define UI UI_ST
THIRD_PARTY_INCLUDES_START
#include "libwebsockets.h"
THIRD_PARTY_INCLUDES_END
#undef UI

FAttoConnection::~FAttoConnection()
{
	for (const auto& UserId : Users)
	{
		Server.Sessions.Remove(UserId);
	}
}

void FAttoConnection::ReceiveInternal(const void* Data, const size_t Size)
{
	if (!lws_frame_is_binary(LwsConnection))
	{
		return;
	}

	// TODO: Implement partial read
	if (!ensure(lws_is_final_fragment(LwsConnection)))
	{
		return;
	}

	FBitReader Ar{static_cast<const uint8*>(Data), static_cast<int64>(Size * 8)};

	int64 RequestId = SERVER_PUSH_REQUEST_ID;
	Ar << RequestId;

	if (!ensure(RequestId != SERVER_PUSH_REQUEST_ID))
	{
		// TODO: Disconnect them?
		return;
	}

	FAttoClientRequestProtocol Message;
	Ar << Message;

	if (!ensure(!Ar.IsError()))
	{
		// TODO: Disconnect them?
		return;
	}

	Visit(
	    [&]<typename RequestType>(const RequestType& Variant) {
		    static_assert(std::is_same_v<TFuture<typename RequestType::Result>, decltype(operator()(Variant))>);
		    operator()(Variant)
		        .Next([=, this](auto&& Result) {
			        Send(RequestId, FAttoServerResponseProtocol{TInPlaceType<typename RequestType::Result>{}, Result});
		        });
	    },
	    Message);
}

void FAttoConnection::SendRaw(const void* Data, const size_t Size)
{
	if (!ensure(Data))
	{
		return;
	}

	TArray<unsigned char> Payload;

	Payload.Reserve(LWS_PRE + Size);
	Payload.AddDefaulted(LWS_PRE);
	Payload.Append(static_cast<const unsigned char*>(Data), Size);

	SendQueue.Enqueue({
	    .Payload = MoveTemp(Payload),
	});

	lws_callback_on_writable(LwsConnection);
}

void FAttoConnection::SendFromQueueInternal()
{
	if (auto* Message = SendQueue.Peek(); ensure(Message))
	{
		const auto PayloadSize = Message->Payload.Num() - LWS_PRE;
		const auto Offset = Message->BytesWritten;
		const auto Protocol = Message->BytesWritten == 0 ? LWS_WRITE_BINARY : LWS_WRITE_CONTINUATION;

		const auto BytesWritten = lws_write(
		    LwsConnection,
		    Message->Payload.GetData() + LWS_PRE + Offset,
		    PayloadSize - Offset,
		    Protocol);

		if (BytesWritten < 0)
		{
			// TODO: Log error?
			SendQueue.Pop();
		}
		else
		{
			Message->BytesWritten += BytesWritten;
			if (Message->BytesWritten >= PayloadSize)
			{
				SendQueue.Pop();
			}
		}
	}

	if (!SendQueue.IsEmpty())
	{
		lws_callback_on_writable(LwsConnection);
	}
}

TFuture<FAttoLoginRequest::Result> FAttoConnection::operator()(const FAttoLoginRequest& Message)
{
	auto Func = [&]() -> FAttoLoginRequest::Result {
		if (Message.BuildUniqueId != GetBuildUniqueId())
		{
			return FAttoLoginRequest::Result{
			    TInPlaceType<FString>(),
			    FString::Printf(TEXT("Rejecting login [%s]: mismatched build id. Server: %d != Client: %d"), *Message.Username, GetBuildUniqueId(), Message.BuildUniqueId)};
		}

		if (Message.Username.IsEmpty())
		{
			return FAttoLoginRequest::Result{TInPlaceType<FString>(), TEXT("Username must not be empty")};
		}

		if (Message.Username != Message.Password)
		{
			return FAttoLoginRequest::Result{TInPlaceType<FString>(), TEXT("Wrong password")};
		}

		const auto Guid = FGuid::NewGuid();
		const auto Id = CityHash64(reinterpret_cast<const char*>(&Guid), sizeof(Guid));
		Users.Add(Id);

		// TODO: TInPlaceType is weird
		return FAttoLoginRequest::Result{TInPlaceType<uint64>(), Id};
	};

	return MakeFulfilledPromise<FAttoLoginRequest::Result>(Func()).GetFuture();
}

TFuture<FAttoLogoutRequest::Result> FAttoConnection::operator()(const FAttoLogoutRequest& Message)
{
	const bool bSuccess = Users.Remove(Message.UserId) > 0;

	if (bSuccess)
	{
		Server.Sessions.Remove(Message.UserId);
	}

	return MakeFulfilledPromise<FAttoLogoutRequest::Result>(bSuccess).GetFuture();
}

TFuture<FAttoCreateSessionRequest::Result> FAttoConnection::operator()(const FAttoCreateSessionRequest& Message)
{
	bool bSuccess = false;

	if (Users.Contains(Message.SessionInfo.OwningUserId))
	{
		// TODO: Check if entry already exists? For any of users? But what if they login later?
		Server.Sessions.Add(Message.SessionInfo.OwningUserId, Message.SessionInfo.SessionInfo);
		bSuccess = true;
	}

	return MakeFulfilledPromise<FAttoCreateSessionRequest::Result>(bSuccess).GetFuture();
}

TFuture<FAttoUpdateSessionRequest::Result> FAttoConnection::operator()(const FAttoUpdateSessionRequest& Message)
{
	bool bSuccess = false;

	if (Users.Contains(Message.OwningUserId))
	{
		if (auto* Session = Server.Sessions.Find(Message.OwningUserId))
		{
			Session->UpdatableInfo = Message.SessionInfo;
			bSuccess = true;
		}
	}

	return MakeFulfilledPromise<FAttoUpdateSessionRequest::Result>(bSuccess).GetFuture();
}

TFuture<FAttoDestroySessionRequest::Result> FAttoConnection::operator()(const FAttoDestroySessionRequest& Message)
{
	bool bSuccess = false;

	if (Users.Contains(Message.OwningUserId))
	{
		bSuccess = Server.Sessions.Remove(Message.OwningUserId) > 0;
	}

	return MakeFulfilledPromise<FAttoDestroySessionRequest::Result>(bSuccess).GetFuture();
}

TFuture<FAttoFindSessionsRequest::Result> FAttoConnection::operator()(const FAttoFindSessionsRequest& Message)
{
	TArray<FAttoSessionInfoEx> Sessions;

	if (!Users.IsEmpty())
	{
		const auto MaxResults = FMath::Min(Message.MaxResults, Server.MaxFindSessionsResults);

		for (const auto& [OwningUserId, Session] : Server.Sessions)
		{
			if (Sessions.Num() >= MaxResults)
			{
				break;
			}

			if (!Session.IsJoinable())
			{
				continue;
			}

			if (!Session.Matches(Message.Params))
			{
				continue;
			}

			Sessions.Emplace(OwningUserId, Session);
		}
	}

	return MakeFulfilledPromise<FAttoFindSessionsRequest::Result>(Message.RequestId, MoveTemp(Sessions)).GetFuture();
}

TFuture<FAttoQueryServerUtcTimeRequest::Result> FAttoConnection::operator()(const FAttoQueryServerUtcTimeRequest& Message)
{
	return MakeFulfilledPromise<FAttoQueryServerUtcTimeRequest::Result>(FDateTime::UtcNow()).GetFuture();
}
