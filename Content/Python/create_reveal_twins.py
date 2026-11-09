"""
Creates a masked "_Reveal" child for every material instance under SOURCE_DIRS
whose base material is in MASTERS, and registers original -> twin in DATA_ASSET.

Safe to run again: existing twins are updated, not duplicated.
Run in the editor: Tools > Execute Python Script, or in the Output Log (Python):
    import create_reveal_twins; create_reveal_twins.run()
"""
import unreal

# Folders to scan for material instances.
SOURCE_DIRS = [
    "/Game/Assets/kb3d_missiontominerva/Materials",
]

# Masters that contain the UseReveal switch. Add more masters here once they have it.
MASTERS = {
    "/Game/Assets/kb3d_missiontominerva/Materials/Master/M_KB3D_Master_Standard",
}

DATA_ASSET = "/Game/Materials/PixelReveal/DA_PixelRevealSet"

SWITCH_NAME = "UseReveal"
SUFFIX = "_Reveal"

asset_lib = unreal.EditorAssetLibrary
mat_lib = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def _package_path(obj):
    # "/Game/X/M_A.M_A" -> "/Game/X/M_A"
    return obj.get_path_name().split(".")[0]


def _load_or_create_data_asset():
    if asset_lib.does_asset_exist(DATA_ASSET):
        return asset_lib.load_asset(DATA_ASSET)
    folder, name = DATA_ASSET.rsplit("/", 1)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.PixelRevealSet)
    return tools.create_asset(name, folder, unreal.PixelRevealSet, factory)


def _load_or_create_twin(original):
    original_path = _package_path(original)
    twin_path = original_path + SUFFIX
    if asset_lib.does_asset_exist(twin_path):
        return asset_lib.load_asset(twin_path)
    folder, name = twin_path.rsplit("/", 1)
    factory = unreal.MaterialInstanceConstantFactoryNew()
    factory.set_editor_property("initial_parent", original)
    return tools.create_asset(name, folder, unreal.MaterialInstanceConstant, factory)


def _configure_twin(twin, original):
    mat_lib.set_material_instance_parent(twin, original)
    mat_lib.set_material_instance_static_switch_parameter_value(twin, SWITCH_NAME, True)

    overrides = twin.get_editor_property("base_property_overrides")
    overrides.set_editor_property("override_blend_mode", True)
    overrides.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    twin.set_editor_property("base_property_overrides", overrides)

    mat_lib.update_material_instance(twin)
    asset_lib.save_loaded_asset(twin, only_if_is_dirty=False)


def run():
    data_asset = _load_or_create_data_asset()
    if data_asset is None:
        unreal.log_error("PixelReveal: could not create " + DATA_ASSET)
        return

    twins = data_asset.get_editor_property("twins")
    created = 0
    skipped = 0

    for source_dir in SOURCE_DIRS:
        for asset_path in asset_lib.list_assets(source_dir, recursive=True, include_folder=False):
            mi = asset_lib.load_asset(asset_path)
            if not isinstance(mi, unreal.MaterialInstanceConstant):
                continue
            if mi.get_name().endswith(SUFFIX):
                continue
            base = mi.get_base_material()
            if base is None or _package_path(base) not in MASTERS:
                skipped += 1
                continue

            twin = _load_or_create_twin(mi)
            if twin is None:
                unreal.log_warning("PixelReveal: could not create twin for " + asset_path)
                continue

            _configure_twin(twin, mi)
            twins[mi] = twin
            created += 1

    data_asset.set_editor_property("twins", twins)
    asset_lib.save_loaded_asset(data_asset, only_if_is_dirty=False)
    unreal.log("PixelReveal: {} twins created or updated, {} MIs skipped (other master).".format(created, skipped))


if __name__ == "__main__":
    run()
