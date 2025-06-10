#include "AttoServerSubsystem.h"

bool UAttoServerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	// TODO: Improve this
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
