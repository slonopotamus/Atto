#pragma once

#include "AttoProtocol.h"

class FAttoServer;
class FAttoServerInstance;
struct lws;

class FAttoMessage final
{
public:
	explicit FAttoMessage(const void* Data, const size_t Size);

	FAttoMessage();

	bool Write(lws* LwsConnection);

private:
	TArray<unsigned char> Buffer;
	TArrayView<unsigned char> Payload;

	int32 BytesWritten = 0;
};

class FAttoConnection final : public FNoncopyable
{
	FAttoServerInstance& Server;
	lws* const LwsConnection;
	FGuid MatchmakingToken;

	// TODO: Separate dedicated servers from real users?
	/** User ids of currently logged-in users on this connection */
	TSet<uint64> Users;

	TQueue<FAttoMessage> SendQueue;

	void SendRaw(const void* Data, size_t Size);

	template<typename T>
	void SendImpl(int64 RequestId, T&& Message)
	{
		// TODO: Store writer between calls to reduce allocations?
		FBitWriter Ar{0, true};

		Ar << RequestId;
		Ar << Message;

		if (ensure(!Ar.IsError()))
		{
			SendRaw(Ar.GetData(), Ar.GetNumBytes());
		}
	}

	void Send(const int64 RequestId, FAttoServerResponseProtocol&& Message)
	{
		if (ensure(RequestId != SERVER_PUSH_REQUEST_ID))
		{
			SendImpl(RequestId, Message);
		}
	}

	void Send(FAttoServerPushProtocol&& Message)
	{
		SendImpl(SERVER_PUSH_REQUEST_ID, Message);
	}

	template<
	    typename V,
	    typename... ArgTypes
	        UE_REQUIRES(std::is_constructible_v<V, ArgTypes...>)>
	void Send(const int64 RequestId, ArgTypes&&... Args)
	{
		Send(RequestId, FAttoServerResponseProtocol{TInPlaceType<V>(), Forward<ArgTypes>(Args)...});
	}

	TFuture<FAttoLoginRequest::Result> operator()(const FAttoLoginRequest& Message);

	TFuture<FAttoLogoutRequest::Result> operator()(const FAttoLogoutRequest& Message);

	TFuture<FAttoCreateSessionRequest::Result> operator()(const FAttoCreateSessionRequest& Message);

	TFuture<FAttoUpdateSessionRequest::Result> operator()(const FAttoUpdateSessionRequest& Message);

	TFuture<FAttoDestroySessionRequest::Result> operator()(const FAttoDestroySessionRequest& Message);

	TFuture<FAttoFindSessionsRequest::Result> operator()(const FAttoFindSessionsRequest& Message);

	TFuture<FAttoQueryServerUtcTimeRequest::Result> operator()(const FAttoQueryServerUtcTimeRequest& Message);

	TFuture<FAttoStartMatchmakingRequest::Result> operator()(const FAttoStartMatchmakingRequest& Message);

	TFuture<FAttoCancelMatchmakingRequest::Result> operator()(const FAttoCancelMatchmakingRequest& Message);

public:
	explicit FAttoConnection(FAttoServerInstance& Server, lws* LwsConnection)
	    : Server(Server)
	    , LwsConnection(LwsConnection)
	{
	}

	~FAttoConnection();

	void SendFromQueueInternal();

	void ReceiveInternal(const void* Data, size_t Size);
};
