#pragma once

struct FAttoLoginRequest
{
	// TODO: Pass credentials

	friend FArchive& operator<<(FArchive& Ar, FAttoLoginRequest& Message)
	{
		return Ar;
	}
};

struct FAttoLogoutRequest
{
	uint64 UserId = 0;

	friend FArchive& operator<<(FArchive& Ar, FAttoLogoutRequest& Message)
	{
		Ar << Message.UserId;
		return Ar;
	}
};

using FAttoLoginResponse = TVariant<uint64 /* UserId */, FString /* Error */>;

using FAttoC2SProtocol = TVariant<FAttoLoginRequest, FAttoLogoutRequest>;

using FAttoS2CProtocol = TVariant<FAttoLoginResponse>;
