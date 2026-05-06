import os

import maya.cmds as cmds

SHELF_NAME = "ColliderTools"
PLUGIN_NAME = "ColliderTools"
ROOT_DIR = os.path.dirname(os.path.dirname(__file__))
ICON_DIR = os.path.join(ROOT_DIR, "icons")
PLUGIN_PATHS = [
    os.path.join(ROOT_DIR, "plug-ins", "ColliderTools.mll"),
    os.path.join(ROOT_DIR, "build", "Release", "ColliderTools.mll"),
]

_preview_nodes = []
_preview_source_selection = []
_preview_warning = ""


def _ensure_plugin():
    if cmds.pluginInfo(PLUGIN_NAME, query=True, loaded=True):
        return True
    for plugin_path in PLUGIN_PATHS:
        plugin_path = plugin_path.replace("\\", "/")
        if os.path.exists(plugin_path):
            cmds.loadPlugin(plugin_path)
            return True
    cmds.warning("ColliderTools.mll is not loaded and was not found in the installed module or local build output.")
    return False


def _option_var(name, default):
    if not cmds.optionVar(exists=name):
        cmds.optionVar(intValue=(name, default))
    return cmds.optionVar(query=name)


def _set_option_var(name, value):
    cmds.optionVar(intValue=(name, int(value)))


def _combine_value():
    return bool(_option_var("ct_combine", 0))


def _collider_transforms():
    result = []
    for group in ("|Colliders|UBX", "|Colliders|UCX", "|Colliders|UCP"):
        if not cmds.objExists(group):
            continue
        result.extend(cmds.listRelatives(group, children=True, fullPath=True, type="transform") or [])
    return result


def _run_create(collider_type, **kwargs):
    global _preview_warning
    if not _ensure_plugin():
        return []
    kwargs.setdefault("unreal", True)
    kwargs.setdefault("combine", _combine_value())
    before = set(_collider_transforms())
    try:
        result = cmds.ctCreateCollider(type=collider_type, **kwargs) or []
        _preview_warning = ""
    except RuntimeError as error:
        after = set(_collider_transforms())
        leaked = [node for node in after - before if cmds.objExists(node)]
        if leaked:
            cmds.delete(leaked)
            _delete_empty_collider_groups()
        message = str(error)
        if message != _preview_warning:
            cmds.warning("Collider preview failed: {0}".format(message))
            _preview_warning = message
        return []
    if isinstance(result, str):
        return [result]
    return list(result)


def create_box(*_):
    return _run_create("box")


def create_obox(*_):
    return _run_create("obox")


def create_hull(max_vertices=None, *_):
    if max_vertices is None:
        max_vertices = _option_var("ct_hullMaxVertices", 64)
    return _run_create("hull", maxVertices=int(max_vertices))


def create_cylinder(segments=None, *_):
    if segments is None:
        segments = _option_var("ct_cylinderSegments", 16)
    return _run_create("cylinder", segments=int(segments))


def create_capsule(segments=None, *_):
    if segments is None:
        segments = _option_var("ct_capsuleSegments", 16)
    return _run_create("capsule", segments=int(segments))


def _capture_source_selection():
    global _preview_source_selection
    selection = cmds.ls(selection=True, long=True, flatten=True) or []
    collider_groups = ("|Colliders", "|Colliders|UBX", "|Colliders|UCX", "|Colliders|UCP")
    _preview_source_selection = [node for node in selection if not any(node == group or node.startswith(group + "|") for group in collider_groups)]
    return list(_preview_source_selection)


def _resolve_node(node):
    if cmds.objExists(node):
        return node
    short_name = node.split("|")[-1]
    matches = cmds.ls(short_name, long=True) or []
    for match in matches:
        if match not in ("|Colliders", "|Colliders|UBX", "|Colliders|UCX", "|Colliders|UCP"):
            return match
    return None


def _mark_preview(nodes):
    for node in nodes:
        if cmds.objExists(node):
            if not cmds.attributeQuery("ctPreview", node=node, exists=True):
                cmds.addAttr(node, longName="ctPreview", attributeType="bool")
            cmds.setAttr(node + ".ctPreview", True)


def _unmark_preview(nodes):
    for node in nodes:
        if cmds.objExists(node) and cmds.attributeQuery("ctPreview", node=node, exists=True):
            cmds.deleteAttr(node + ".ctPreview")


def _preview_marked_nodes():
    result = []
    for group in ("|Colliders|UBX", "|Colliders|UCX", "|Colliders|UCP"):
        if not cmds.objExists(group):
            continue
        descendants = cmds.listRelatives(group, allDescendents=True, fullPath=True, type="transform") or []
        children = cmds.listRelatives(group, children=True, fullPath=True, type="transform") or []
        for node in children + descendants:
            if cmds.objExists(node) and cmds.attributeQuery("ctPreview", node=node, exists=True) and cmds.getAttr(node + ".ctPreview"):
                result.append(node)
    return list(dict.fromkeys(result))


