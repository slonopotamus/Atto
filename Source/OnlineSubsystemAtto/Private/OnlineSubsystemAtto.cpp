#include "OnlineSubsystemAtto.h"
#include "AttoClient.h"
#include "AttoCommon.h"
#include "OnlineAsyncTaskManagerAtto.h"
#include "OnlineIdentityAtto.h"
#include "OnlineSessionAtto.h"

const FName ATTO_SUBSYSTEM{TEXT("ATTO")};

bool FOnlineSubsystemAtto::Init()
{
	UE_LOG_ONLINE(VeryVerbose, TEXT("FOnlineSubsystemAtto::Init()"));

	auto ConnectUrl = Atto::GetConnectUrl();
	UE_LOG_ONLINE(Log, TEXT("Atto server url: %s"), *ConnectUrl);
	AttoClient = MakeShared<FAttoClient>(MoveTemp(ConnectUrl));

	AttoClient->OnConnectionError.AddLambda([](const FString& Error) {
		// TODO: We need better error handling. Currently, login callbacks will not be called if this happens
		UE_LOG(LogAtto, Error, TEXT("Failed to connect to Atto server: %s"), *Error);
	});

	AttoClient->OnDisconnected.AddLambda([](const FString& Error, const bool bWasClean) {
		// TODO: We need better error handling
		// 1. Current in-flight requests callbacks will never be called
		// 2. Our current state will not properly reflect the fact that server cleaned up everything
		UE_LOG(LogAtto, Error, TEXT("Disconnected from Atto server: %s"), *Error);
	});

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

bool FOnlineSubsystemAtto::Tick(const float DeltaSeconds)
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
