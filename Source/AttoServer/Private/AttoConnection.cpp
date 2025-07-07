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
		Server.Matchmaker.DestroySession(UserId);
	}

	Server.Matchmaker.Cancel(MatchmakingToken);
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
	UE_LOG(LogAtto, Verbose, TEXT("Login request from %s"), *Message.Username);

	auto Func = [&]() -> FAttoLoginRequest::Result {
		if (Message.BuildUniqueId != GetBuildUniqueId())
		{
			const auto Error = FString::Printf(TEXT("Mismatched build id. Server: %d != Client: %d"), GetBuildUniqueId(), Message.BuildUniqueId);
			UE_LOG(LogAtto, Verbose, TEXT("%s"), *Error);
			return FAttoLoginRequest::Result{TInPlaceType<FString>(), Error};
		}

		if (Message.Username.IsEmpty())
		{
			static const auto* const Error = TEXT("Username must not be empty");
			UE_LOG(LogAtto, Verbose, TEXT("%s"), Error);
			return FAttoLoginRequest::Result{TInPlaceType<FString>(), Error};
		}

		if (Message.Username != Message.Password)
		{
			static const auto* const Error = TEXT("Wrong password");
			UE_LOG(LogAtto, Verbose, TEXT("%s"), Error);
			return FAttoLoginRequest::Result{TInPlaceType<FString>(), Error};
		}

		const auto Guid = FGuid::NewGuid();
		const auto Id = CityHash64(reinterpret_cast<const char*>(&Guid), sizeof(Guid));
		Users.Add(Id);

		UE_LOG(LogAtto, Verbose, TEXT("Login succeeded with userId=%016llx"), Id);
		return FAttoLoginRequest::Result{TInPlaceType<uint64>(), Id};
	};

	return MakeFulfilledPromise<FAttoLoginRequest::Result>(Func()).GetFuture();
}

TFuture<FAttoLogoutRequest::Result> FAttoConnection::operator()(const FAttoLogoutRequest& Message)
{
	UE_LOG(LogAtto, Verbose, TEXT("Logout request from %016llx"), Message.UserId);
	const bool bSuccess = Users.Remove(Message.UserId) > 0;

	if (bSuccess)
	{
		Server.Matchmaker.DestroySession(Message.UserId);
		Server.Matchmaker.Cancel(MatchmakingToken);
		UE_LOG(LogAtto, Verbose, TEXT("Logout succeeded"));
	}
	else
	{
		UE_LOG(LogAtto, Verbose, TEXT("Logout failed"));
	}

	return MakeFulfilledPromise<FAttoLogoutRequest::Result>(bSuccess).GetFuture();
}

TFuture<FAttoCreateSessionRequest::Result> FAttoConnection::operator()(const FAttoCreateSessionRequest& Message)
{
	UE_LOG(LogAtto, Verbose, TEXT("Create session request from %016llx"), Message.OwningUserId);
	bool bSuccess = false;

	if (Users.Contains(Message.OwningUserId))
	{
		if (Server.Matchmaker.CreateSession(Message.OwningUserId, Message.SessionInfo))
		{
			UE_LOG(LogAtto, Verbose, TEXT("Session created successfully"));
			bSuccess = true;
		}
		else
		{
			UE_LOG(LogAtto, Verbose, TEXT("Create session failed"));
		}
	}
	else
	{
		UE_LOG(LogAtto, Verbose, TEXT("Create session failed: user is not authenticated"));
	}

	return MakeFulfilledPromise<FAttoCreateSessionRequest::Result>(bSuccess).GetFuture();
}

TFuture<FAttoUpdateSessionRequest::Result> FAttoConnection::operator()(const FAttoUpdateSessionRequest& Message)
{
	UE_LOG(LogAtto, Verbose, TEXT("Update session request from %016llx"), Message.OwningUserId);

	bool bSuccess = false;

	if (Users.Contains(Message.OwningUserId))
	{
		if (Server.Matchmaker.UpdateSession(Message.OwningUserId, Message.SessionInfo))
		{
			UE_LOG(LogAtto, Verbose, TEXT("Session updated successfully"));
			bSuccess = true;
		}
		else
		{
			UE_LOG(LogAtto, Verbose, TEXT("Update session failed"));
		}
	}
	else
	{
		UE_LOG(LogAtto, Verbose, TEXT("Update session failed: user is not authenticated"));
	}

	return MakeFulfilledPromise<FAttoUpdateSessionRequest::Result>(bSuccess).GetFuture();
}

