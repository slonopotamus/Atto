#pragma once

struct FAttoLoginRequest
{
	friend FArchive& operator<<(FArchive& Ar, FAttoLoginRequest& Message)
	{
		return Ar;
	}
};

using FAttoLoginResponse = TVariant<uint64 /* UserId */, FString /* Error */>;

using FAttoC2SProtocol = TVariant<FAttoLoginRequest>;

using FAttoS2CProtocol = TVariant<FAttoLoginResponse>;
