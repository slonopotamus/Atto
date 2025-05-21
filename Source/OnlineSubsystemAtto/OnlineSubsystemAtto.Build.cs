using UnrealBuildTool;

public class OnlineSubsystemAtto : ModuleRules
{
	public OnlineSubsystemAtto(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange([
			"AttoClient",
			"Core",
			"CoreUObject",
			"Json",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"Sockets"
		]);
	}
}
