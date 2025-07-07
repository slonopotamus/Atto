#pragma once

#include "OnlineSessionSettings.h"

struct ATTOCOMMON_API FAttoLoginRequest
{
	FString Username;

	FString Password;

	// TODO: Handle this during WS upgrade procedure?
	int32 BuildUniqueId = 0;

	friend FArchive& operator<<(FArchive& Ar, FAttoLoginRequest& Message)
	{
		Ar << Message.Username;
		Ar << Message.Password;
		Ar << Message.BuildUniqueId;

		return Ar;
	}

	using Result = TVariant<uint64 /* UserId */, FString /* Error */>;
};

struct ATTOCOMMON_API FAttoLogoutRequest
{
	uint64 UserId = 0;

	friend FArchive& operator<<(FArchive& Ar, FAttoLogoutRequest& Message)
	{
		Ar << Message.UserId;
		return Ar;
	}

	struct ATTOCOMMON_API Result
	{
		bool bSuccess = false;

		friend FArchive& operator<<(FArchive& Ar, Result& Message)
		{
			Ar.SerializeBits(&Message.bSuccess, 1);
			return Ar;
		}
	};
};

using FAttoSessionSettingValue = TVariant<bool, int32, uint32, int64, uint64, double, FString, float, TArray<uint8>>;

struct ATTOCOMMON_API FAttoSessionUpdatableInfo
{
	int32 NumOpenPublicConnections = 0;

	int32 NumPublicConnections = 0;

	EOnlineSessionState::Type State;

	bool bAllowJoinInProgress = false;

	bool bShouldAdvertise = false;

	FAttoSessionUpdatableInfo() = default;

	explicit FAttoSessionUpdatableInfo(const FNamedOnlineSession& OnlineSession)
	    : NumOpenPublicConnections{OnlineSession.NumOpenPublicConnections}
	    , NumPublicConnections{OnlineSession.SessionSettings.NumPublicConnections}
	    , State{OnlineSession.SessionState}
	    , bAllowJoinInProgress{OnlineSession.SessionSettings.bAllowJoinInProgress}
	    , bShouldAdvertise{OnlineSession.SessionSettings.bShouldAdvertise}
	{}

	friend FArchive& operator<<(FArchive& Ar, FAttoSessionUpdatableInfo& Message)
	{
		Ar << Message.NumOpenPublicConnections;
		Ar << Message.NumPublicConnections;

		int32 StateValue = Message.State;
		Ar << StateValue;
		Message.State = static_cast<EOnlineSessionState::Type>(StateValue);

		Ar.SerializeBits(&Message.bAllowJoinInProgress, 1);
		Ar.SerializeBits(&Message.bShouldAdvertise, 1);

		return Ar;
	}

	void CopyTo(FOnlineSession& OnlineSession) const
	{
		OnlineSession.NumOpenPublicConnections = NumOpenPublicConnections;
		OnlineSession.SessionSettings.NumPublicConnections = NumPublicConnections;
		OnlineSession.SessionSettings.bAllowJoinInProgress = bAllowJoinInProgress;
		OnlineSession.SessionSettings.bShouldAdvertise = bShouldAdvertise;
	}
};

struct ATTOCOMMON_API FAttoFindSessionsParam
{
	EOnlineComparisonOp::Type ComparisonOp = EOnlineComparisonOp::Equals;
	FAttoSessionSettingValue Value{TInPlaceType<bool>{}, false};
	int32 ID = 0;

	friend FArchive& operator<<(FArchive& Ar, FAttoFindSessionsParam& Message)
	{
		int32 ComparisonOpValue = Message.ComparisonOp;
		Ar << ComparisonOpValue;
		Message.ComparisonOp = static_cast<EOnlineComparisonOp::Type>(ComparisonOpValue);

		Ar << Message.Value;
		Ar << Message.ID;
		return Ar;
	}
};

struct ATTOCOMMON_API FAttoSessionAddress
{
	TArray<uint8> Host;
	int32 Port = 0;

	FAttoSessionAddress() = default;

