#########################################
##  plotWeights.py
##  Written by Dylan Paiton, William Shainin
##  Dec 5, 2014
##
##  Input is PetaVision weight struct, generated by pvAnalysis.py
##  Produces weight plot and makes file if requested
##  Output is handle to plot or -1 if no plot is requested
##
## TODO: Multi-layer networks - multiple methods for displaying upper-layer elements
##          spike triggered average
##          regression (same as sta?)
##          Gar's deconvolution method
#########################################

#TODO: Can we plan out the imports better so they are only imported once & when needed?
#      What is proper importing protocol?
import numpy as np
import matplotlib.pyplot as plt # only need if showPlot==True OR savePlot==true
import os                       # only needed if savePlot==true
import pdb

def plotWeights(weightDat,activityDat=None,arborIdxList=None,i_frame=0,margin=0,plotColor=True,showPlot=False,savePlot=False,saveName=''):
    # NOTE: i_arbor and i_frame are indices for the given frame or arbor.
    #       They are not the actual arbor/frame number. This is because
    #       there may be a writeStep that is not 1.
    #

    # weightDat should be dims [time, numArbors, numPatches, nyp, nxp, nfp]
    weight_vals = np.array(weightDat["values"])
    weight_time = weightDat["time"]

    (numFrames,numArbors,numPatches,nyp,nxp,nfp) = weight_vals.shape

    if i_frame is -1:
        i_frame = numFrames-1

    if arborIdxList is None:
       arborIdxList = np.arange(numArbors)

    if i_frame > numFrames:
        print("Warning: i_frame > numFrames. Setting i_frame to numFrames.")
        i_frame = numFrames

    if activityDat:
        activity_vals = np.array(activityDat["values"])
        
        (numAFrames,ny,nx,nf) = activity_vals.shape
        sorted_indices = np.argsort(np.mean(np.mean(np.mean(activity_vals,0),0),0))

        weight_vals = weight_vals[:,:,sorted_indices[::-1],:,:,:]


    if np.sqrt(numPatches)%1 == 0: #If numPaches has a square root
        numPatchesX = np.sqrt(numPatches)
        numPatchesY = numPatchesX
    else:
        numPatchesX = np.ceil(np.sqrt(numPatches))
        numPatchesY = numPatchesX

    patch_set = np.zeros((numPatchesX*numPatchesY,nyp+margin,nxp+margin,nfp))
    out_mat   = np.zeros((numPatchesY*(nyp+margin),numPatchesX*(nxp+margin),nfp))

    half_margin = np.floor(margin/2)

    ##TODO: Since refactoring this I think we have lost a lot of efficiency.
    ##      It would be nice to refactor again with fewer redundant loops.

    # If time is stored via delayed arbors (as it usually is) then we need to rescale
    # and normalize across all arbors per patch index. Alternatively, time might be 
    # stored in the feature dimension, so we need to normalize across that dim as well.
    for i_patch in range(numPatches):
        min_patch = np.amin(weight_vals[i_frame,:,i_patch,:,:,:])
        max_patch = np.amax(weight_vals[i_frame,:,i_patch,:,:,:])
        new_max = 255
        new_min = 0

        ## PER PATCH - normalizing contrast
        ## Assign 0 to 128 (0 is gray)
        ## Per dictionary element (but across time), find max abs, make that value min & max 
        ##  i.e. max(abs(patch)) -> 255, -max(abs(patch)) -> 0
        ## Need to take max over time if time is there

        
        # Rescale & normalize patch. Scale patch to increase contrast, across space/time.
        #   NOTE: Rescaling will make low frequency (& thus low contrast) patches look like they are higher contrast
        weight_vals[i_frame,:,i_patch,:,:,:] = (weight_vals[i_frame,:,i_patch,:,:,:] - min_patch) * ((new_max-new_min) / (max_patch - min_patch + np.finfo(float).eps)) + new_min


    out_list = len(arborIdxList) * [None] # pre-allocate list to hold weight matrices
    for i_arbor in arborIdxList:
       if i_arbor > numArbors:
           print("Warning: i_arbor > numArbors. Setting i_arbor to numArbors.")
           i_arbor = numArbors

       xpos = 0
       ypos = 0
       for i_patch in range(numPatches):
           # Patches are padded with zeros - just fill in center. Ordered as [x,y,f]
           #patch_set[i_patch,half_margin:half_margin+nyp,half_margin:half_margin+nxp,:] = np.uint8(np.squeeze(np.transpose(weight_vals[i_frame,i_arbor,i_patch,:,:,:],(1,0,2)))) 
           # Patches are padded with zeros - just fill in center. Ordered as [y,x,f]
           try:
               patch_set[i_patch,half_margin:half_margin+nyp,half_margin:half_margin+nxp,:] = np.uint8(np.squeeze(weight_vals[i_frame,i_arbor,i_patch,:,:,:])) 
           except:
               pdb.set_trace()

           out_mat[ypos:ypos+nyp+margin,xpos:xpos+nxp+margin,:] = patch_set[i_patch,:,:,:]

           xpos += nxp+margin
           if xpos > out_mat.shape[1]-(nxp+margin):
               ypos += nyp+margin
               xpos = 0

       out_list[i_arbor] = out_mat

       #TODO: Really need to clean up plotting code... It would be best to push it out to its own function
       if showPlot:
           if plotColor:
               plt.figure()
               plt.imshow(np.squeeze(out_mat),cmap='Greys',interpolation='nearest')
               plt.show(block=False)
           else:
               for feat in range(nfp):
                   plt.figure()
                   plt.imshow(np.squeeze(out_mat[:,:,feat]),cmap='Greys',interpolation='nearest')
                   plt.show(block=False)
       if savePlot:
           if plotColor:
               if len(saveName) == 0:
                   fileName = 'plotWeightsOutput_fr'+str(i_frame).zfill(3)+'_a'+str(i_arbor).zfill(3)
                   if activityDat:
                       fileName = fileName+'_sorted'
                   fileExt  = 'png'
                   filePath = './'
                   saveName = filePath+fileName+'.'+fileExt
               else:
                   seps     = saveName.split(os.sep)
                   fileName = seps[-1]
                   filePath = saveName[0:-len(fileName)]
                   seps     = fileName.split(os.extsep)
                   fileExt  = seps[-1]
                   fileName = seps[0]
                   if activityDat:
                       fileName = fileName+'_sorted'
                   if not os.path.exists(filePath):
                       os.makedirs(filePath)
               plt.imsave(filePath+fileName+'_fr'+str(i_frame).zfill(3)+'_a'+str(i_arbor).zfill(3)+'.'+fileExt,np.squeeze(out_mat),vmin=0,vmax=255,cmap='Greys',origin='upper')
           else:
               for feat in range(nfp):
                   if len(saveName) == 0:
                       fileName = 'plotWeightsOutput_fr'+str(i_frame).zfill(3)+'_a'+str(i_arbor).zfill(3)+'_fe'+str(feat).zfill(3)
                       if activityDat:
                           fileName = fileName+'_sorted'
                       fileExt  = 'png'
                       filePath = './'
                       saveName = filePath+fileName+'.'+fileExt
                   else:
                       seps     = saveName.split(os.sep)
                       fileName = seps[-1]
                       filePath = saveName[0:-len(fileName)]
                       seps     = fileName.split(os.extsep)
                       fileExt  = seps[-1]
                       fileName = seps[0]
                       if activityDat:
                           fileName = fileName+'_sorted'
                       if not os.path.exists(filePath):
                           os.makedirs(filePath)
                   plt.imsave(filePath+fileName+'_fr'+str(i_frame).zfill(3)+'_a'+str(i_arbor).zfill(3)+'_fe'+str(feat).zfill(2)+'.'+fileExt,np.squeeze(out_mat[:,:,feat]),vmin=0,vmax=255,cmap='Greys',origin='upper')

    return out_list



#TODO:
#def plotWeightMovie(...):  # can receive weight file with multiple frames OR path to checkpoint folder
#def plotWeightHistograms(...):
#def plotActivationHistory(...): # activation histogram over time for each dictionary element
