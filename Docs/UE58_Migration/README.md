# ArtemisOutpost: migration from UE 5.7 to UE 5.8

Prepared on 2026-11-09. Everything in this folder is versioned in git. The `Plugins/` folder is git-ignored, so this folder is the only record of the plugin patches.

## 1. Audit: what was changed outside the project

Every plugin was compared file by file with a pristine copy. The engine was checked with `git status` in `C:\UE\UnrealEngine`.

| Location | Change | Still needed in 5.8? | Handled by |
|---|---|---|---|
| Engine source `C:\UE\UnrealEngine` (tag `5.7.4-release`) | No C++ changes. Only `Engine/Config/BaseEngine.ini`, 3 shader compiler values. | Yes (convenience) | Already moved to `Config/DefaultEngine.ini`, section `[DevOptions.Shaders]`. Works with any engine now. |
| Meta XR 1.201.0, `OculusXRAnchorsXR.cpp` | Fix 1: group sharing flag typo. Upstream writes the GROUP_SHARING result into `bExtSharingMetaEnabled`, so group sharing reports "unsupported". | Yes | `Apply-MetaXRPatches.ps1` |
| same file | Fix 2: early return in the `SPACE_QUERY_RESULTS_AVAILABLE_FB` handler. `GetSharedAnchorsAsync` always returned 0 anchors. | Yes | `Apply-MetaXRPatches.ps1` |
| same file | Fix 3: `SHARE_SPACES_COMPLETE_META` handler broadcasts the legacy delegate. The `ShareAnchorsAsync` callback never fired. | Yes | `Apply-MetaXRPatches.ps1` |
| same file | Extra diagnostic logs (Log raised to Warning, two extra warnings). | No | Dropped. The extension list is still logged at Log level. |
| Meta XR, anchor double base orientation | Not patched on purpose. The project compensates with `artemis.AnchorTiltApplications=2`. | Check after migration | See step 5 |
| Meta XR Platform 1.201.0 | No changes. | | |
| Cesium 2.26.0, `ExtensionImageAssetUnreal.cpp` | NullRHI texture guard for a dedicated server. | No, the project runs a listen server with RHI | Not re-applied. Kept in `patches/` for reference. |
| Cesium 2.26.0, `Content/Materials/M_CesiumBaseMaterial.uasset` | Edited on 2026-08-19 (re-saved in 5.7, a Vertex Color node added). Parent of `MI_VRMoonSurfaceMaterial` and `MI_MapClipping`. | Yes | `move_cesium_base_material.py` moves it into `/Game` |

Pristine references used: `Downloads/UnrealMetaXRPlugin.201.0.zip` and the Fab copies in `C:\Program Files\Epic Games\UE_5.7\Engine\Plugins\Marketplace`. The exact diffs are in `patches/`.

## 2. Target versions (state of 2026-11-09)

| Component | Version | Note |
|---|---|---|
| Unreal Engine | 5.8.4 (latest hotfix) | 5.8 is the last major UE5 release. It ships an experimental MCP plugin. |
| Meta XR plugin ("Unreal Engine 5 Integration") | 207.0 | Requires UE 5.8.0. Release notes mention no anchor fixes, so the 3 patches are still expected to be needed. |
| Meta XR Platform plugin | 207.0 (matching the Meta XR version) | |
| Cesium for Unreal | v2.31.x, UE 5.8 build | Supports UE 5.6 to 5.8. |

Known issues to watch:
- Meta: the 207.0 plugins declare 5.8 but are built for 5.7.0. Modules may fail to load with "Engine modules cannot be compiled at runtime". The install script removes the prebuilt binaries and sets `EngineVersion` to 5.8.0, so your build compiles them fresh.
- Cesium: one report of a crash with v2.31.0 on UE 5.8.4 in the VR sample, linked to an occlusion call. If the editor crashes when the moon loads, check the newest Cesium release first.

## 3. Engine choice: Launcher build is enough

The source build was only needed for the dedicated server target. The project now uses a listen server, and the engine source has no code changes. So the Epic Games Launcher build of 5.8 does everything you need (Win64 editor, Android packaging for Quest, MCP plugin). It also saves the multi-hour engine compile.

You lose only the `ArtemisOutpostServer` target (a dedicated server cannot be built with a Launcher engine). It is unused, so leave the file or delete it.

If you still want a source build later, check out the tag `5.8.4-release`. No engine patches need to be applied.

## 4. Steps

### Step 0: in UE 5.7, before anything else
1. Commit or stash the current work and create a branch, for example `ue58-migration`.
2. Open the project in UE 5.7. Run `Tools > Execute Python Script...` and select `Docs/UE58_Migration/move_cesium_base_material.py`. It needs the "Python Editor Script Plugin". The output log must end with `No /Game asset references the plugin material anymore`.
   Manual alternative: duplicate `CesiumForUnreal Content/Materials/M_CesiumBaseMaterial` into `/Game/Moon/Materials/M_MoonCesiumBase`, then set it as parent in `MI_VRMoonSurfaceMaterial` and `MI_MapClipping`.
