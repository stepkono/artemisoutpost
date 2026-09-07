// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class ArtemisOutpost : ModuleRules
{
	public ArtemisOutpost(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableExceptions = true;
		
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"DeveloperSettings",
			"Json",
			"JsonUtilities",
			"WebSockets",
			"Chaos",
			"ChaosVehiclesCore",
			"ChaosVehicles",
			"ChaosVehiclesEngine",
			"UMG",
			"Slate",
			"SlateCore",
			"EnhancedInput",
			"Niagara", 
            "CesiumRuntime", 
            "HeadMountedDisplay",
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDependencyModuleNames.AddRange(new string[]
			{
				"OculusXRHMD",
				"OculusXRAnchors",
				"AndroidPermission",
			});
		}

		PrivateDependencyModuleNames.AddRange(new string[] { "XRBase" });

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true

    }
}
