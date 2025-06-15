#pragma once

#include "Interfaces/OnlineTimeInterface.h"

class FOnlineSubsystemAtto;

class FOnlineTimeAtto final : public IOnlineTime
{
	FOnlineSubsystemAtto& Subsystem;

	TOptional<FDateTime> LastServerUtcTime;

public:
	explicit FOnlineTimeAtto(FOnlineSubsystemAtto& Subsystem);

	virtual bool QueryServerUtcTime() override;

	virtual FString GetLastServerUtcTime() override;
};
