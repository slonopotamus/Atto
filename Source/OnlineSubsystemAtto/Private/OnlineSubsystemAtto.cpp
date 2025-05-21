#include "OnlineSubsystemAtto.h"

#include "OnlineIdentityAtto.h"
#include "OnlineSessionAtto.h"

const FName ATTO_SUBSYSTEM{TEXT("ATTO")};

bool FOnlineSubsystemAtto::Init()
{
	UE_LOG_ONLINE(VeryVerbose, TEXT("FOnlineSubsystemAtto::Init()"));

	IdentityInterface = MakeShared<FOnlineIdentityAtto>(*this);
	SessionInterface = MakeShared<FOnlineSessionAtto>(*this);

	return true;
}

template<typename T>
static void DestructAttoInterface(T& Interface)
{
	if (Interface)
	{
		ensure(Interface.IsUnique());
		Interface.Reset();
	}
}

bool FOnlineSubsystemAtto::Shutdown()
{
	UE_LOG_ONLINE(VeryVerbose, TEXT("FOnlineSubsystemAtto::Shutdown()"));

	DestructAttoInterface(IdentityInterface);
	DestructAttoInterface(SessionInterface);

	Super::Shutdown();

	return true;
}

bool FOnlineSubsystemAtto::Tick(float DeltaSeconds)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FOnlineSubsystemAtto_Tick);

	if (!Super::Tick(DeltaSeconds))
	{
		return false;
	}

	if (SessionInterface)
	{
		SessionInterface->Tick(DeltaSeconds);
	}

	return true;
}

FString FOnlineSubsystemAtto::GetAppId() const
{
	return TEXT("");
}

IOnlineFriendsPtr FOnlineSubsystemAtto::GetFriendsInterface() const
{
	return nullptr;
}

IOnlineIdentityPtr FOnlineSubsystemAtto::GetIdentityInterface() const
{
	return IdentityInterface;
}

FText FOnlineSubsystemAtto::GetOnlineServiceName() const
{
	return NSLOCTEXT("OnlineSubsystemAtto", "OnlineServiceName", "Atto");
}

IOnlineSessionPtr FOnlineSubsystemAtto::GetSessionInterface() const
{
	return SessionInterface;
}
