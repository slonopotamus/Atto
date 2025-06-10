#pragma once

#include "AttoServer.h"
#include "Subsystems/EngineSubsystem.h"

#include "AttoServerSubsystem.generated.h"

class FAttoServerInstance;

UCLASS()
class ATTOSERVER_API UAttoServerSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;

	TUniquePtr<FAttoServerInstance> AttoServer;
};
