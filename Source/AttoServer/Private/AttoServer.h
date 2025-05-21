#pragma once

struct lws_context;
struct lws_protocols;

class FAttoServer final
{
	lws_context* Context = nullptr;

	lws_protocols* Protocols = nullptr;

public:
	FAttoServer();
	~FAttoServer();

	bool Listen(uint32 Port, const FString& BindAddress = TEXT(""));
};
