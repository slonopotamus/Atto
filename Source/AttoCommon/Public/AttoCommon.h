#pragma once

namespace Atto
{
	ATTOCOMMON_API extern const uint32 DefaultPort;

	ATTOCOMMON_API extern const char* const Protocol;

	ATTOCOMMON_API uint32 GetPort(const TCHAR* Stream = FCommandLine::Get());

	ATTOCOMMON_API TOptional<FString> GetBindAddress(const TCHAR* Stream = FCommandLine::Get());

	ATTOCOMMON_API TOptional<FString> GetConnectUrl(const TCHAR* Stream = FCommandLine::Get());

	ATTOCOMMON_API FString GetSteamServiceIdentity();
} // namespace Atto

ATTOCOMMON_API DECLARE_LOG_CATEGORY_EXTERN(LogAtto, Log, All);
