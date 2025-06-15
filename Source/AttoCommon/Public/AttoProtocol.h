#pragma once

struct FAttoLoginRequest
{
	FString Username;
	FString Password;
	// TODO: Pass credentials

	friend FArchive& operator<<(FArchive& Ar, FAttoLoginRequest& Message)
	{
		Ar << Message.Username;
		Ar << Message.Password;

		return Ar;
	}
};

using FAttoLoginResponse = TVariant<uint64 /* UserId */, FString /* Error */>;

struct FAttoLogoutRequest
{
	friend FArchive& operator<<(FArchive& Ar, FAttoLogoutRequest& Message)
	{
		return Ar;
	}
};

struct FAttoLogoutResponse
{
	friend FArchive& operator<<(FArchive& Ar, FAttoLogoutResponse& Message)
	{
		return Ar;
	}
};

struct FAttoSessionInfo
{
	uint64 SessionId = 0;
	TArray<uint8> HostAddress;
	int32 Port = 0;
	int32 BuildUniqueId = 0;
	int32 NumOpenPublicConnections = 0;
	int32 NumPublicConnections = 0;

	friend FArchive& operator<<(FArchive& Ar, FAttoSessionInfo& Message)
	{
		Ar << Message.SessionId;
		Ar << Message.HostAddress;
		Ar << Message.Port;
		Ar << Message.BuildUniqueId;
		Ar << Message.NumOpenPublicConnections;
		Ar << Message.NumPublicConnections;
		return Ar;
	}
};

struct FAttoSessionInfoEx
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

struct FAttoCreateSessionRequest
{
	FAttoSessionInfo SessionInfo;

	friend FArchive& operator<<(FArchive& Ar, FAttoCreateSessionRequest& Message)
	{
		Ar << Message.SessionInfo;
		return Ar;
	}
};

struct FAttoCreateSessionResponse
{
	bool bSuccess = false;

	friend FArchive& operator<<(FArchive& Ar, FAttoCreateSessionResponse& Message)
	{
		Ar.SerializeBits(&Message.bSuccess, 1);
		return Ar;
	}
};

struct FAttoDestroySessionRequest
{
	friend FArchive& operator<<(FArchive& Ar, FAttoDestroySessionRequest& Message)
	{
		return Ar;
	}
};

struct FAttoDestroySessionResponse
{
	bool bSuccess = false;

	friend FArchive& operator<<(FArchive& Ar, FAttoDestroySessionResponse& Message)
	{
		Ar.SerializeBits(&Message.bSuccess, 1);
		return Ar;
	}
};

struct FAttoFindSessionsRequest
{
	int32 RequestId = 0;
	uint32 MaxResults = 0;

	friend FArchive& operator<<(FArchive& Ar, FAttoFindSessionsRequest& Message)
	{
		Ar << Message.RequestId;
		Ar << Message.MaxResults;
		return Ar;
	}
};

struct FAttoFindSessionsResponse
{
	int32 RequestId = 0;
	TArray<FAttoSessionInfoEx> Sessions;

	friend FArchive& operator<<(FArchive& Ar, FAttoFindSessionsResponse& Message)
	{
		Ar << Message.RequestId;
		Ar << Message.Sessions;
		return Ar;
	}
};

using FAttoC2SProtocol = TVariant<FAttoLoginRequest, FAttoLogoutRequest, FAttoCreateSessionRequest, FAttoDestroySessionRequest, FAttoFindSessionsRequest>;

using FAttoS2CProtocol = TVariant<FAttoLoginResponse, FAttoLogoutResponse, FAttoCreateSessionResponse, FAttoDestroySessionResponse, FAttoFindSessionsResponse>;
