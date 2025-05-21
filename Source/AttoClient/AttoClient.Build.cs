using UnrealBuildTool;

public class AttoClient : ModuleRules
{
	public AttoClient(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange([
			"AttoCommon",
			"Core",
			"CoreUObject",
			"Engine",
			"WebSockets",
		]);
	}
}
