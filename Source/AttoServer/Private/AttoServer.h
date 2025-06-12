#pragma once

#include "AttoCommon.h"
#include "AttoProtocol.h"

class FAttoServer;
struct lws;

struct FAttoMessage final
{
	TArray<unsigned char> Payload;
	bool bIsBinary = false;
};

class FAttoConnection final : public FNoncopyable
{
	lws* const LwsConnection;

	/** User id of currently logged-in user (if any) */
	TOptional<uint64> UserId;

	TQueue<FAttoMessage> SendQueue;

	void Send(const void* Data, const size_t Size, bool bIsBinary);

public:
	explicit FAttoConnection(lws* LwsConnection)
	    : LwsConnection(LwsConnection)
	{
	}

	void Send(FAttoS2CProtocol&& Message);

	template<
	    typename V,
	    typename... ArgTypes
	        UE_REQUIRES(std::is_constructible_v<V, ArgTypes...>)>
	void Send(ArgTypes&&... Args)
	{
		Send(FAttoS2CProtocol{TInPlaceType<V>(), Forward<ArgTypes>(Args)...});
	}

	void SendFromQueueInternal();

	void operator()(const FAttoLoginRequest& Message);

	void operator()(const FAttoLogoutRequest& Message);
};

class FAttoServerInstance final : FNoncopyable
{
	typedef FAttoServerInstance ThisClass;

	friend class FAttoServer;

	struct lws_context* Context = nullptr;

	struct lws_vhost* Vhost = nullptr;

	struct lws_protocols* Protocols;

	FTSTicker::FDelegateHandle TickerHandle;

	explicit FAttoServerInstance(const FAttoServer& Config);

	bool Tick(float DeltaSeconds);

public:
	TSet<FAttoConnection*> Connections;

	~FAttoServerInstance();
};

class FAttoServer final
{
	friend class FAttoServerInstance;

	TOptional<FString> BindAddress;
	uint32 ReceiveBufferSize = 65536;
	uint32 ListenPort = Atto::DefaultPort;

public:
	[[nodiscard]] FAttoServer& WithBindAddress(FString InBindAddress) &&
	{
		BindAddress = MoveTemp(InBindAddress);
		return *this;
	}

	[[nodiscard]] FAttoServer& WithReceiveBufferSize(const uint32 InReceiveBufferSize) &&
	{
		ReceiveBufferSize = InReceiveBufferSize;
		return *this;
	}

	[[nodiscard]] FAttoServer& WithListenPort(const uint32 InListenPort) &&
	{
		ListenPort = InListenPort;
		return *this;
	}

	[[nodiscard]] FAttoServer& WithCommandLineOptions() &&;

	[[nodiscard]] TUniquePtr<FAttoServerInstance> Create() const;
};
