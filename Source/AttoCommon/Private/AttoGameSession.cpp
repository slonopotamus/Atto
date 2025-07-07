#include "AttoGameSession.h"
#include "AttoCommon.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"

FOnlineSessionSettings AAttoGameSession::BuildSessionSettings() const
{
	FOnlineSessionSettings Settings;

	Settings.NumPublicConnections = MaxPlayers;
	Settings.bShouldAdvertise = true;
	Settings.bIsDedicated = GetNetMode() == NM_DedicatedServer;

	Settings.Set(SETTING_MAXSPECTATORS, MaxSpectators);

	return Settings;
}

void AAttoGameSession::RegisterServer()
{
	Super::RegisterServer();

	if (const auto& SessionInt = Online::GetSessionInterface(GetWorld()); ensure(SessionInt))
	{
		const auto Settings = BuildSessionSettings();

		OnCreateSessionCompleteDelegateHandle = SessionInt->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete));

		UE_LOG(LogAtto, Log, TEXT("Creating game session: %s"), *SessionName.ToString());
		SessionInt->CreateSession(0, SessionName, Settings);
	}

	if (const auto& IdentityInt = Online::GetIdentityInterface(GetWorld()); ensure(IdentityInt))
	{
		OnLoginStatusChangedDelegateHandle = IdentityInt->AddOnLoginStatusChangedDelegate_Handle(0, FOnLoginStatusChangedDelegate::CreateUObject(this, &ThisClass::OnLoginStatusChanged));
	}
}

void AAttoGameSession::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (const auto& SessionInt = Online::GetSessionInterface(GetWorld()))
	{
		SessionInt->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegateHandle);
		SessionInt->DestroySession(SessionName);
	}

	if (const auto& IdentityInt = Online::GetIdentityInterface(GetWorld()); ensure(IdentityInt))
	{
		IdentityInt->ClearOnLoginStatusChangedDelegate_Handle(0, OnLoginStatusChangedDelegateHandle);
	}
}

void AAttoGameSession::OnCreateSessionComplete(const FName InSessionName, bool bWasSuccessful)
{
	if (!ensure(InSessionName == SessionName))
	{
		return;
	}

	UE_LOG(LogAtto, Log, TEXT("OnCreateSessionComplete %s bSuccess: %d"), *SessionName.ToString(), bWasSuccessful);

	if (const auto& SessionInt = Online::GetSessionInterface(GetWorld()))
	{
		SessionInt->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegateHandle);
	}

	if (!bWasSuccessful)
	{
		RegisterServerFailed();
	}
}

void AAttoGameSession::OnLoginStatusChanged(int32 LocalPlayerIndex, ELoginStatus::Type OldStatus, ELoginStatus::Type NewStatus, const FUniqueNetId& NewId)
{
	if (NewStatus == ELoginStatus::NotLoggedIn)
	{
		RegisterServerFailed();
	}
}

void AAttoGameSession::RegisterServerFailed()
{
	Super::RegisterServerFailed();

	if (IsRunningDedicatedServer())
	{
		FPlatformMisc::RequestExitWithStatus(false, 33);
	}
	else
	{
		ReturnToMainMenuHost();
	}
}
