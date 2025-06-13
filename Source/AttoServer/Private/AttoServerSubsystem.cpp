#include "AttoServerSubsystem.h"

bool UAttoServerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("EnableAttoServer")))
	{
		return true;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("DisableAttoServer")))
	{
		return false;
	}

	return GIsEditor;
}

void UAttoServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	AttoServer = FAttoServer().WithCommandLineOptions().Create();
}

void UAttoServerSubsystem::Deinitialize()
{
	AttoServer.Reset();

	Super::Deinitialize();
}
