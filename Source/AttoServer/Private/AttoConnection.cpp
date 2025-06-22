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
	if (const auto* UserIdPtr = UserId.GetPtrOrNull())
	{
		Server.Sessions.Remove(*UserIdPtr);
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

	int64 RequestId = 0;
	Ar << RequestId;
	FAttoC2SProtocol Message;
	Ar << Message;

	if (ensure(!Ar.IsError()) && ensure(Ar.GetBitsLeft() == 0))
	{
		Visit(
		    [&]<typename RequestType>(const RequestType& Variant) {
			    static_assert(std::is_same_v<typename RequestType::Result, decltype(operator()(Variant))>);
			    Send(RequestId, FAttoS2CProtocol{TInPlaceType<typename RequestType::Result>{}, operator()(Variant)});
		    },
		    Message);
	}
	else
	{
		// TODO: Maybe just disconnect them?
	}
}

void FAttoConnection::Send(const void* Data, const size_t Size)
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

void FAttoConnection::Send(int64 RequestId, FAttoS2CProtocol&& Message)
{
	// TODO: Store writer between calls to reduce allocations?
	FBitWriter Ar{0, true};

	Ar << RequestId;
	Ar << Message;

	if (ensure(!Ar.IsError()))
	{
		Send(Ar.GetData(), Ar.GetNumBytes());
	}
}

void FAttoConnection::SendFromQueueInternal()
{
	if (auto* Message = SendQueue.Peek(); ensure(Message))
	{
		const auto Size = Message->Payload.Num() - LWS_PRE;
		// TODO: Implement partial write
		ensure(lws_write(LwsConnection, Message->Payload.GetData() + LWS_PRE, Size, LWS_WRITE_BINARY) == Size);
		SendQueue.Pop();
	}

	if (!SendQueue.IsEmpty())
	{
		lws_callback_on_writable(LwsConnection);
	}
}

FAttoLoginRequest::Result FAttoConnection::operator()(const FAttoLoginRequest& Message)
{
	// TODO: Check credentials
	(void)Message.Username;
	(void)Message.Password;

	const auto Guid = FGuid::NewGuid();
	const auto Id = CityHash64(reinterpret_cast<const char*>(&Guid), sizeof(Guid));
	UserId = Id;

	// TODO: TInPlaceType is weird
	return FAttoLoginRequest::Result{TInPlaceType<uint64>(), Id};
}

FAttoLogoutRequest::Result FAttoConnection::operator()(const FAttoLogoutRequest& Message)
{
	if (const auto* UserIdPtr = UserId.GetPtrOrNull())
	{
		Server.Sessions.Remove(*UserIdPtr);
	}

	UserId.Reset();
	return {true};
}

FAttoCreateSessionRequest::Result FAttoConnection::operator()(const FAttoCreateSessionRequest& Message)
{
	if (const auto* UserIdPtr = UserId.GetPtrOrNull())
	{
		// TODO: Check if entry already exists?
		// TODO: Should we allow to create multiple sessions simultaneously?
		Server.Sessions.Add(*UserIdPtr, Message.SessionInfo);
		return {true};
	}

	// TODO: Disconnect them?

	return {false};
}

FAttoUpdateSessionRequest::Result FAttoConnection::operator()(const FAttoUpdateSessionRequest& Message)
{
	if (const auto* UserIdPtr = UserId.GetPtrOrNull())
	{
		if (auto* Session = Server.Sessions.Find(*UserIdPtr))
		{
			Session->UpdatableInfo = Message.SessionInfo;
			return {true};
		}
	}
	else
	{
		// TODO: Disconnect them?
	}

	return {false};
}

FAttoDestroySessionRequest::Result FAttoConnection::operator()(const FAttoDestroySessionRequest& Message)
{
	if (const auto* UserIdPtr = UserId.GetPtrOrNull())
	{
		return {Server.Sessions.Remove(*UserIdPtr) > 0};
	}

	// TODO: Disconnect them?
	return {false};
}

FAttoFindSessionsRequest::Result FAttoConnection::operator()(const FAttoFindSessionsRequest& Message)
{
	TArray<FAttoSessionInfoEx> Sessions;

	if (UserId.IsSet())
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
	else
	{
		// TODO: Disconnect them?
	}

	return {Message.RequestId, MoveTemp(Sessions)};
}

FAttoQueryServerUtcTimeRequest::Result FAttoConnection::operator()(const FAttoQueryServerUtcTimeRequest& Message)
{
	return {FDateTime::UtcNow()};
}
