#pragma once

#include "AttoProtocol.h"

class FAttoServer;
class FAttoServerInstance;
struct lws;

struct FAttoMessage final
{
	TArray<unsigned char> Payload;
};

class FAttoConnection final : public FNoncopyable
{
	FAttoServerInstance& Server;
	lws* const LwsConnection;

	// TODO: Separate dedicated servers from real users?
	// TODO: Support multiple users per connection?
	/** User id of currently logged-in user (if any) */
	TOptional<uint64> UserId;

	TQueue<FAttoMessage> SendQueue;

	void Send(const void* Data, size_t Size);

	void Send(FAttoS2CProtocol&& Message);

	template<
	    typename V,
	    typename... ArgTypes
	        UE_REQUIRES(std::is_constructible_v<V, ArgTypes...>)>
	void Send(ArgTypes&&... Args)
	{
		Send(FAttoS2CProtocol{TInPlaceType<V>(), Forward<ArgTypes>(Args)...});
	}

	void operator()(const FAttoLoginRequest& Message);

	void operator()(const FAttoLogoutRequest& Message);

	void operator()(const FAttoCreateSessionRequest& Message);

	void operator()(const FAttoDestroySessionRequest& Message);

	void operator()(const FAttoFindSessionsRequest& Message);

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
