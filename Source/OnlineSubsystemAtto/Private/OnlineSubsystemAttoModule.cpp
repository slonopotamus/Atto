#include "OnlineSubsystemAtto.h"

class FOnlineFactoryAtto final : public IOnlineFactory
{
public:
	virtual IOnlineSubsystemPtr CreateSubsystem(FName InstanceName) override
	{
		auto Result = MakeShared<FOnlineSubsystemAtto, ESPMode::ThreadSafe>(InstanceName);

		if (!Result->IsEnabled())
		{
			UE_LOG_ONLINE(Warning, TEXT("OnlineSubsystemAtto is disabled"));
			return nullptr;
		}

		if (!Result->Init())
		{
			UE_LOG_ONLINE(Warning, TEXT("OnlineSubsystemAtto failed to initialize"));
			Result->Shutdown();
			return nullptr;
		}

		return Result;
	}
};

class FOnlineSubsystemAttoModule final : public FDefaultModuleImpl
{
	FOnlineFactoryAtto Factory;

	virtual void StartupModule() override
	{
		if (auto* OSSModule = FModuleManager::GetModulePtr<FOnlineSubsystemModule>("OnlineSubsystem"))
		{
			OSSModule->RegisterPlatformService(ATTO_SUBSYSTEM, &Factory);
		}
	}

	virtual void ShutdownModule() override
	{
		if (auto* OSSModule = FModuleManager::GetModulePtr<FOnlineSubsystemModule>("OnlineSubsystem"))
		{
			OSSModule->UnregisterPlatformService(ATTO_SUBSYSTEM);
		}
	}

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

	virtual bool SupportsAutomaticShutdown() override
	{
		return false;
	}
};

IMPLEMENT_MODULE(FOnlineSubsystemAttoModule, OnlineSubsystemAtto);
