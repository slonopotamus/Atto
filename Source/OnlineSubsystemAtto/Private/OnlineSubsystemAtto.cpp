#include "OnlineSubsystemAtto.h"
#include "AttoClient.h"
#include "AttoCommon.h"
#include "OnlineAsyncTaskManagerAtto.h"
#include "OnlineIdentityAtto.h"
#include "OnlineSessionAtto.h"
#include "OnlineTimeAtto.h"

const FName ATTO_SUBSYSTEM{TEXT("ATTO")};

bool FOnlineSubsystemAtto::Init()
{
	UE_LOG_ONLINE(VeryVerbose, TEXT("FOnlineSubsystemAtto::Init()"));

	auto ConnectUrl = Atto::GetConnectUrl();
	if (!ConnectUrl.IsSet())
	{
		UE_LOG_ONLINE(Log, TEXT("Atto server url not specified, Atto subsystem will not initialize"));
		return false;
	}

	UE_LOG_ONLINE(Log, TEXT("Atto server url: %s"), **ConnectUrl);
	AttoClient = MakeShared<FAttoClient>(MoveTemp(*ConnectUrl));

	AttoClient->OnDisconnected.AddLambda([](const FString& Error, const bool bWasClean) {
		// TODO: We need better error handling
		// 1. Current in-flight requests callbacks will never be called
		// 2. Our current state will not properly reflect the fact that server cleaned up everything
		UE_LOG(LogAtto, Error, TEXT("Disconnected from Atto server: %s"), *Error);
	});

	IdentityInterface = MakeShared<FOnlineIdentityAtto>(*this);
	SessionInterface = MakeShared<FOnlineSessionAtto>(*this);
	TimeInterface = MakeShared<FOnlineTimeAtto>(*this);

	TaskManager = MakeShared<FOnlineAsyncTaskManagerAtto>(*this);

	if (!TaskManager->Init())
	{
		UE_LOG_ONLINE(Error, TEXT("Failed to init Atto TaskManager"));
		return false;
	}

	OnlineAsyncTaskThread = MakeShareable(FRunnableThread::Create(TaskManager.Get(), TEXT("OnlineSubsystemAtto"), 128 * 1024, TPri_Normal));

	// 1. We do not want to connect editor to itself
	// 2. Dedicated servers will auto-login in AGameSession::ProcessAutoLogin anyway
	// 3. PIE is handled by bOnlinePIEEnabled
	if (IsRunningGame())
	{
		bool bAutologinAtStartup = true;
		GConfig->GetBool(TEXT("OnlineSubsystemAtto"), TEXT("bAutoLoginAtStartup"), bAutologinAtStartup, GEngineIni);

		if (bAutologinAtStartup)
		{
			IdentityInterface->AutoLogin(0);
		}
	}

	return true;
}

template<typename T>
static void DestructAttoInterface(TSharedPtr<T>& Interface)
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
	AttoClient.Reset();

	DestructAttoInterface(IdentityInterface);
	DestructAttoInterface(SessionInterface);
	DestructAttoInterface(TimeInterface);

	TaskManager.Reset();

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

TSharedPtr<IOnlineTime> FOnlineSubsystemAtto::GetTimeInterface() const
{
	return TimeInterface;
}
