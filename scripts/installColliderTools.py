import os
import shutil
import sys

import maya.cmds as cmds

MODULE_NAME = "ColliderTools"
PLUGIN_NAME = "ColliderTools"
PLUGIN_FILE = "ColliderTools.mll"
MAYA_VERSION = cmds.about(version=True)
USER_APP_DIR = cmds.internalVar(userAppDir=True)
MODULES_DIR = os.path.join(USER_APP_DIR, MAYA_VERSION, "modules")
INSTALL_DIR = os.path.join(MODULES_DIR, MODULE_NAME)


def _repo_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _copy_tree(src, dst):
    if os.path.exists(dst):
        shutil.rmtree(dst)
    shutil.copytree(src, dst)


def _copy_plugin(src_plugin, install_dir):
    plugin_dir = os.path.join(install_dir, "plug-ins")
    if not os.path.isdir(plugin_dir):
        os.makedirs(plugin_dir)
    shutil.copy2(src_plugin, os.path.join(plugin_dir, PLUGIN_FILE))


def _write_module_file(modules_dir, install_dir):
    if not os.path.isdir(modules_dir):
        os.makedirs(modules_dir)
    module_file = os.path.join(modules_dir, MODULE_NAME + ".mod")
    module_path = install_dir.replace("\\", "/")
    with open(module_file, "w") as stream:
        stream.write("+ {0} 1.0 {1}\n".format(MODULE_NAME, module_path))
        stream.write("scripts: scripts\n")
        stream.write("icons: icons\n")
        stream.write("plug-ins: plug-ins\n")
    return module_file


def _load_installed_plugin(install_dir):
    plugin_path = os.path.join(install_dir, "plug-ins", PLUGIN_FILE).replace("\\", "/")
    if cmds.pluginInfo(PLUGIN_NAME, query=True, loaded=True):
        cmds.unloadPlugin(PLUGIN_NAME)
    cmds.loadPlugin(plugin_path)
    return plugin_path


def _install_shelf(install_dir):
    scripts_dir = os.path.join(install_dir, "scripts")
    if scripts_dir not in sys.path:
        sys.path.insert(0, scripts_dir)
    import colliderTools
    colliderTools.install_shelf()


def _default_plugin_path(root):
    package_plugin = os.path.join(root, PLUGIN_FILE)
    if os.path.isfile(package_plugin):
        return package_plugin
    return os.path.join(root, "build", "Release", PLUGIN_FILE)


def install(src_plugin=None, install_dir=INSTALL_DIR):
    root = _repo_root()
    if src_plugin is None:
        src_plugin = _default_plugin_path(root)
    if not os.path.isfile(src_plugin):
        raise RuntimeError("ColliderTools plugin was not found: {0}".format(src_plugin))

    if not os.path.isdir(install_dir):
        os.makedirs(install_dir)

    _copy_plugin(src_plugin, install_dir)
    _copy_tree(os.path.join(root, "scripts"), os.path.join(install_dir, "scripts"))
    _copy_tree(os.path.join(root, "icons"), os.path.join(install_dir, "icons"))
    module_file = _write_module_file(os.path.dirname(install_dir), install_dir)
    plugin_path = _load_installed_plugin(install_dir)
    _install_shelf(install_dir)

    cmds.inViewMessage(
        amg="ColliderTools installed",
        pos="topCenter",
        fade=True,
    )
    return {"module": module_file, "plugin": plugin_path, "installDir": install_dir}


def run():
    try:
        result = install()
    except Exception as error:
        cmds.warning("ColliderTools install failed: {0}".format(error))
        raise
    print("ColliderTools installed: {0}".format(result["installDir"]))
    return result


if __name__ == "__main__":
    run()