	explicit FAttoSessionAddress(const FInternetAddr& Addr)
	    : Host{Addr.GetRawIp()}
	    , Port{Addr.GetPort()}
	{}

	friend FArchive& operator<<(FArchive& Ar, FAttoSessionAddress& Message)
	{
		Ar << Message.Host;
		Ar << Message.Port;
		return Ar;
	}

	TSharedRef<FInternetAddr> ToInternetAddr() const;
};

struct ATTOCOMMON_API FAttoSessionInfo
{
	uint64 SessionId = 0;

	FAttoSessionAddress HostAddress;

	int32 BuildUniqueId = 0;

	TMap<FString, FAttoSessionSettingValue> Settings;

	FAttoSessionUpdatableInfo UpdatableInfo;

	// TODO: We could skip sending this when creating session and instead determine whether connection is from dedicated server during Login
	bool bIsDedicated = false;

	bool bAntiCheatProtected = false;

	void CopyTo(FOnlineSession& OnlineSession) const
	{
		OnlineSession.SessionSettings.BuildUniqueId = BuildUniqueId;
		for (const auto& [SettingName, SettingValue] : Settings)
		{
			Visit(
			    [&](const auto& Value) {
				    OnlineSession.SessionSettings.Set(FName{SettingName}, Value);
			    },
			    SettingValue);
		}
		UpdatableInfo.CopyTo(OnlineSession);
		OnlineSession.SessionSettings.bIsDedicated = bIsDedicated;
		OnlineSession.SessionSettings.bAntiCheatProtected = bAntiCheatProtected;
	}

	friend FArchive& operator<<(FArchive& Ar, FAttoSessionInfo& Message)
	{
		Ar << Message.SessionId;
		Ar << Message.HostAddress;
		Ar << Message.BuildUniqueId;
		Ar << Message.Settings;
		Ar << Message.UpdatableInfo;
		Ar << Message.bIsDedicated;
		Ar << Message.bAntiCheatProtected;
		return Ar;
	}
};

struct ATTOCOMMON_API FAttoCreateSessionRequest
{
	uint64 OwningUserId = 0;

	FAttoSessionInfo SessionInfo;

	friend FArchive& operator<<(FArchive& Ar, FAttoCreateSessionRequest& Message)
	{
		Ar << Message.OwningUserId;
		Ar << Message.SessionInfo;
		return Ar;
	}

	struct ATTOCOMMON_API Result
	{
		bool bSuccess = false;

		friend FArchive& operator<<(FArchive& Ar, Result& Message)
		{
			Ar.SerializeBits(&Message.bSuccess, 1);
			return Ar;
		}
	};
};

struct ATTOCOMMON_API FAttoUpdateSessionRequest
{
	uint64 OwningUserId = 0;
	uint64 SessionId = 0;

	FAttoSessionUpdatableInfo SessionInfo;

	friend FArchive& operator<<(FArchive& Ar, FAttoUpdateSessionRequest& Message)
	{
		Ar << Message.OwningUserId;
		Ar << Message.SessionInfo;
		return Ar;
	}

	struct ATTOCOMMON_API Result
	{
		bool bSuccess = false;

		friend FArchive& operator<<(FArchive& Ar, Result& Message)
		{
			Ar.SerializeBits(&Message.bSuccess, 1);
			return Ar;
		}
	};
};

struct ATTOCOMMON_API FAttoDestroySessionRequest
{
	uint64 OwningUserId = 0;

	friend FArchive& operator<<(FArchive& Ar, FAttoDestroySessionRequest& Message)
	{
		Ar << Message.OwningUserId;
		return Ar;
	}

	struct ATTOCOMMON_API Result
	{
		bool bSuccess = false;

		friend FArchive& operator<<(FArchive& Ar, Result& Message)
		{
			Ar.SerializeBits(&Message.bSuccess, 1);
			return Ar;
		}
	};
};

struct ATTOCOMMON_API FAttoFindSessionsRequest
{
	TMap<FString, FAttoFindSessionsParam> Params;

