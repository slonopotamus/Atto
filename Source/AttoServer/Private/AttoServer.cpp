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

FAttoServerInstance::~FAttoServerInstance()
{
	if (Vhost)
	{
		lws_vhost_destroy(Vhost);
		Vhost = nullptr;
	}

	if (Context)
	{
		lws_context_destroy(Context);
		Context = nullptr;
	}
}

static int AttoServerCallback(lws* Wsi, const lws_callback_reasons Reason, void* User, void* In, const size_t Len)
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

TUniquePtr<FAttoServerInstance> FAttoServer::Listen(const uint32 Port) const
{
	auto Result = MakeUnique<FAttoServerInstance>();

	const auto ProtocolName = StringCast<ANSICHAR>(Atto::Protocol);
	const lws_protocols Protocols[] = {
	    {
	        .name = ProtocolName.Get(),
	        .callback = AttoServerCallback,
	        .per_session_data_size = sizeof(FAttoServerPerSessionData),
	        .rx_buffer_size = 1 * 1024 * 1024,
	        .user = Result.Get(),
	    },
	    {},
	};

	const auto BindAddressAnsi = StringCast<ANSICHAR>(*BindAddress.Get(TEXT("")));
	lws_context_creation_info ContextCreationInfo = {
	    .port = static_cast<int>(Port),
	    .iface = BindAddress ? BindAddressAnsi.Get() : nullptr,
	    .protocols = Protocols,
	    .gid = -1,
	    .uid = -1,
	    .options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS,
	    .user = Result.Get(),
	};

	Result->Context = lws_create_context(&ContextCreationInfo);
	if (!Result->Context)
	{
		// TODO: Log error
		return nullptr;
	}

	Result->Vhost = lws_create_vhost(Result->Context, &ContextCreationInfo);
	if (!Result->Vhost)
	{
		// TODO: Log error
		return nullptr;
	}

	const auto PortActual = lws_get_vhost_listen_port(Result->Vhost);
	UE_LOG(LogAtto, Log, TEXT("Atto Server started accepting connections on %s:%d"), StringCast<TCHAR>(BindAddressAnsi.Get()).Get(), PortActual)

	return Result;
}
