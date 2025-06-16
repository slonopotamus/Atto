using UnrealBuildTool;

public class OnlineSubsystemAtto : ModuleRules
{
	public OnlineSubsystemAtto(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new[]
		{
			"AttoClient",
			"AttoCommon",
			"Core",
			"CoreUObject",
			"Json",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"Sockets"
		});
	}
}
