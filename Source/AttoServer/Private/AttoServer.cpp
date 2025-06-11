#include "AttoServer.h"
#include "AttoCommon.h"
#include "Containers/BackgroundableTicker.h"

// Work around a conflict between a UI namespace defined by engine code and a typedef in OpenSSL
#define UI UI_ST
THIRD_PARTY_INCLUDES_START
#include "libwebsockets.h"
THIRD_PARTY_INCLUDES_END
#undef UI

void FAttoConnection::Send(const void* Data, const size_t Size, const EAttoMessageType Type)
{
	if (!ensure(Data))
	{
		return;
	}

	TArray<unsigned char> Payload;

	Payload.Reserve(LWS_PRE + Size);
	Payload.AddDefaulted(LWS_PRE);
	Payload.Append(static_cast<const unsigned char*>(Data), Size);

	SendQueue.Enqueue({
	    .Payload = MoveTemp(Payload),
	    .Type = Type,
	});

	lws_callback_on_writable(LwsConnection);
}

void FAttoConnection::Send(const FString& Message)
{
	const FTCHARToUTF8 Converted{Message};
	Send(Converted.Get(), Converted.Length(), EAttoMessageType::Text);
}

void FAttoConnection::SendFromQueueInternal()
{
	if (auto* Message = SendQueue.Peek(); ensure(Message))
	{
		const auto Mode = Message->Type == EAttoMessageType::Text ? LWS_WRITE_TEXT : LWS_WRITE_BINARY;
		const auto Size = Message->Payload.Num() - LWS_PRE;
		// TODO: Implement partial write
		ensure(lws_write(LwsConnection, Message->Payload.GetData() + LWS_PRE, Size, Mode) == Size);
		SendQueue.Pop();
	}

	if (!SendQueue.IsEmpty())
	{
		lws_callback_on_writable(LwsConnection);
	}
}

void FAttoConnection::HandleMessageInternal(const void* Data, const size_t Size)
{
	if (lws_frame_is_binary(LwsConnection))
	{
		// TODO
	}
	else
	{
		const FUTF8ToTCHAR Converted{static_cast<const ANSICHAR*>(Data), static_cast<int32>(Size)};
		const FString Message{Converted.Length(), Converted.Get()};
		if (ensureMsgf(Message == TEXT("Ping"), TEXT("Expected 'Ping' but got '%s'"), *Message))
		{
			Send(TEXT("Pong"));
		}
	}
}

static int AttoServerCallback(lws* LwsConnection, const lws_callback_reasons Reason, void* User, void* In, const size_t Len)
{
	if (const lws_protocols* Protocol = lws_get_protocol(LwsConnection))
	{
		auto* Session = static_cast<FAttoConnection*>(User);

		if (auto* Server = static_cast<FAttoServerInstance*>(Protocol->user); ensure(Server))
		{
			switch (Reason)
			{
				case LWS_CALLBACK_ESTABLISHED:
				{
					if (ensure(Session))
					{
						Server->Connections.Add(new (Session) FAttoConnection{LwsConnection});
					}
					break;
				}

				case LWS_CALLBACK_CLOSED:
				{
					if (ensure(Session))
					{
						Server->Connections.Remove(Session);
						Session->~FAttoConnection();
					}

					break;
				}

				case LWS_CALLBACK_RECEIVE:
				{
					// TODO: Implement partial read
					if (ensure(lws_is_final_fragment(LwsConnection)))
					{
						Session->HandleMessageInternal(In, Len);
					}
					break;
				}

				case LWS_CALLBACK_SERVER_WRITEABLE:
				{
					if (ensure(Session))
					{
						Session->SendFromQueueInternal();
					}
					break;
				}
			}
		}
	}

	return lws_callback_http_dummy(LwsConnection, Reason, User, In, Len);
}

FAttoServerInstance::FAttoServerInstance(const FAttoServer& Config)
    : Protocols{new lws_protocols[2]}
{
	Protocols[0] = {
	    .name = Atto::Protocol,
	    .callback = AttoServerCallback,
	    .per_session_data_size = sizeof(FAttoConnection),
	    .rx_buffer_size = Config.ReceiveBufferSize,
	    .user = this,
	};
	Protocols[1] = {};

	// TODO: Move libwebsocket activity off game thread and only do message handling on it?
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

	delete[] Protocols;
	Protocols = nullptr;
}

bool FAttoServerInstance::Tick(float DeltaSeconds)
{
	lws_service(Context, 0);
	return true;
}

FAttoServer& FAttoServer::WithCommandLineOptions() &&
{
	// TODO: Also make client port respect this
	FParse::Value(FCommandLine::Get(), TEXT("AttoPort"), ListenPort);
	return *this;
}

TUniquePtr<FAttoServerInstance> FAttoServer::Create() const
{
	auto Result = TUniquePtr<FAttoServerInstance>(new FAttoServerInstance{*this});

	const auto BindAddressAnsi = StringCast<ANSICHAR>(*BindAddress.Get(TEXT("")));
	const lws_context_creation_info ContextCreationInfo = {
	    .port = static_cast<int>(ListenPort),
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
