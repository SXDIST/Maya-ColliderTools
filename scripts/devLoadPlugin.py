import sys
import maya.cmds as cmds
scripts_path = r"C:/Users/targaryen/source/repos/maya/collider-tools/scripts"
plugin_path = r"C:/Users/targaryen/source/repos/maya/collider-tools/build/Release/ColliderTools.mll"
if scripts_path not in sys.path:
    sys.path.append(scripts_path)
if cmds.pluginInfo("ColliderTools", query=True, loaded=True):
    cmds.unloadPlugin("ColliderTools")
cmds.loadPlugin(plugin_path)
import importlib
import colliderTools
importlib.reload(colliderTools)
colliderTools.install_shelf()