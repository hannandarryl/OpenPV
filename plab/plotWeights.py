#########################################
##  plotWeights.py
##  Written by Dylan Paiton, William Shainin
##  Dec 5, 2014
##  
##  Input is PetaVision weight struct, generated by pvAnalysis.py
##  Produces weight plot and makes file if requested
##  Output is handle to plot or -1 if no plot is requested
##
## TODO: Multi-layer networks - multiple methods
##          spike triggered average
##          regression
##          Gar's deconvolution method
##
#########################################

#TODO: Can we plan out the imports better so they are only imported once & when needed?
#      What is proper importing protocol?
import numpy as np
import matplotlib.pyplot as plt

def plotWeights(weightStruct,showPlot=True,savePlot=False,savePath=''):
    margin = 6 #pixels

    # weightStruct should be dims [time, numArbors, numPatches, nyp, nxp, nfp]
    weight_vals = np.array(weightStruct["values"])
    weight_time = weightStruct["time"]

    (numFrames,numArbors,numPatches,nyp,nxp,nfp) = weight_vals.shape
    i_frame = 400
    i_arbor = 0

    if np.sqrt(numPatches)%1 == 0: #If numPaches has a square root
        numPatchesX = np.sqrt(numPatches)
        numPatchesY = numPatchesX
    else:
        numPatchesX = np.floor(np.sqrt(numPatches))
        numPatchesY = numPatchesX+1

    patch_set = np.zeros((numPatchesX*numPatchesY,nyp+margin,nxp+margin))

    for i_patch in range(numPatches): 
        patch_tmp = weight_vals[i_frame,i_arbor,i_patch,:,:,:]
        min_patch = np.amin(patch_tmp)
        max_patch = np.amax(patch_tmp)
        # Normalize patch
        patch_tmp = (patch_tmp - min_patch) * 255 / (max_patch - min_patch + np.finfo(float).eps) # re-scaling & normalizing TODO: why? and what exactly is it doing?
        # Patches are padded with zeros - just fill in center
        patch_set[i_patch,np.floor(margin/2):np.floor(margin/2)+nyp,np.floor(margin/2):np.floor(margin/2)+nxp] = np.uint8(np.squeeze(np.transpose(patch_tmp,(1,0,2)))) # re-ordering to [x,y,f] TODO: why?

    #TODO: Does this actually reshape it how I want it to? (hint: no...)
    out_mat = np.reshape(patch_set,(numPatchesY*(nyp+margin),numPatchesX*(nxp+margin)),'C')

    plt.imshow(out_mat,cmap='Greys',interpolation='nearest')
    plt.show()

#TODO:
#def plotWeightHistograms(...):
