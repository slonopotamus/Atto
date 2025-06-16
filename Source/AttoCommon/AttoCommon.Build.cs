using UnrealBuildTool;

public class AttoCommon : ModuleRules
{
	public AttoCommon(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
		});
	}
}
