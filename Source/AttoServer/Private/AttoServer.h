#pragma once

#include "AttoCommon.h"

class FAttoServer;

class FAttoServerInstance final : FNoncopyable
{
	typedef FAttoServerInstance ThisClass;

	friend class FAttoServer;

	struct lws_context* Context = nullptr;

	struct lws_vhost* Vhost = nullptr;

	struct lws_protocols* Protocols;

	FTSTicker::FDelegateHandle TickerHandle;

	FAttoServerInstance();

	bool Tick(float DeltaSeconds);

public:
	~FAttoServerInstance();
};

class FAttoServer final
{
	TOptional<FString> BindAddress;

public:
	[[nodiscard]] FAttoServer& WithBindAddress(FString InBindAddress) &&
	{
		BindAddress = MoveTemp(InBindAddress);
		return *this;
	}

	[[nodiscard]] TUniquePtr<FAttoServerInstance> Listen(uint32 Port = Atto::DefaultPort) const;
};