TFuture<FAttoDestroySessionRequest::Result> FAttoConnection::operator()(const FAttoDestroySessionRequest& Message)
{
	UE_LOG(LogAtto, Verbose, TEXT("Destroy session request from %016llx"), Message.OwningUserId);
	bool bSuccess = false;

	if (Users.Contains(Message.OwningUserId))
	{
		if (Server.Matchmaker.DestroySession(Message.OwningUserId))
		{
			UE_LOG(LogAtto, Verbose, TEXT("Session destroyed successfully"));
			bSuccess = true;
		}
		else
		{
			UE_LOG(LogAtto, Verbose, TEXT("Destroy session failed"));
		}
	}
	else
	{
		UE_LOG(LogAtto, Verbose, TEXT("Destroy session failed: user is not authenticated"));
	}

	return MakeFulfilledPromise<FAttoDestroySessionRequest::Result>(bSuccess).GetFuture();
}

TFuture<FAttoFindSessionsRequest::Result> FAttoConnection::operator()(const FAttoFindSessionsRequest& Message)
{
	UE_LOG(LogAtto, Verbose, TEXT("Find sessions request [%d] from %016llx"), Message.RequestId, Message.SearchingUserId);
	TArray<FAttoSessionInfo> Sessions;

	if (Users.Contains(Message.SearchingUserId))
	{
		Server.Matchmaker.FindSessions(Users.Num(), Message, Sessions);
		UE_LOG(LogAtto, Verbose, TEXT("Sessions found: %d"), Sessions.Num());
	}
	else
	{
		UE_LOG(LogAtto, Verbose, TEXT("Find sessions failed: user is not authenticated"));
	}

	return MakeFulfilledPromise<FAttoFindSessionsRequest::Result>(Message.RequestId, MoveTemp(Sessions)).GetFuture();
}

TFuture<FAttoQueryServerUtcTimeRequest::Result> FAttoConnection::operator()(const FAttoQueryServerUtcTimeRequest& Message)
{
	UE_LOG(LogAtto, Verbose, TEXT("Query server UTC time request"));
	return MakeFulfilledPromise<FAttoQueryServerUtcTimeRequest::Result>(FDateTime::UtcNow()).GetFuture();
}

TFuture<FAttoStartMatchmakingRequest::Result> FAttoConnection::operator()(const FAttoStartMatchmakingRequest& Message)
{
	UE_LOG(LogAtto, Verbose, TEXT("Start matchmaking request"));

	if (Users.IsEmpty())
	{
		static const auto* const Error = TEXT("Not authenticated");
		UE_LOG(LogAtto, Verbose, TEXT("%s"), Error);
		return MakeFulfilledPromise<FAttoStartMatchmakingRequest::Result>(TInPlaceType<FString>{}, Error).GetFuture();
	}

	if (Users.Num() != Message.Users.Num())
	{
		static const auto* const Error = TEXT("All connected users must enter matchmaking");
		UE_LOG(LogAtto, Verbose, TEXT("%s"), Error);
		return MakeFulfilledPromise<FAttoStartMatchmakingRequest::Result>(TInPlaceType<FString>{}, Error).GetFuture();
	}

	for (const auto& User : Message.Users)
	{
		if (!Users.Contains(User))
		{
			const auto Error = FString::Printf(TEXT("User %016llx is not logged in"), User);
			UE_LOG(LogAtto, Verbose, TEXT("%s"), *Error);
			return MakeFulfilledPromise<FAttoStartMatchmakingRequest::Result>(TInPlaceType<FString>{}, Error).GetFuture();
		}
	}

	if (MatchmakingToken.IsValid())
	{
		static const auto* const Error = TEXT("Already in matchmaking queue");
		UE_LOG(LogAtto, Verbose, TEXT("%s"), Error);
		return MakeFulfilledPromise<FAttoStartMatchmakingRequest::Result>(TInPlaceType<FString>{}, Error).GetFuture();
	}

	return Server.Matchmaker.Enqueue(MatchmakingToken, Users.Num(), Message)
	    .Next([=, this](const auto&& Response) {
		    MatchmakingToken.Invalidate();
		    return Response;
	    });
}

TFuture<FAttoCancelMatchmakingRequest::Result> FAttoConnection::operator()(const FAttoCancelMatchmakingRequest& Message)
{
	UE_LOG(LogAtto, Verbose, TEXT("Cancel matchmaking request from %016llx"), Message.UserId);
	bool bSuccess = false;

	if (Users.Contains(Message.UserId))
	{
		if (Server.Matchmaker.Cancel(MatchmakingToken))
		{
			UE_LOG(LogAtto, Verbose, TEXT("Matchmaking canceled successfully"));
			bSuccess = true;
		}
		else
		{
			UE_LOG(LogAtto, Verbose, TEXT("Cancel matchmaking failed"));
		}
	}
	else
	{
		UE_LOG(LogAtto, Verbose, TEXT("Cancel matchmaking failed: user is not authenticated"));
	}

	return MakeFulfilledPromise<FAttoCancelMatchmakingRequest::Result>(bSuccess).GetFuture();
}
