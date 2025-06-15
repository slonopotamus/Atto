using UnrealBuildTool;

public class AttoCommon : ModuleRules
{
	public AttoCommon(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange([
			"Core",
			"CoreUObject",
			"Engine",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
		]);
	}
}
