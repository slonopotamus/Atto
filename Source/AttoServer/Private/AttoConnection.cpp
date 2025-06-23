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

	int64 RequestId = 0;
	Ar << RequestId;
	FAttoC2SProtocol Message;
	Ar << Message;

	if (ensure(!Ar.IsError()))
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

FAttoLoginRequest::Result FAttoConnection::operator()(const FAttoLoginRequest& Message)
{
	// TODO: Check credentials
	(void)Message.Username;
	(void)Message.Password;

	const auto Guid = FGuid::NewGuid();
	const auto Id = CityHash64(reinterpret_cast<const char*>(&Guid), sizeof(Guid));
	Users.Add(Id);

	// TODO: TInPlaceType is weird
	return FAttoLoginRequest::Result{TInPlaceType<uint64>(), Id};
}

FAttoLogoutRequest::Result FAttoConnection::operator()(const FAttoLogoutRequest& Message)
{
	const bool bSuccess = Server.Sessions.Remove(Message.UserId) > 0;
	return {bSuccess};
}

FAttoCreateSessionRequest::Result FAttoConnection::operator()(const FAttoCreateSessionRequest& Message)
{
	if (!Users.Contains(Message.SessionInfo.OwningUserId))
	{
		return {false};
	}

	// TODO: Check if entry already exists? For any of users? But what if they login later?
	Server.Sessions.Add(Message.SessionInfo.OwningUserId, Message.SessionInfo.SessionInfo);
	return {true};
}

FAttoUpdateSessionRequest::Result FAttoConnection::operator()(const FAttoUpdateSessionRequest& Message)
{
	if (!Users.Contains(Message.OwningUserId))
	{
		return {false};
	}

	auto* Session = Server.Sessions.Find(Message.OwningUserId);
	if (!Session)
	{
		return {false};
	}

	Session->UpdatableInfo = Message.SessionInfo;
	return {true};
}

FAttoDestroySessionRequest::Result FAttoConnection::operator()(const FAttoDestroySessionRequest& Message)
{
	if (!Users.Contains(Message.OwningUserId))
	{
		return {false};
	}

	return {Server.Sessions.Remove(Message.OwningUserId) > 0};
}

FAttoFindSessionsRequest::Result FAttoConnection::operator()(const FAttoFindSessionsRequest& Message)
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

	return {Message.RequestId, MoveTemp(Sessions)};
}

FAttoQueryServerUtcTimeRequest::Result FAttoConnection::operator()(const FAttoQueryServerUtcTimeRequest& Message)
{
	return {FDateTime::UtcNow()};
}
