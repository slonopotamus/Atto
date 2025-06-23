#include "AttoClient.h"
#include "AttoCommon.h"
#include "AttoProtocol.h"
#include "OnlineSessionSettings.h"
#include "WebSocketsModule.h"

FAttoClient::FAttoClient(const FString& Url)
    : WebSocket{FWebSocketsModule::Get().CreateWebSocket(Url, Atto::Protocol)}
{
	WebSocket->OnConnected().AddLambda([=, this] {
		for (const auto& Promise : ConnectPromises)
		{
			Promise->SetValue(true);
		}

		ConnectPromises.Empty();
	});

	WebSocket->OnConnectionError().AddLambda([=, this](const FString& Error) {
		// TODO: Log error?

		for (const auto& Promise : ConnectPromises)
		{
			Promise->SetValue(false);
		}

		ConnectPromises.Empty();
	});

	WebSocket->OnClosed().AddLambda([=, this](int32 StatusCode, const FString& Reason, const bool bWasClean) {
		for (const auto& [_, Promise] : RequestPromises)
		{
			Promise->HandleDisconnect();
		}

		RequestPromises.Empty();
		OnDisconnected.Broadcast(Reason, bWasClean);
	});

	WebSocket->OnBinaryMessage().AddLambda([=, this](const void* Data, const size_t Size, const bool bIsLastFragment) {
		if (!ensure(bIsLastFragment))
		{
			return;
		}

		FBitReader Ar{static_cast<const uint8*>(Data), static_cast<int64>(Size * 8)};

		int64 RequestId = 0;
		Ar << RequestId;
		FAttoS2CProtocol Message;
		Ar << Message;

		if (ensure(!Ar.IsError()))
		{
			if (TUniquePtr<IRequestPromise> RequestPromise; ensure(RequestPromises.RemoveAndCopyValue(RequestId, RequestPromise)))
			{
				RequestPromise->Handle(MoveTemp(Message));
			}
		}
	});
}

FAttoClient::~FAttoClient()
{
	WebSocket->Close();

	for (const auto& Promise : ConnectPromises)
	{
		Promise->SetValue(false);
	}
}

TFuture<bool> FAttoClient::ConnectAsync()
{
	if (IsConnected())
	{
		return MakeFulfilledPromise<bool>(true).GetFuture();
	}

	const auto Promise = ConnectPromises.Add_GetRef(MakeShared<TPromise<bool>>());
	WebSocket->Connect();
	return Promise->GetFuture();
}

void FAttoClient::Disconnect()
{
	WebSocket->Close();
}

void FAttoClient::Send(int64 RequestId, FAttoC2SProtocol&& Message)
{
	// TODO: Store writer between calls to reduce allocations?
	FBitWriter Ar{0, true};

	Ar << RequestId;
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

TFuture<UE::Online::TOnlineResult<FAttoFindSessionsRequest>> FAttoClient::FindSessionsAsync(const FOnlineSessionSearch& Search)
{
	TMap<FName, FAttoFindSessionsParam> Params;

	for (const auto& [Name, Value] : Search.QuerySettings.SearchParams)
	{
		if (const auto& ParamValue = ConvertVariantData(Value.Data))
		{
			Params.Add(Name, FAttoFindSessionsParam{Value.ComparisonOp, *ParamValue, Value.ID});
		}
	}

	return Send<FAttoFindSessionsRequest>(MoveTemp(Params), Search.PlatformHash, Search.MaxSearchResults);
}
