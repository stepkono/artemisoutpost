"""
Run ONCE in the UE 5.7 editor, BEFORE swapping the Cesium plugin.

Why: M_CesiumBaseMaterial inside the Cesium plugin was edited on 2026-08-19 (re-saved in 5.7,
a Vertex Color node was added). The plugin folder is git-ignored and a Cesium upgrade replaces it,
so that edit would be lost. MI_VRMoonSurfaceMaterial and MI_MapClipping use it as parent.

What it does:
  1. duplicates /CesiumForUnreal/Materials/M_CesiumBaseMaterial -> /Game/Moon/Materials/M_MoonCesiumBase
  2. re-parents every /Game material instance whose parent is the plugin material
  3. saves, then lists anything in /Game that still references the plugin material

How to run: Tools > Execute Python Script... and pick this file
(needs the "Python Editor Script Plugin", Edit > Plugins).
"""
import unreal

SRC = "/CesiumForUnreal/Materials/M_CesiumBaseMaterial"
DST = "/Game/Moon/Materials/M_MoonCesiumBase"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log("[CesiumMatMove] " + msg)


if not eal.does_asset_exist(SRC):
    raise RuntimeError("Source material not found: " + SRC)

src_mat = eal.load_asset(SRC)

if eal.does_asset_exist(DST):
    log("Destination exists already, reusing " + DST)
else:
    if not eal.duplicate_asset(SRC, DST):
        raise RuntimeError("Duplicate failed: " + DST)
    eal.save_asset(DST, only_if_is_dirty=False)
    log("Duplicated to " + DST)

new_parent = eal.load_asset(DST)

referencers = eal.find_package_referencers_for_asset(SRC, load_assets_to_confirm=True) or []
reparented, manual = [], []
for pkg in referencers:
    pkg = str(pkg)
    if not pkg.startswith("/Game/") or pkg == DST:
        continue
    asset = eal.load_asset(pkg)
    if isinstance(asset, unreal.MaterialInstanceConstant) and asset.get_editor_property("parent") == src_mat:
        mel.set_material_instance_parent(asset, new_parent)
        eal.save_loaded_asset(asset, only_if_is_dirty=False)
        reparented.append(pkg)
    else:
        manual.append(pkg)

log("Re-parented: " + (", ".join(reparented) if reparented else "none"))

left = [str(p) for p in (eal.find_package_referencers_for_asset(SRC, load_assets_to_confirm=True) or [])
        if str(p).startswith("/Game/") and str(p) != DST]
if left:
    unreal.log_warning("[CesiumMatMove] Still referencing the plugin material, fix by hand: " + ", ".join(left))
else:
    log("Done. No /Game asset references the plugin material anymore.")
