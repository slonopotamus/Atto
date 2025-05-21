using UnrealBuildTool;

public class AttoServer : ModuleRules
{
	public AttoServer(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange([
			"AttoCommon",
			"Core",
			"CoreUObject",
			"Engine",
			"libWebSockets"
		]);
	}
}
