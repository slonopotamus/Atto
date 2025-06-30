#pragma once

#include "AttoProtocol.h"
#include "IWebSocket.h"
#include "Online/OnlineError.h"
#include "Online/OnlineErrorDefinitions.h"
#include "Online/OnlineResult.h"

class FOnlineSessionSearch;

class ATTOCLIENT_API FAttoClient final : FNoncopyable
{
	typedef FAttoClient ThisClass;

	TSharedRef<IWebSocket> WebSocket;

	void Send(int64 RequestId, FAttoClientRequestProtocol&& Message);

	struct IRequestPromise : FNoncopyable
	{
		virtual ~IRequestPromise() = default;

		virtual void Handle(FAttoServerResponseProtocol&& Response) = 0;

		virtual void HandleDisconnect() = 0;
	};

	template<typename T>
	struct TRequestPromise final : IRequestPromise
	{
		TSharedPtr<TPromise<UE::Online::TOnlineResult<T>>> Promise = MakeShared<TPromise<UE::Online::TOnlineResult<T>>>();

		virtual void Handle(FAttoServerResponseProtocol&& Response) override
		{
			if (ensure(Promise))
			{
				if (typename T::Result* Result = Response.TryGet<typename T::Result>(); ensure(Result))
				{
					Promise->SetValue(UE::Online::TOnlineResult<T>{MoveTemp(*Result)});
				}
				else
				{
					Promise->SetValue(
					    UE::Online::TOnlineResult<T>{
					        UE::Online::FOnlineError{
					            UE::Online::Errors::ErrorCode::Common::CantParse}});
				}
			}

			Promise = nullptr;
		}

		virtual void HandleDisconnect() override
		{
			if (Promise)
			{
				Promise->SetValue(
				    UE::Online::TOnlineResult<T>{
				        UE::Online::FOnlineError{
				            UE::Online::Errors::ErrorCode::Common::RequestFailure}});
			}

			Promise = nullptr;
		}

		virtual ~TRequestPromise() override
		{
			if (Promise)
			{
				Promise->SetValue(
				    UE::Online::TOnlineResult<T>{
				        UE::Online::FOnlineError{
				            UE::Online::Errors::ErrorCode::Common::Cancelled}});
			}

			Promise = nullptr;
		}
	};

	TArray<TSharedRef<TPromise<bool>>> ConnectPromises;

	TMap<int64, TUniquePtr<IRequestPromise>> RequestPromises;

	int64 LastRequestId = 0;

public:
	explicit FAttoClient(const FString& Url);

	~FAttoClient();

	TFuture<bool> ConnectAsync();

	[[nodiscard]] bool IsConnected() const;

	void Disconnect();

	DECLARE_EVENT_TwoParams(FAttoClient, FAttoDisconnectedEvent, const FString& /* Reason */, bool /* bWasClean */);
	FAttoDisconnectedEvent OnDisconnected;

	DECLARE_EVENT_OneParam(FAttoClient, FAttoServerPushEvent, const FAttoServerPushProtocol& /* Message */);
	FAttoServerPushEvent OnServerPush;

	template<
	    typename T,
	    typename... ArgTypes
	        UE_REQUIRES(std::is_constructible_v<T, ArgTypes...>)>
	TFuture<UE::Online::TOnlineResult<T>> Send(ArgTypes&&... Args)
	{
		if (!IsConnected())
		{
			return MakeFulfilledPromise<UE::Online::TOnlineResult<T>>(
			           UE::Online::FOnlineError{
			               UE::Online::Errors::ErrorCode::Common::NoConnection})
			    .GetFuture();
		}

		auto Promise = MakeUnique<TRequestPromise<T>>();
		auto Future = Promise->Promise->GetFuture();
		const auto RequestId = ++LastRequestId;
		RequestPromises.Add(RequestId, MoveTemp(Promise));
		Send(RequestId, FAttoClientRequestProtocol{TInPlaceType<T>(), Forward<ArgTypes>(Args)...});
		return Future;
	}

	TFuture<UE::Online::TOnlineResult<FAttoFindSessionsRequest>> FindSessionsAsync(const FOnlineSessionSearch& Search);
};