	uint64 SearchingUserId = 0;

	int32 RequestId = 0;

	int32 MaxResults = 0;

	friend FArchive& operator<<(FArchive& Ar, FAttoFindSessionsRequest& Message)
	{
		Ar << Message.SearchingUserId;
		Ar << Message.Params;
		Ar << Message.RequestId;
		Ar << Message.MaxResults;
		return Ar;
	}

	struct ATTOCOMMON_API Result
	{
		int32 RequestId = 0;

		TArray<FAttoSessionInfo> Sessions;

		friend FArchive& operator<<(FArchive& Ar, Result& Message)
		{
			Ar << Message.RequestId;
			Ar << Message.Sessions;

			return Ar;
		}
	};
};

struct ATTOCOMMON_API FAttoQueryServerUtcTimeRequest
{
	// Workaround to fix clang-format 19 messing up everything
	bool bDummy = false;

	friend FArchive& operator<<(FArchive& Ar, FAttoQueryServerUtcTimeRequest& Message)
	{
		return Ar;
	}

	struct ATTOCOMMON_API Result
	{
		FDateTime ServerTime;

		friend FArchive& operator<<(FArchive& Ar, Result& Message)
		{
			Ar << Message.ServerTime;
			return Ar;
		}
	};
};

struct ATTOCOMMON_API FAttoStartMatchmakingRequest
{
	TArray<uint64> Users;
	TMap<FString, FAttoFindSessionsParam> Params;
	FTimespan Timeout;

	friend FArchive& operator<<(FArchive& Ar, FAttoStartMatchmakingRequest& Message)
	{
		Ar << Message.Users;
		Ar << Message.Params;
		Ar << Message.Timeout;
		return Ar;
	}

	struct FTimeout
	{
		friend FArchive& operator<<(FArchive& Ar, FTimeout& Message)
		{
			return Ar;
		}
	};

	struct FCanceled
	{
		friend FArchive& operator<<(FArchive& Ar, FCanceled& Message)
		{
			return Ar;
		}
	};

	using Result = TVariant<FAttoSessionInfo, FTimeout, FCanceled, FString>;
};

struct ATTOCOMMON_API FAttoCancelMatchmakingRequest
{
	uint64 UserId = 0;

	friend FArchive& operator<<(FArchive& Ar, FAttoCancelMatchmakingRequest& Message)
	{
		Ar << Message.UserId;
		return Ar;
	}

	struct ATTOCOMMON_API Result
	{
		bool bSuccess = false;

		friend FArchive& operator<<(FArchive& Ar, Result& Message)
		{
			Ar << Message.bSuccess;
			return Ar;
		}
	};
};

struct ATTOCOMMON_API FAttoDummyServerPush
{
	// Workaround to fix clang-format 19 messing up everything
	bool bDummy = false;

	friend FArchive& operator<<(FArchive& Ar, FAttoDummyServerPush& Message)
	{
		return Ar;
	}
};

using FAttoClientRequestProtocol = TVariant<
    FAttoLoginRequest,
    FAttoLogoutRequest,
    FAttoCreateSessionRequest,
    FAttoUpdateSessionRequest,
    FAttoDestroySessionRequest,
    FAttoFindSessionsRequest,
    FAttoQueryServerUtcTimeRequest,
    FAttoStartMatchmakingRequest,
    FAttoCancelMatchmakingRequest>;

using FAttoServerResponseProtocol = TVariant<
    FAttoLoginRequest::Result,
    FAttoLogoutRequest::Result,
    FAttoCreateSessionRequest::Result,
    FAttoUpdateSessionRequest::Result,
    FAttoDestroySessionRequest::Result,
    FAttoFindSessionsRequest::Result,
    FAttoQueryServerUtcTimeRequest::Result,
    FAttoStartMatchmakingRequest::Result,
    FAttoCancelMatchmakingRequest::Result>;

using FAttoServerPushProtocol = TVariant<
    FAttoDummyServerPush>;

constexpr int64 SERVER_PUSH_REQUEST_ID = 0;
