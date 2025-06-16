#pragma once

#include "AttoCommon.h"
#include "AttoProtocol.h"

class FAttoConnection;
class FAttoServer;

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
	const int32 MaxFindSessionsResults;

	TSet<FAttoConnection*> Connections;

	TMap<uint64, FAttoSessionInfo> Sessions;

	~FAttoServerInstance();
};

class FAttoServer final
{
	friend class FAttoServerInstance;

	TOptional<FString> BindAddress;
	uint32 ReceiveBufferSize = 65536;
	uint32 ListenPort = Atto::DefaultPort;
	int32 MaxFindSessionsResults = 100;

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
