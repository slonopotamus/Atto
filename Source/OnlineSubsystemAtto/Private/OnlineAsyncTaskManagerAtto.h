#pragma once

#include "OnlineAsyncTaskManager.h"

class FOnlineSubsystemAtto;

class FOnlineAsyncTaskManagerAtto final : public FOnlineAsyncTaskManager
{
public:
	explicit FOnlineAsyncTaskManagerAtto(FOnlineSubsystemAtto& Subsystem)
	    : Subsystem(Subsystem)
	{
	}

	virtual void OnlineTick() override;

	FOnlineSubsystemAtto& Subsystem;
};
