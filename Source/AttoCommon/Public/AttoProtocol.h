#pragma once

#include "OnlineSessionSettings.h"

struct ATTOCOMMON_API FAttoLoginRequest
{
	FString Username;

	FString Password;

	friend FArchive& operator<<(FArchive& Ar, FAttoLoginRequest& Message)
	{
		Ar << Message.Username;
		Ar << Message.Password;

		return Ar;
	}

	using Result = TVariant<uint64 /* UserId */, FString /* Error */>;
};

struct ATTOCOMMON_API FAttoLogoutRequest
{
	// Workaround to fix clang-format 19 messing up everything
	bool bDummy = false;

	friend FArchive& operator<<(FArchive& Ar, FAttoLogoutRequest& Message)
	{
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

	void CopyTo(FOnlineSession& OnlineSession, EOnlineSessionState::Type* StatePtr) const
	{
		OnlineSession.NumOpenPublicConnections = NumOpenPublicConnections;
		OnlineSession.SessionSettings.NumPublicConnections = NumPublicConnections;
		OnlineSession.SessionSettings.bAllowJoinInProgress = bAllowJoinInProgress;
		OnlineSession.SessionSettings.bShouldAdvertise = bShouldAdvertise;

		if (StatePtr)
		{
			*StatePtr = State;
		}
	}

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
};

using FAttoFindSessionsParamValue = TVariant<bool, int32, uint32, int64, uint64, double, FString, float, TArray<uint8>>;

struct ATTOCOMMON_API FAttoFindSessionsParam
{
	EOnlineComparisonOp::Type ComparisonOp = EOnlineComparisonOp::Equals;
	FAttoFindSessionsParamValue Value{TInPlaceType<bool>{}, false};
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

struct ATTOCOMMON_API FAttoSessionInfo
{
	uint64 SessionId = 0;
	TArray<uint8> HostAddress;
	int32 Port = 0;
	int32 BuildUniqueId = 0;
	FAttoSessionUpdatableInfo UpdatableInfo;
	// TODO: We could skip sending this when creating session and instead determine whether connection is from dedicated server during Login
	bool bIsDedicated = false;
	bool bAntiCheatProtected = false;

	bool IsJoinable() const
	{
		if (!UpdatableInfo.bShouldAdvertise)
		{
			return false;
		}

		if (UpdatableInfo.NumOpenPublicConnections < 0)
		{
			return false;
		}

		return UpdatableInfo.bAllowJoinInProgress || UpdatableInfo.State != EOnlineSessionState::InProgress;
	}

	bool IsEmpty() const
	{
		return UpdatableInfo.NumOpenPublicConnections >= UpdatableInfo.NumPublicConnections;
	}

	bool Matches(const TMap<FName, FAttoFindSessionsParam>& Params) const;

	friend FArchive& operator<<(FArchive& Ar, FAttoSessionInfo& Message)
	{
		Ar << Message.SessionId;
		Ar << Message.HostAddress;
		Ar << Message.Port;
		Ar << Message.BuildUniqueId;
		Ar << Message.UpdatableInfo;
		Ar << Message.bIsDedicated;
		Ar << Message.bAntiCheatProtected;
		return Ar;
	}
};

struct ATTOCOMMON_API FAttoSessionInfoEx
{
	uint64 OwningUserId = 0;
	FAttoSessionInfo Info;

	friend FArchive& operator<<(FArchive& Ar, FAttoSessionInfoEx& Message)
	{
		Ar << Message.OwningUserId;
		Ar << Message.Info;
		return Ar;
	}
};

struct ATTOCOMMON_API FAttoCreateSessionRequest
{
	FAttoSessionInfo SessionInfo;

	friend FArchive& operator<<(FArchive& Ar, FAttoCreateSessionRequest& Message)
	{
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
	uint64 SessionId = 0;

	FAttoSessionUpdatableInfo SessionInfo;

	friend FArchive& operator<<(FArchive& Ar, FAttoUpdateSessionRequest& Message)
	{
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
	// Workaround to fix clang-format 19 messing up everything
	bool bDummy = false;

	friend FArchive& operator<<(FArchive& Ar, FAttoDestroySessionRequest& Message)
	{
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
	TMap<FName, FAttoFindSessionsParam> Params;

	int32 RequestId = 0;

	int32 MaxResults = 0;

	friend FArchive& operator<<(FArchive& Ar, FAttoFindSessionsRequest& Message)
	{
		Ar << Message.Params;
		Ar << Message.RequestId;
		Ar << Message.MaxResults;
		return Ar;
	}

	struct ATTOCOMMON_API Result
	{
		int32 RequestId = 0;

		TArray<FAttoSessionInfoEx> Sessions;

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

using FAttoC2SProtocol = TVariant<
    FAttoLoginRequest,
    FAttoLogoutRequest,
    FAttoCreateSessionRequest,
    FAttoUpdateSessionRequest,
    FAttoDestroySessionRequest,
    FAttoFindSessionsRequest,
    FAttoQueryServerUtcTimeRequest>;

using FAttoS2CProtocol = TVariant<
    FAttoLoginRequest::Result,
    FAttoLogoutRequest::Result,
    FAttoCreateSessionRequest::Result,
    FAttoUpdateSessionRequest::Result,
    FAttoDestroySessionRequest::Result,
    FAttoFindSessionsRequest::Result,
    FAttoQueryServerUtcTimeRequest::Result>;
