using UnrealBuildTool;

public class AttoClient : ModuleRules
{
	public AttoClient(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new[]
		{
			"AttoCommon",
			"Core",
			"CoreUObject",
			"Engine",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"WebSockets",
		});
	}
}