3. Press Play once and check that the VR moon looks unchanged. Commit.
4. Close the editor and Rider / Visual Studio.

### Step 1: install UE 5.8
1. Epic Games Launcher > Unreal Engine > Library > `+` > choose 5.8.4.
2. Under Options, enable the Android target platform.
3. Android toolchain: open the 5.8 documentation page "Set up Android SDK and NDK" and install the versions it lists. Then run `Engine\Extras\Android\SetupAndroid.bat` from the 5.8 install.
4. Do not install Meta XR or Cesium from Fab into the engine. The project copies in `Plugins/` are the ones that get patched.

### Step 2: download the plugins
- Meta XR 207.0: https://developers.meta.com/horizon/downloads/package/unreal-engine-5-integration/
- Meta XR Platform 207.0: https://developers.meta.com/horizon/downloads/package/unreal-5-platform-sdk-plugin/
- Cesium for Unreal, the UE 5.8 zip: https://github.com/CesiumGS/cesium-unreal/releases

### Step 3: swap plugins and patch (automated)
Run in PowerShell from `Docs\UE58_Migration` (adjust the zip names to what you downloaded):

```powershell
powershell -ExecutionPolicy Bypass -File .\Install-UE58Plugins.ps1 -MetaXRZip "$env:USERPROFILE\Downloads\UnrealMetaXRPlugin.207.0.zip" -PlatformZip "$env:USERPROFILE\Downloads\Unreal5PlatformSDKPlugin.207.0.zip" -CesiumZip "$env:USERPROFILE\Downloads\CesiumForUnreal-58-v2.31.0.zip" -CleanProject
```

What it does: moves the old plugin folders to `D:\StepanProjects\ArtemisOutpost_PluginBackup_<date>` (nothing is deleted), installs the new ones, removes their prebuilt binaries, sets `EngineVersion`, and runs `Apply-MetaXRPatches.ps1`. `-CleanProject` also deletes the project's `Binaries`, `Intermediate`, `.vs` and `.sln`.

Read the patch report at the end:
- `APPLIED`: the bug was still there and is fixed now.
- `PRESENT`: Meta fixed it upstream, nothing to do.
- `REVIEW`: the code changed too much to match. Send the output to Claude.

### Step 4: build
1. Right-click `ArtemisOutpost.uproject` > "Switch Unreal Engine version..." > 5.8.
2. Open the generated solution and build `ArtemisOutpostEditor`, `Development Editor`, `Win64`.
3. Compile errors from 5.8 API changes are likely in the project code. Paste the error list to Claude.
4. If the editor asks to rebuild modules on start, answer No and build from the IDE instead.

### Step 5: verify
In the editor:
- Output log shows Meta XR 207.0 and the new Cesium version loaded, without version mismatch warnings.
- VR moon and AR moon render, tile culling (`artemis.CutoutCulling*`) still works.

On two Quests (logcat):
- `[Anchors] Extensions available` lists `Group Sharing: ENABLED`. If it says DISABLED, fix 1 is missing.
- Authoritative Quest: the `ShareAnchorsAsync` callback fires (fix 3).
- Second Quest: `GetSharedAnchorsAsync` returns 4 anchors and the AR cutout appears (fix 2).
- `[VRCues] AnchorsTilt` still reports 2 tilt applications. If it reports 1, Meta fixed the double base orientation. Then change the default of `artemis.AnchorTiltApplications` to 1.
- AR frame rate in GameLvl is not worse than in 5.7.

### Step 6: MCP for agentic work
1. Edit > Plugins, enable Epic's MCP plugin (experimental), restart the editor.
2. In its preferences, enable the auto-started local server. The default endpoint is `http://127.0.0.1:8000/mcp`.
3. Connect Claude Code from the project folder:

```bash
claude mcp add --transport http unreal-editor http://127.0.0.1:8000/mcp
```

The server has no authentication and listens only locally. Tool calls run on the game thread one after another.

## 5. Rollback
- UE 5.7 (Launcher) and the source build in `C:\UE\UnrealEngine` stay untouched. Keep them until 5.8 is verified on device.
- The old plugins are in the backup folder. Move them back into `Plugins/`.
- Switch the `.uproject` back to the 5.7 source build.

## 6. Future plugin updates
After every Meta XR update run `Apply-MetaXRPatches.ps1` again (or `Install-UE58Plugins.ps1` with only `-MetaXRZip`). Use `-DryRun` to see what it would change.
