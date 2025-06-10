#pragma once

#include "AttoCommon.h"
#include <span>

class FAttoServer;
struct lws;

enum class EAttoMessageType : uint8
{
	Text,
	Binary,
};

struct FAttoMessage final
{
	TArray<unsigned char> Payload;
	EAttoMessageType Type = EAttoMessageType::Binary;
};

class FAttoConnection final : public FNoncopyable
{
	lws* const LwsConnection;

	TQueue<FAttoMessage> SendQueue;

public:
	explicit FAttoConnection(lws* LwsConnection)
	    : LwsConnection(LwsConnection)
	{
	}

	void Send(const FString& Message);

	void Send(const void* Data, const size_t Size, EAttoMessageType Type);

	void SendFromQueueInternal();

	void HandleMessageInternal(const void* Data, const size_t Size);
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
