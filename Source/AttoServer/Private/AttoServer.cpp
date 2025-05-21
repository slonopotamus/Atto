#include "AttoServer.h"

#include "AttoCommon.h"

// Work around a conflict between a UI namespace defined by engine code and a typedef in OpenSSL
#define UI UI_ST
THIRD_PARTY_INCLUDES_START
#include "libwebsockets.h"
THIRD_PARTY_INCLUDES_END
#undef UI

struct FAttoServerPerSessionData
{
};

static int AttoServerCallback(lws* Wsi, const lws_callback_reasons Reason, void* User, void* In, size_t Len)
{
	switch (Reason)
	{
		case LWS_CALLBACK_ESTABLISHED:
		{
			lws_set_timeout(Wsi, NO_PENDING_TIMEOUT, 0);
			break;
		}

		case LWS_CALLBACK_RECEIVE:
		{
			break;
		}

		case LWS_CALLBACK_SERVER_WRITEABLE:
		{
			break;
		}

		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		{
			break;
		}

		case LWS_CALLBACK_CLOSED:
		{
			break;
		}
	}

	return lws_callback_http_dummy(Wsi, Reason, User, In, Len);
}

FAttoServer::FAttoServer()
{
	Protocols = new lws_protocols[2];
	FMemory::Memzero(Protocols, sizeof(lws_protocols) * 2);

	Protocols[0].name = StringCast<ANSICHAR>(Atto::Protocol).Get();
	Protocols[0].callback = AttoServerCallback;
	Protocols[0].per_session_data_size = sizeof(FAttoServerPerSessionData);
	Protocols[0].rx_buffer_size = 1 * 1024 * 1024;
}

FAttoServer::~FAttoServer()
{
	if (Context)
	{
		lws_context_destroy(Context);
		Context = nullptr;

		UE_LOG(LogAtto, Log, TEXT("Atto Server was shut down"));
	}

	delete[] Protocols;
	Protocols = nullptr;
}

bool FAttoServer::Listen(const uint32 Port, const FString& BindAddress)
{
	if (!ensure(!Context))
	{
		return false;
	}

	lws_context_creation_info Info;
	FMemory::Memzero(&Info, sizeof(decltype(Info)));

	Info.iface = BindAddress.IsEmpty() ? nullptr : StringCast<ANSICHAR>(*BindAddress).Get();
	Info.port = Port;
	Info.gid = -1;
	Info.uid = -1;
	Info.user = this;

	Context = lws_create_context(&Info);

	UE_LOG(LogAtto, Log, TEXT("Atto Server started accepting connections on %s:%d"), *BindAddress, Port)

	return Context != nullptr;
}
