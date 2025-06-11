#include "OnlineSubsystemAtto.h"
#include "OnlineAsyncTaskManagerAtto.h"
#include "OnlineIdentityAtto.h"
#include "OnlineSessionAtto.h"

const FName ATTO_SUBSYSTEM{TEXT("ATTO")};

bool FOnlineSubsystemAtto::Init()
{
	UE_LOG_ONLINE(VeryVerbose, TEXT("FOnlineSubsystemAtto::Init()"));

	IdentityInterface = MakeShared<FOnlineIdentityAtto>(*this);
	SessionInterface = MakeShared<FOnlineSessionAtto>(*this);

	TaskManager = MakeShared<FOnlineAsyncTaskManagerAtto>(*this);

	if (!TaskManager->Init())
	{
		return false;
	}

	OnlineAsyncTaskThread = MakeShareable(FRunnableThread::Create(TaskManager.Get(), TEXT("OnlineSubsystemAtto"), 128 * 1024, TPri_Normal));

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

	OnlineAsyncTaskThread.Reset();

	DestructAttoInterface(IdentityInterface);
	DestructAttoInterface(SessionInterface);

	TaskManager.Reset();
	AttoClient.Reset();

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

	if (TaskManager)
	{
		TaskManager->GameTick();
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
