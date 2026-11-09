<#
.SYNOPSIS
  Re-applies the ArtemisOutpost fixes to the Meta XR plugin (OculusXRAnchors, OpenXR path).

.DESCRIPTION
  The fixes were made by hand in MetaXR 1.201.0 (UE 5.7). This script finds the code by content,
  not by line number, so it also works on newer plugin versions (e.g. 207.0 for UE 5.8).
  For every fix it reports one of:
    APPLIED   the bug was found and patched now
    PRESENT   the fix is already there (patched earlier, or Meta fixed it upstream)
    REVIEW    the code changed upstream and could not be matched, check by hand
  The script is idempotent. A backup "<file>.orig" is written before the first change.

  Fix 1  Group sharing flag typo. Upstream assigns the GROUP_SHARING extension result to
         bExtSharingMetaEnabled, so bExtGroupSharingEnabled stays false and
         ShareAnchorsWithGroups / GetSharedAnchors refuse to run.
  Fix 2  XrEventDataSpaceQueryResultsAvailableFB handler returns early for requests that are
         not in the legacy binding maps. The group async request (FGetAnchorsSharedWithGroup)
         lives in FAsyncRequestSystem, so GetSharedAnchorsAsync always got 0 anchors.
  Fix 3  XrEventDataShareSpacesCompleteMETA handler broadcasts the legacy
         OculusSpaceShareComplete delegate. FShareAnchorsWithGroups listens to
         OculusShareAnchorsComplete, so the ShareAnchorsAsync callback never fired.

  NOT patched on purpose: the double base orientation in FAnchorsXR::TryGetAnchorTransform /
  CreateSpatialAnchor. The project compensates it with artemis.AnchorTiltApplications=2.

.PARAMETER PluginRoot
  Folder that contains OculusXR.uplugin. Default: <project>\Plugins\MetaXR

.PARAMETER DryRun
  Only report, do not write.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File .\Apply-MetaXRPatches.ps1
  powershell -ExecutionPolicy Bypass -File .\Apply-MetaXRPatches.ps1 -DryRun
#>
[CmdletBinding()]
param(
    [string]$PluginRoot = '',
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
if (-not $PluginRoot) { $PluginRoot = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) '..\..\Plugins\MetaXR' }
$Marker = '[ArtemisOutpost patch]'

$PluginRoot = (Resolve-Path $PluginRoot).Path
$File = Join-Path $PluginRoot 'Source\OculusXRAnchors\Private\openxr\OculusXRAnchorsXR.cpp'
if (-not (Test-Path $File)) { throw "Not found: $File" }

# Keep the original encoding (BOM or not) and line endings.
$bytes = [IO.File]::ReadAllBytes($File)
$hasBom = ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)
$text = [IO.File]::ReadAllText($File)
$nl = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
$results = [ordered]@{}

# Returns start/end of the event handler block that begins at the given XR event type.
function Get-HandlerRange([string]$src, [string]$eventType) {
    $start = $src.IndexOf("InHeader->type == $eventType")
    if ($start -lt 0) { return $null }
    $end = $src.IndexOf('InHeader->type ==', $start + 20)
    if ($end -lt 0) {
        # Last handler: stop at the end of OnEvent (first line that is exactly "	}" after the block).
        $m = [regex]::Match($src.Substring($start), '\r?\n\t\}\r?\n')
        $end = if ($m.Success) { $start + $m.Index } else { $src.Length }
    }
    return @($start, $end)
}

# ---------- Fix 1: group sharing flag ----------
$fix1Bad  = 'bExtSharingMetaEnabled(\s*=\s*InModule->IsExtensionEnabled\(\s*XR_META_SPATIAL_ENTITY_GROUP_SHARING_EXTENSION_NAME\s*\))'
$fix1Good = 'bExtGroupSharingEnabled\s*=\s*InModule->IsExtensionEnabled\(\s*XR_META_SPATIAL_ENTITY_GROUP_SHARING_EXTENSION_NAME\s*\)'
if ([regex]::IsMatch($text, $fix1Bad)) {
    $text = [regex]::Replace($text, $fix1Bad, 'bExtGroupSharingEnabled$1')
    $results['1 GroupSharing flag'] = 'APPLIED'
} elseif ([regex]::IsMatch($text, $fix1Good)) {
    $results['1 GroupSharing flag'] = 'PRESENT'
} else {
    $results['1 GroupSharing flag'] = 'REVIEW  (no IsExtensionEnabled(GROUP_SHARING) assignment found)'
}

