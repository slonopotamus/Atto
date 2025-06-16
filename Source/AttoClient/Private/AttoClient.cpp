#include "AttoClient.h"
#include "AttoCommon.h"
#include "AttoProtocol.h"
#include "OnlineSessionSettings.h"
#include "WebSocketsModule.h"

FAttoClient::FAttoClient(const FString& Url)
    : WebSocket{FWebSocketsModule::Get().CreateWebSocket(Url, Atto::Protocol)}
{
	WebSocket->OnConnected().AddLambda([&] {
		OnConnected.Broadcast();
	});

	WebSocket->OnClosed().AddLambda([&](int32 StatusCode, const FString& Reason, const bool bWasClean) { OnDisconnected.Broadcast(Reason, bWasClean); });

	WebSocket->OnConnectionError().AddLambda([&](const FString& Error) { OnConnectionError.Broadcast(Error); });

	WebSocket->OnBinaryMessage().AddLambda([&](const void* Data, const size_t Size, const bool bIsLastFragment) {
		if (!ensure(bIsLastFragment))
		{
			return;
		}

		FBitReader Ar{static_cast<const uint8*>(Data), static_cast<int64>(Size * 8)};

		FAttoS2CProtocol Message;
		Ar << Message;

		if (ensure(!Ar.IsError()))
		{
			Visit([&](auto& Variant) { this->operator()(Variant); }, Message);
		}
	});
}

void FAttoClient::ConnectAsync()
{
	WebSocket->Connect();
}

void FAttoClient::Disconnect()
{
	WebSocket->Close();
}

void FAttoClient::Send(FAttoC2SProtocol&& Message)
{
	// TODO: Store writer between calls to reduce allocations?
	FBitWriter Ar{0, true};

	Ar << Message;

	if (ensure(!Ar.IsError()))
	{
		WebSocket->Send(Ar.GetData(), Ar.GetNumBytes(), true);
	}
}

bool FAttoClient::IsConnected() const
{
	return WebSocket->IsConnected();
}

void FAttoClient::LoginAsync(FString Username, FString Password)
{
	Send<FAttoLoginRequest>(MoveTemp(Username), MoveTemp(Password));
}

void FAttoClient::LogoutAsync()
{
	Send<FAttoLogoutRequest>();
}

template<typename T>
static TOptional<FAttoFindSessionsParamValue> GetValueFromVariantData(const FVariantData& Data)
{
	T Result{};
	Data.GetValue(Result);
	return {FAttoFindSessionsParamValue{TInPlaceType<T>{}, MoveTemp(Result)}};
}

// FVariantData is just brain-damaged
static TOptional<FAttoFindSessionsParamValue> ConvertVariantData(const FVariantData& Data)
{
	switch (Data.GetType())
	{
		case EOnlineKeyValuePairDataType::Empty:
			// TODO: How to handle this?
			return {};
		case EOnlineKeyValuePairDataType::Int32:
			return GetValueFromVariantData<int32>(Data);
		case EOnlineKeyValuePairDataType::UInt32:
			return GetValueFromVariantData<uint32>(Data);
		case EOnlineKeyValuePairDataType::Int64:
			return GetValueFromVariantData<int64>(Data);
		case EOnlineKeyValuePairDataType::UInt64:
			return GetValueFromVariantData<uint64>(Data);
		case EOnlineKeyValuePairDataType::Double:
			return GetValueFromVariantData<double>(Data);
		case EOnlineKeyValuePairDataType::String:
			return GetValueFromVariantData<FString>(Data);
		case EOnlineKeyValuePairDataType::Float:
			return GetValueFromVariantData<float>(Data);
		case EOnlineKeyValuePairDataType::Blob:
			return GetValueFromVariantData<TArray<uint8>>(Data);
		case EOnlineKeyValuePairDataType::Bool:
			return GetValueFromVariantData<bool>(Data);
		case EOnlineKeyValuePairDataType::Json:
			// TODO: Add support
			ensureMsgf(false, TEXT("JSON is not supported"));
			return {};
		case EOnlineKeyValuePairDataType::MAX: [[fallthrough]];
		default:
			ensureMsgf(false, TEXT("Unknown EOnlineKeyValuePairDataType: %d"), Data.GetType());
			return {};
	}
}

void FAttoClient::FindSessionsAsync(const FOnlineSessionSearch& Search)
{
	TMap<FName, FAttoFindSessionsParam> Params;

	for (const auto& [Name, Value] : Search.QuerySettings.SearchParams)
	{
		if (const auto& ParamValue = ConvertVariantData(Value.Data))
		{
			Params.Add(Name, FAttoFindSessionsParam{Value.ComparisonOp, *ParamValue, Value.ID});
		}
	}

	FindSessionsAsync(MoveTemp(Params), Search.PlatformHash, Search.MaxSearchResults);
}

void FAttoClient::FindSessionsAsync(TMap<FName, FAttoFindSessionsParam>&& Params, int32 RequestId, int32 MaxResults)
{
	Send<FAttoFindSessionsRequest>(MoveTemp(Params), RequestId, MaxResults);
}

void FAttoClient::CreateSessionAsync(const FAttoSessionInfo& SessionInfo)
{
	Send<FAttoCreateSessionRequest>(SessionInfo);
}

void FAttoClient::UpdateSessionAsync(uint64 SessionId, const FAttoSessionUpdatableInfo& SessionInfo)
{
	Send<FAttoUpdateSessionRequest>(SessionId, SessionInfo);
}

void FAttoClient::DestroySessionAsync()
{
	Send<FAttoDestroySessionRequest>();
}

void FAttoClient::QueryServerUtcTimeAsync()
{
	Send<FAttoQueryServerUtcTimeRequest>();
}

void FAttoClient::operator()(const FAttoLoginResponse& Message)
{
	OnLoginResponse.Broadcast(Message);
}

void FAttoClient::operator()(const FAttoLogoutResponse& Message)
{
	OnLogoutResponse.Broadcast();
}

void FAttoClient::operator()(const FAttoCreateSessionResponse& Message)
{
	OnCreateSessionResponse.Broadcast(Message.bSuccess);
}

void FAttoClient::operator()(const FAttoUpdateSessionResponse& Message)
{
	OnUpdateSessionResponse.Broadcast(Message.bSuccess);
}

void FAttoClient::operator()(const FAttoDestroySessionResponse& Message)
{
	OnDestroySessionResponse.Broadcast(Message.bSuccess);
}

void FAttoClient::operator()(const FAttoFindSessionsResponse& Message)
{
	OnFindSessionsResponse.Broadcast(Message);
}

void FAttoClient::operator()(const FAttoQueryServerUtcTimeResponse& Message)
{
	OnServerUtcTime.Broadcast(Message.ServerTime);
}