def _delete_preview():
    global _preview_nodes
    existing = []
    for node in _preview_nodes + _preview_marked_nodes():
        resolved = _resolve_node(node)
        if resolved and resolved not in ("|Colliders", "|Colliders|UBX", "|Colliders|UCX", "|Colliders|UCP"):
            existing.append(resolved)
    if existing:
        cmds.delete(list(dict.fromkeys(existing)))
    _preview_nodes = []


def _delete_empty_collider_groups():
    for group in ("|Colliders|UBX", "|Colliders|UCX", "|Colliders|UCP"):
        if cmds.objExists(group) and not (cmds.listRelatives(group, children=True, fullPath=True) or []):
            cmds.delete(group)
    if cmds.objExists("|Colliders") and not (cmds.listRelatives("|Colliders", children=True, fullPath=True) or []):
        cmds.delete("|Colliders")


def _restore_source_selection():
    existing = [node for node in _preview_source_selection if cmds.objExists(node)]
    if existing:
        cmds.select(existing, replace=True)
    else:
        cmds.select(clear=True)


def _clear_preview_state():
    global _preview_nodes, _preview_source_selection
    _preview_nodes = []
    _preview_source_selection = []


def _rebuild_preview(collider_type, **kwargs):
    global _preview_nodes
    if not _preview_source_selection:
        _capture_source_selection()
    _delete_preview()
    _delete_empty_collider_groups()
    if not _preview_source_selection:
        return []
    _restore_source_selection()
    _preview_nodes = _run_create(collider_type, **kwargs)
    if not _preview_nodes:
        _delete_empty_collider_groups()
        return []
    _mark_preview(_preview_nodes)
    existing = [node for node in _preview_nodes if cmds.objExists(node)]
    if existing:
        cmds.select(existing, replace=True)
    return existing


def _accept_preview(window):
    _unmark_preview(_preview_nodes)
    _clear_preview_state()
    if cmds.window(window, exists=True):
        cmds.deleteUI(window)


def _cancel_preview(window):
    _delete_preview()
    _delete_empty_collider_groups()
    _restore_source_selection()
    _clear_preview_state()
    if cmds.window(window, exists=True):
        cmds.deleteUI(window)


def _delete_window(name):
    if cmds.window(name, exists=True):
        cmds.deleteUI(name)


def _options_window(window_name, title, slider_name, label, option_var, min_value, max_value, default_value, collider_type, value_kwarg):
    _delete_window(window_name)
    _delete_preview()
    _delete_empty_collider_groups()
    _capture_source_selection()

    window = cmds.window(window_name, title=title, titleBar=False, sizeable=False, minimizeButton=False, maximizeButton=False, resizeToFitChildren=True, closeCommand=lambda *_: _cancel_preview(window_name))
    root = cmds.columnLayout(adjustableColumn=True, rowSpacing=0, columnAttach=("both", 0), backgroundColor=(0.22, 0.22, 0.22))
    cmds.rowLayout(numberOfColumns=3, columnWidth3=(150, 16, 16), adjustableColumn=1, backgroundColor=(0.18, 0.18, 0.18))
    cmds.text(label=title, align="center", height=18, width=150, backgroundColor=(0.18, 0.18, 0.18))
    cmds.button(label="", width=14, height=14, command=lambda *_: _accept_preview(window_name), backgroundColor=(0.18, 0.36, 0.40))
    cmds.button(label="≡", width=16, height=16, command=lambda *_: None, backgroundColor=(0.18, 0.18, 0.18))
    cmds.setParent(root)

    value = max(min_value, min(int(_option_var(option_var, default_value)), max_value))
    field_name = slider_name + "Field"
    slider_control = slider_name + "Slider"
    combine_name = slider_name + "Combine"

    def controls_exist():
        return cmds.window(window_name, exists=True) and cmds.intField(field_name, exists=True)

    def read_values():
        if not controls_exist():
            return None
        field_value = cmds.intField(field_name, query=True, value=True)
        clamped = max(min_value, min(int(field_value), max_value))
        combine_value = cmds.checkBox(combine_name, query=True, value=True) if cmds.checkBox(combine_name, exists=True) else _combine_value()
        _set_option_var(option_var, clamped)
        _set_option_var("ct_combine", combine_value)
        return {value_kwarg: int(clamped), "combine": bool(combine_value)}

    def rebuild(*_):
        values = read_values()
        if values is None:
            return
        clamped = values[value_kwarg]
        cmds.intField(field_name, edit=True, value=clamped)
        if cmds.intSlider(slider_control, exists=True):
            cmds.intSlider(slider_control, edit=True, value=min(clamped, max_value))
        _rebuild_preview(collider_type, **values)

    def slider_changed(*_):
        if not controls_exist() or not cmds.intSlider(slider_control, exists=True):
            return
        cmds.intField(field_name, edit=True, value=cmds.intSlider(slider_control, query=True, value=True))
        rebuild()

    cmds.rowLayout(numberOfColumns=2, columnWidth2=(116, 66), adjustableColumn=1, columnAlign2=("left", "right"), backgroundColor=(0.22, 0.22, 0.22))
    cmds.text(label=label, align="left", height=18, width=112, backgroundColor=(0.22, 0.22, 0.22))
    cmds.intField(field_name, value=value, width=62, height=18, changeCommand=rebuild, enterCommand=rebuild, backgroundColor=(0.10, 0.10, 0.10))
    cmds.setParent(root)

    cmds.intSlider(slider_control, minValue=min_value, maxValue=max_value, value=value, width=182, height=10, dragCommand=slider_changed, changeCommand=slider_changed, backgroundColor=(0.22, 0.22, 0.22))

    combine = bool(_option_var("ct_combine", 0))
    cmds.rowLayout(numberOfColumns=2, columnWidth2=(116, 66), adjustableColumn=1, columnAlign2=("left", "right"), backgroundColor=(0.22, 0.22, 0.22))
    cmds.text(label="Combine", align="left", height=18, width=112, backgroundColor=(0.22, 0.22, 0.22))
    cmds.checkBox(combine_name, label="", value=combine, changeCommand=rebuild, height=18, backgroundColor=(0.22, 0.22, 0.22))
    cmds.setParent(root)

    cmds.rowLayout(numberOfColumns=2, columnWidth2=(91, 91), adjustableColumn=2, columnAlign2=("center", "center"), backgroundColor=(0.22, 0.22, 0.22))
    cmds.button(label="Accept", height=18, command=lambda *_: _accept_preview(window_name))
    cmds.button(label="Cancel", height=18, command=lambda *_: _cancel_preview(window_name))
    cmds.setParent(root)
    cmds.showWindow(window)
    rebuild()
    return window


