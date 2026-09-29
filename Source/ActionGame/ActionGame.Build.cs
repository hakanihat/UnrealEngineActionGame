using UnrealBuildTool;

public class ActionGame : ModuleRules
{
	public ActionGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Allows includes like "Combat/HealthComponent.h" from anywhere in the module.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayTags",
			"DeveloperSettings",
			"PhysicsCore",
			"Niagara",
			"AIModule",
			"NavigationSystem"
		});
	}
}
