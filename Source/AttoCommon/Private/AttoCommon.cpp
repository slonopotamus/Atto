#include "AttoCommon.h"

namespace Atto
{
	const uint32 DefaultPort{27777};

	const char* const Protocol{"atto"};

	uint32 GetPort(const TCHAR* Stream)
	{
		uint32 Result = DefaultPort;
		FParse::Value(Stream, TEXT("AttoListenPort="), Result);
		return Result;
	}

	TOptional<FString> GetBindAddress(const TCHAR* Stream)
	{
		if (FString Bind; FParse::Value(Stream, TEXT("AttoBindAddress="), Bind))
		{
			return Bind;
		}

		return {};
	}

	TOptional<FString> GetConnectUrl(const TCHAR* Stream)
	{
		if (FString Url; FParse::Value(FCommandLine::Get(), TEXT("AttoUrl="), Url))
		{
			return Url;
		}

		if (GIsEditor && !IsRunningCommandlet())
		{
			const auto BindAddress = GetBindAddress(Stream);
			return FString::Printf(TEXT("ws://%s:%u"), *BindAddress.Get(TEXT("localhost")), GetPort(Stream));
		}

		return {};
	}
} // namespace Atto

DEFINE_LOG_CATEGORY(LogAtto);