def _show_history_manipulator():
    try:
        cmds.setToolTo("ShowManips")
    except RuntimeError:
        try:
            cmds.ShowManipulatorTool()
        except RuntimeError:
            pass


def show_hull_options(*_):
    max_vertices = _option_var("ct_hullMaxVertices", 64)
    result = _run_create("hull", maxVertices=int(max_vertices), history=True)
    _show_history_manipulator()
    return result


def show_cylinder_options(*_):
    segments = _option_var("ct_cylinderSegments", 16)
    result = _run_create("cylinder", segments=int(segments), history=True)
    _show_history_manipulator()
    return result


def show_capsule_options(*_):
    segments = _option_var("ct_capsuleSegments", 16)
    result = _run_create("capsule", segments=int(segments), history=True)
    _show_history_manipulator()
    return result


def _icon(name):
    return os.path.join(ICON_DIR, name).replace("\\", "/")


def _ensure_icon_path():
    current = os.environ.get("XBMLANGPATH", "")
    icon_path = ICON_DIR.replace("\\", "/")
    if icon_path not in current:
        os.environ["XBMLANGPATH"] = icon_path + os.pathsep + current if current else icon_path


def _shelf_layout():
    top_shelf = mel_eval("$tmp=$gShelfTopLevel")
    if not cmds.shelfLayout(SHELF_NAME, exists=True):
        cmds.shelfLayout(SHELF_NAME, parent=top_shelf)
    return SHELF_NAME


def _clear_shelf(shelf):
    children = cmds.shelfLayout(shelf, query=True, childArray=True) or []
    for child in children:
        cmds.deleteUI(child)


def _button(shelf, label, annotation, command, image):
    cmds.shelfButton(
        parent=shelf,
        label=label,
        annotation=annotation,
        image=image,
        sourceType="python",
        command=command,
    )


def mel_eval(command):
    import maya.mel as mel
    return mel.eval(command)


def install_shelf(*_):
    _ensure_icon_path()
    shelf = _shelf_layout()
    _clear_shelf(shelf)
    module_cmd = "import colliderTools; "
    _button(shelf, "UBX Box", "Create Unreal box collider from selection", module_cmd + "colliderTools.create_box()", _icon("qc_boxstraight.png"))
    _button(shelf, "UBX OBox", "Create Unreal oriented box collider from selection", module_cmd + "colliderTools.create_obox()", _icon("qc_boxaligned.png"))
    _button(shelf, "UCX Hull", "Live convex hull collider options", module_cmd + "colliderTools.show_hull_options()", _icon("qc_convex.png"))
    _button(shelf, "UCX Cyl", "Live cylinder collider options", module_cmd + "colliderTools.show_cylinder_options()", _icon("qc_capsulestraight.png"))
    _button(shelf, "UCP Cap", "Live capsule collider options", module_cmd + "colliderTools.show_capsule_options()", _icon("qc_capsulealigned.png"))
    cmds.shelfTabLayout(mel_eval("$tmp=$gShelfTopLevel"), edit=True, selectTab=shelf)
    return shelf
