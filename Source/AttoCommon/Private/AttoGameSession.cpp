#include "AttoGameSession.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"

FOnlineSessionSettings AAttoGameSession::BuildSessionSettings() const
{
	FOnlineSessionSettings Settings;

	Settings.bIsDedicated = GetNetMode() == NM_DedicatedServer;
	Settings.NumPublicConnections = MaxPlayers;
	Settings.bShouldAdvertise = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bUsesPresence = !Settings.bIsDedicated;
	Settings.bAllowJoinViaPresence = true;

	return Settings;
}

void AAttoGameSession::RegisterServer()
{
	Super::RegisterServer();

	if (const auto& SessionInt = Online::GetSessionInterface(GetWorld()); ensure(SessionInt))
	{
		const auto Settings = BuildSessionSettings();

		SessionInt->CreateSession(0, SessionName, Settings);
	}
}

void AAttoGameSession::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (const auto& SessionInt = Online::GetSessionInterface(GetWorld()))
	{
		SessionInt->DestroySession(SessionName);
	}
}
