// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class ArtemisOutpostEditorTarget : TargetRules
{
	public ArtemisOutpostEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;

        //DefaultBuildSettings = BuildSettingsVersion.V6;

        //BuildEnvironment = TargetBuildEnvironment.Unique;

        DefaultBuildSettings = BuildSettingsVersion.Latest;

        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        ExtraModuleNames.AddRange( new string[] { "ArtemisOutpost" } );
	}
}
