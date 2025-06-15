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

	FBitReader Ar{static_cast<const uint8*>(Data), Size * 8};

	FAttoC2SProtocol Message;
	Ar << Message;

	if (ensure(!Ar.IsError()))
	{
		Visit([&](auto& Variant) { operator()(Variant); }, Message);
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

void FAttoConnection::Send(FAttoS2CProtocol&& Message)
{
	// TODO: Store writer between calls to reduce allocations?
	FBitWriter Ar{0, true};

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

void FAttoConnection::operator()(const FAttoLoginRequest& Message)
{
	// TODO: Check credentials
	(void)Message.Username;
	(void)Message.Password;

	const auto Guid = FGuid::NewGuid();
	const auto Id = CityHash64(reinterpret_cast<const char*>(&Guid), sizeof(Guid));
	UserId = Id;

	// TODO: TInPlaceType is weird
	Send<FAttoLoginResponse>(TInPlaceType<uint64>(), Id);
}

void FAttoConnection::operator()(const FAttoLogoutRequest& Message)
{
	if (const auto* UserIdPtr = UserId.GetPtrOrNull())
	{
		Server.Sessions.Remove(*UserIdPtr);
	}

	UserId.Reset();
	Send<FAttoLogoutResponse>();
}

void FAttoConnection::operator()(const FAttoCreateSessionRequest& Message)
{
	bool bSuccess = false;

	if (const auto* UserIdPtr = UserId.GetPtrOrNull())
	{
		// TODO: Check if entry already exists?
		// TODO: Should we allow to create multiple sessions simultaneously?
		Server.Sessions.Add(*UserIdPtr, Message.SessionInfo);
		bSuccess = true;
	}
	else
	{
		// TODO: Disconnect them?
	}

	Send<FAttoCreateSessionResponse>(bSuccess);
}

void FAttoConnection::operator()(const FAttoUpdateSessionRequest& Message)
{
	bool bSuccess = false;

	if (const auto* UserIdPtr = UserId.GetPtrOrNull())
	{
		if (auto* Session = Server.Sessions.Find(*UserIdPtr))
		{
			Session->UpdatableInfo = Message.SessionInfo;
			bSuccess = true;
		}
	}
	else
	{
		// TODO: Disconnect them?
	}

	Send<FAttoUpdateSessionResponse>(bSuccess);
}

void FAttoConnection::operator()(const FAttoDestroySessionRequest& Message)
{
	bool bSuccess = false;

	if (const auto* UserIdPtr = UserId.GetPtrOrNull())
	{
		bSuccess = Server.Sessions.Remove(*UserIdPtr) > 0;
	}
	else
	{
		// TODO: Disconnect them?
	}

	Send<FAttoDestroySessionResponse>(bSuccess);
}

void FAttoConnection::operator()(const FAttoFindSessionsRequest& Message)
{
	TArray<FAttoSessionInfoEx> Sessions;

	if (UserId.IsSet())
	{
		// TODO: Respect search params

		const auto MaxResults = FMath::Min(Message.MaxResults, Server.MaxFindSessionsResults);

		for (const auto& [OwningUserId, Session] : Server.Sessions)
		{
			if (Sessions.Num() >= MaxResults)
			{
				break;
			}

			if (Session.IsJoinable())
			{
				Sessions.Emplace(OwningUserId, Session);
			}
		}
	}
	else
	{
		// TODO: Disconnect them?
	}

	Send<FAttoFindSessionsResponse>(Message.RequestId, MoveTemp(Sessions));
}