# ---------- Fix 2: query results early return ----------
$r = Get-HandlerRange $text 'XR_TYPE_EVENT_DATA_SPACE_QUERY_RESULTS_AVAILABLE_FB'
if ($null -eq $r) {
    $results['2 QueryResults guard'] = 'REVIEW  (handler XR_TYPE_EVENT_DATA_SPACE_QUERY_RESULTS_AVAILABLE_FB not found)'
} else {
    $section = $text.Substring($r[0], $r[1] - $r[0])
    $guard = '(?s)(?<indent>[ \t]*)auto\*\s+AnchorsInstance\s*=\s*OculusXRAnchors::FOculusXRAnchors::GetInstance\(\);[ \t]*\r?\n[ \t]*if\s*\(\s*!AnchorsInstance\s*\|\|\s*!AnchorsInstance->HasAnchorQueryBinding\(\s*event->requestId\s*\)\s*\)\s*\{.*?return;\s*\}'
    if ($section.Contains($Marker) -or $section -match '(?s)/\*\s*(//[^\n]*\n\s*)*auto\*\s+AnchorsInstance') {
        # Patched by this script, or the original 5.7 hand patch that wrapped the guard in /* */.
        $results['2 QueryResults guard'] = 'PRESENT'
    } elseif ([regex]::IsMatch($section, $guard)) {
        $section = ([regex]$guard).Replace($section, [Text.RegularExpressions.MatchEvaluator]{
            param($m)
            $i = $m.Groups['indent'].Value
            "$i// $Marker Do not early-return here. The group async request (FGetAnchorsSharedWithGroup)$nl" +
            "$i// is tracked in FAsyncRequestSystem, not in the legacy binding maps, so this guard made$nl" +
            "$i// GetSharedAnchorsAsync always return 0 anchors. Every consumer checks its own request id.$nl" +
            "#if 0$nl" + $m.Value + "$nl#endif"
        }, 1)
        $text = $text.Substring(0, $r[0]) + $section + $text.Substring($r[1])
        $results['2 QueryResults guard'] = 'APPLIED'
    } elseif ($section -notmatch 'HasAnchorQueryBinding') {
        $results['2 QueryResults guard'] = 'PRESENT (guard no longer exists upstream, verify GetSharedAnchorsAsync on device)'
    } else {
        $results['2 QueryResults guard'] = 'REVIEW  (HasAnchorQueryBinding still used but in a different form)'
    }
}

# ---------- Fix 3: share-complete delegate ----------
$r = Get-HandlerRange $text 'XR_TYPE_EVENT_DATA_SHARE_SPACES_COMPLETE_META'
if ($null -eq $r) {
    $results['3 ShareSpaces delegate'] = 'REVIEW  (handler XR_TYPE_EVENT_DATA_SHARE_SPACES_COMPLETE_META not found)'
} else {
    $section = $text.Substring($r[0], $r[1] - $r[0])
    $bad = '(?<indent>[ \t]*)FOculusXRAnchorEventDelegates::OculusSpaceShareComplete\.Broadcast\('
    if ($section -match 'OculusShareAnchorsComplete\.Broadcast\(') {
        $results['3 ShareSpaces delegate'] = 'PRESENT'
    } elseif ([regex]::IsMatch($section, $bad)) {
        $section = ([regex]$bad).Replace($section, [Text.RegularExpressions.MatchEvaluator]{
            param($m)
            $i = $m.Groups['indent'].Value
            "$i// $Marker Broadcast the async delegate, FShareAnchorsWithGroups::OnShareComplete listens to it.$nl" +
            "${i}FOculusXRAnchorEventDelegates::OculusShareAnchorsComplete.Broadcast("
        }, 1)
        $text = $text.Substring(0, $r[0]) + $section + $text.Substring($r[1])
        $results['3 ShareSpaces delegate'] = 'APPLIED'
    } else {
        $results['3 ShareSpaces delegate'] = 'REVIEW  (no Broadcast found in the handler)'
    }
}

# ---------- Write + report ----------
$changed = ($results.Values | Where-Object { $_ -eq 'APPLIED' }).Count -gt 0
Write-Host ""
Write-Host "MetaXR patches for $File"
$results.GetEnumerator() | ForEach-Object { Write-Host ("  {0,-24} {1}" -f $_.Key, $_.Value) }

if ($changed -and -not $DryRun) {
    $backup = "$File.orig"
    if (-not (Test-Path $backup)) { Copy-Item $File $backup }
    $enc = New-Object System.Text.UTF8Encoding($hasBom)
    [IO.File]::WriteAllText($File, $text, $enc)
    Write-Host "  -> written (backup: $backup)"
} elseif ($changed) {
    Write-Host "  -> DryRun, nothing written"
} else {
    Write-Host "  -> nothing to change"
}

if (($results.Values | Where-Object { $_ -like 'REVIEW*' }).Count -gt 0) {
    Write-Warning "At least one fix needs a manual look. Compare with patches\MetaXR_1.201.0_OculusXRAnchorsXR.patch"
    exit 2
}
exit 0
