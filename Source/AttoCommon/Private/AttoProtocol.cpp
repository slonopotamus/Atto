#include "AttoProtocol.h"
#include "SocketSubsystem.h"

TSharedRef<FInternetAddr> FAttoSessionAddress::ToInternetAddr() const
{
	auto Result = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	Result->SetRawIp(Host);
	Result->SetPort(Port);
	return Result;
}
