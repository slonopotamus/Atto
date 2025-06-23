#pragma once

#include "AttoProtocol.h"

class FAttoServer;
class FAttoServerInstance;
struct lws;

struct FAttoMessage final
{
	TArray<unsigned char> Payload;
	int32 BytesWritten = 0;
};

class FAttoConnection final : public FNoncopyable
{
	FAttoServerInstance& Server;
	lws* const LwsConnection;

	// TODO: Separate dedicated servers from real users?
	/** User ids of currently logged-in users on this connection */
	TSet<uint64> Users;

	TQueue<FAttoMessage> SendQueue;

	void Send(const void* Data, size_t Size);

	void Send(int64 RequestId, FAttoS2CProtocol&& Message);

	template<
	    typename V,
	    typename... ArgTypes
	        UE_REQUIRES(std::is_constructible_v<V, ArgTypes...>)>
	void Send(int64 RequestId, ArgTypes&&... Args)
	{
		Send(RequestId, FAttoS2CProtocol{TInPlaceType<V>(), Forward<ArgTypes>(Args)...});
	}

	FAttoLoginRequest::Result operator()(const FAttoLoginRequest& Message);

	FAttoLogoutRequest::Result operator()(const FAttoLogoutRequest& Message);

	FAttoCreateSessionRequest::Result operator()(const FAttoCreateSessionRequest& Message);

	FAttoUpdateSessionRequest::Result operator()(const FAttoUpdateSessionRequest& Message);

	FAttoDestroySessionRequest::Result operator()(const FAttoDestroySessionRequest& Message);

	FAttoFindSessionsRequest::Result operator()(const FAttoFindSessionsRequest& Message);

	FAttoQueryServerUtcTimeRequest::Result operator()(const FAttoQueryServerUtcTimeRequest& Message);

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
