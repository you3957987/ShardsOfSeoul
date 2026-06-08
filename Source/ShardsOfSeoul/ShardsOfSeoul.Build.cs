// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ShardsOfSeoul : ModuleRules
{
	public ShardsOfSeoul(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"ShardsOfSeoul",
			"ShardsOfSeoul/Variant_Platforming",
			"ShardsOfSeoul/Variant_Platforming/Animation",
			"ShardsOfSeoul/Variant_Combat",
			"ShardsOfSeoul/Variant_Combat/AI",
			"ShardsOfSeoul/Variant_Combat/Animation",
			"ShardsOfSeoul/Variant_Combat/Gameplay",
			"ShardsOfSeoul/Variant_Combat/Interfaces",
			"ShardsOfSeoul/Variant_Combat/UI",
			"ShardsOfSeoul/Variant_SideScrolling",
			"ShardsOfSeoul/Variant_SideScrolling/AI",
			"ShardsOfSeoul/Variant_SideScrolling/Gameplay",
			"ShardsOfSeoul/Variant_SideScrolling/Interfaces",
			"ShardsOfSeoul/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
