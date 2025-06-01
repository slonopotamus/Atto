#pragma once

#include "AttoCommon.h"

class FAttoServerInstance final : FNoncopyable
{
	friend class FAttoServer;

	struct lws_context* Context = nullptr;

	struct lws_vhost* Vhost = nullptr;

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
