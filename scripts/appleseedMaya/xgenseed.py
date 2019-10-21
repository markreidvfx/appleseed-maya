
#
# This source file is part of appleseed.
# Visit https://appleseedhq.net/ for additional information and resources.
#
# This software is released under the MIT license.
#
# Copyright (c) 2016-2019 Esteban Tovagliari, The appleseedhq Organization
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

# Standard imports.
import os

# XGen imports.
import xgenm as xg
import xgenm.xmaya.xgmExternalAPI as xgmExternalAPI

# Maya imports.
import maya
import maya.cmds as mc
import maya.mel as mel

# appleseedMaya imports.
import xgenseedutil
from logger import logger
import xgenseedui

def exportPalettes():
    if not mc.pluginInfo("xgenToolkit", query=True, loaded=True):
        return

    palettes = mc.ls(exactType="xgmPalette")
    if not palettes:
        return

    mel.eval('xgmPreview();xgmPreRendering();')

def exportPatches(startFrame=None, endFrame=None):

    if not mc.pluginInfo("xgenToolkit", query=True, loaded=True):
        return

    palettes = mc.ls(exactType="xgmPalette")
    if not palettes:
        return

    if not mc.pluginInfo("AbcExport", query=True, loaded=True):
        mc.loadPlugin("AbcExport")

    currentScenePath = mc.file(query=True, sceneName=True)
    if currentScenePath:
        dirPath = os.path.dirname(currentScenePath)
        sceneName, ext = os.path.splitext(os.path.basename(currentScenePath))
    else:
        xg.XGError(maya.stringTable['y_xgDescriptionEditor.kYouMustSaveTheSceneFirst'])
        return

    # TODO: Handle motion blur samples properly
    if startFrame is None:
        startFrame = mc.currentTime(query=True)

    if endFrame is None:
        endFrame = startFrame

    jobCmdBase = ' -frameRange {} {}'.format(startFrame, endFrame)
    jobCmdBase +=' -uvWrite -attrPrefix xgen -worldSpace'

    jobs = []
    for palette in palettes:

        filename = "{}/{}__{}.abc".format(dirPath, sceneName, xgmExternalAPI.encodeNameSpace(str(palette)))
        filename = filename.replace('__ns__', '__')

        descShapes = mc.listRelatives(palette, type="xgmDescription", allDescendents=True)

        jobCmd = jobCmdBase
        exportedMeshes = set()

        # find and export all the patch meshes used by the palette
        for shape in descShapes:
            descriptions = mc.listRelatives(shape, parent=True)
            if not descriptions:
                continue

            patches = xg.descriptionPatches(descriptions[0])
            for patch in patches:
                cmd = 'xgmPatchInfo -p "{}" -g'.format(patch)
                mesh = mel.eval(cmd)

                meshFullName = mc.ls(mesh, l=True)
                if not meshFullName:
                    continue

                root = meshFullName[0]
                # don't export the same mesh multiple times
                if root in exportedMeshes:
                    continue

                exportedMeshes.add(root)
                jobCmd += " -root {}".format(root)

        jobCmd += " -stripNamespaces -file '{}'".format(filename)
        jobs.append(jobCmd)

    cmd = "AbcExport -v -j "
    for job in jobs:
        cmd += '"{}"'.format(job)

    logger.info("XGen exporting abc patches")
    logger.info(cmd)
    mel.eval(cmd)

def appleseedExportFrame(self, frame, objFilename):
    '''Export a single appleseed archive frame.'''

    mc.file(
        objFilename + ".appleseedz",
        force=True,
        options="activeCamera=perspShape;",
        typ="appleseedz",
        es=True,
        pr=True,
        de=False
    )

    self.log("XGen appleseed export, filename = " + objFilename + ".appleseedz")


def appleseedExport(self, objs, filename, lod, materialNS):
    '''Export appleseed archives'''

    filename = self.nestFilenameInDirectory(filename, "appleseed")

    lastProgress = self.progress
    self.splitProgress(len(objs))

    self.log("appleseedExport " + filename + lod)

    # Force units to centimeters when exporting.
    prevUnits = mc.currentUnit(query=True, linear=True, fullName=True)
    mc.currentUnit(linear="centimeter")

    prevTime = mc.currentTime(query=True)

    for obj in objs:
        objFilename = filename + "_" + obj.replace("|", "_") + lod
        mc.select(obj, r=True)

        filenames = []
        # Choose to export single file or a sequence.
        frameToken = ""
        if self.startFrame != self.endFrame:
            frameToken = ".${FRAME}"

            dummyFrameFile = open(objFilename + frameToken + ".appleseed", "wt")
            dummyFrameFile.write("STARTFRAME=%4.4d\nENDFRAME=%4.4d\n" % (int(self.startFrame), int(self.endFrame)))
            dummyFrameFile.close()

            for curFrame in range(int(self.startFrame), int(self.endFrame) + 1):
                appleseedExportFrame(self, curFrame, objFilename + ".%4.4d" % int(curFrame))
        else:
            appleseedExportFrame(self, self.startFrame, objFilename)

        if self.curFiles != None:
            materials = self.getSGsFromObj(obj)
            if materials and len(materials) > 0:
                appleseedFilename = objFilename + frameToken + ".appleseedz"
                appleseedExportAppendFile(self, appleseedFilename, materialNS + materials[0], obj, lod)
        self.incProgress()

    mc.currentUnit(linear=prevUnits)
    mc.currentTime(prevTime)

    self.progress = lastProgress


def appleseedExportAppendFile(self, appleseedFilename, material, obj, lod):
    lodList = self.tweakLodAppend(self.curFiles, lod)
    for l in lodList:
        self.addArchiveFile("appleseed", appleseedFilename, material, "", l, 3)


def xgseedArchiveExportInit(selfid):
    '''Export Init callback. Must fill in some arrays on self.'''

    self = xgenseedutil.castSelf(selfid)
    self.batch_plugins.append("appleseedMaya")


def xgseedArchiveExportInfo(selfid):
    '''Export Info callback. Must fill in some arrays on self.'''

    self = xgenseedutil.castSelf(selfid)
    self.archiveDirs.append("appleseed")
    self.archiveLODBeforeExt.append(".${FRAME}.appleseed")
    self.archiveLODBeforeExt.append(".appleseed")
    self.archiveLODBeforeExt.append(".${FRAME}.appleseedz")
    self.archiveLODBeforeExt.append(".appleseedz")


def xgseedArchiveExport(selfid):
    '''
    Main archive export callback.
    Arguments are passed in self.invokeArgs
    '''

    self = xgenseedutil.castSelf(selfid)
    appleseedExport(
        self=self,
        objs=self.invokeArgs[0],
        filename=self.invokeArgs[1],
        lod=self.invokeArgs[2],
        materialNS=self.invokeArgs[3]
    )
