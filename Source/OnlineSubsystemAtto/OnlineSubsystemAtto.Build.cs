using UnrealBuildTool;

public class OnlineSubsystemAtto : ModuleRules
{
	public OnlineSubsystemAtto(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange([
			"AttoClient",
			"AttoCommon",
			"Core",
			"CoreUObject",
			"Json",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"Sockets"
		]);
	}
}
