#include "AttoServer.h"

#include "AttoCommon.h"
#include "Containers/BackgroundableTicker.h"

// Work around a conflict between a UI namespace defined by engine code and a typedef in OpenSSL
#define UI UI_ST
THIRD_PARTY_INCLUDES_START
#include "libwebsockets.h"
THIRD_PARTY_INCLUDES_END
#undef UI

struct FAttoServerSession
{
};

static int AttoServerCallback(lws* Wsi, const lws_callback_reasons Reason, void* User, void* In, const size_t Len)
{
	FAttoServerSession* Session = static_cast<FAttoServerSession*>(User);
	const lws_protocols* Protocol = lws_get_protocol(Wsi);
	FAttoServerInstance* Server = static_cast<FAttoServerInstance*>(Protocol->user);
	if (ensure(Session) && ensure(Server))
	{
		switch (Reason)
		{
			case LWS_CALLBACK_ESTABLISHED:
			{
				break;
			}

			case LWS_CALLBACK_CLOSED:
			{
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
		}
	}

	return lws_callback_http_dummy(Wsi, Reason, User, In, Len);
}

FAttoServerInstance::FAttoServerInstance()
{
	Protocols = new lws_protocols[2];
	Protocols[0] = {
	    .name = Atto::Protocol,
	    .callback = AttoServerCallback,
	    .per_session_data_size = sizeof(FAttoServerSession),
	    .rx_buffer_size = 65536,
	    .user = this,
	};
	Protocols[1] = {};

	TickerHandle = FTSBackgroundableTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &ThisClass::Tick));
}

FAttoServerInstance::~FAttoServerInstance()
{
	FTSBackgroundableTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	TickerHandle.Reset();

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

	delete Protocols;
	Protocols = nullptr;
}

bool FAttoServerInstance::Tick(float DeltaSeconds)
{
	lws_service(Context, 0);
	return true;
}

TUniquePtr<FAttoServerInstance> FAttoServer::Listen(const uint32 Port) const
{
	auto Result = TUniquePtr<FAttoServerInstance>(new FAttoServerInstance{});

	const auto BindAddressAnsi = StringCast<ANSICHAR>(*BindAddress.Get(TEXT("")));
	const lws_context_creation_info ContextCreationInfo = {
	    .port = static_cast<int>(Port),
	    .iface = BindAddress ? BindAddressAnsi.Get() : nullptr,
	    .protocols = Result->Protocols,
	    .gid = -1,
	    .uid = -1,
	    .options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS,
	};

	Result->Context = lws_create_context(&ContextCreationInfo);
	if (!ensure(Result->Context))
	{
		return nullptr;
	}

	Result->Vhost = lws_create_vhost(Result->Context, &ContextCreationInfo);
	if (!ensure(Result->Vhost))
	{
		return nullptr;
	}

	const auto PortActual = lws_get_vhost_listen_port(Result->Vhost);
	UE_LOG(LogAtto, Log, TEXT("Atto Server started accepting connections on %s:%d"), StringCast<TCHAR>(BindAddressAnsi.Get()).Get(), PortActual);

	return Result;
}
