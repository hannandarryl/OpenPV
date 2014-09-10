/*
 * HyPerConn.cpp
 *
 *  Created on: Oct 21, 2008
 *      Author: Craig Rasmussen
 */

#include "HyPerConn.hpp"
#include "../include/default_params.h"
#include "../io/io.h"
#include "../io/fileio.hpp"
#include "../utils/conversions.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <limits.h>
#include <iostream>
#include "../layers/accumulate_functions.h"
#include "../weightinit/InitWeights.hpp"
#include "../weightinit/InitGauss2DWeights.hpp"
#include "../weightinit/InitCocircWeights.hpp"
#include "../weightinit/InitSmartWeights.hpp"
#include "../weightinit/InitUniformRandomWeights.hpp"
#include "../weightinit/InitGaussianRandomWeights.hpp"
#include "../weightinit/InitGaborWeights.hpp"
#include "../weightinit/InitDistributedWeights.hpp"
#include "../weightinit/InitBIDSLateral.hpp"
#include "../weightinit/InitPoolWeights.hpp"
#include "../weightinit/InitRuleWeights.hpp"
#include "../weightinit/InitSubUnitWeights.hpp"
#include "../weightinit/InitOneToOneWeights.hpp"
#include "../weightinit/InitOneToOneWeightsWithDelays.hpp"
#include "../weightinit/InitIdentWeights.hpp"
#include "../weightinit/InitUniformWeights.hpp"
#include "../weightinit/InitByArborWeights.hpp"
#include "../weightinit/InitSpreadOverArborsWeights.hpp"
#include "../weightinit/Init3DGaussWeights.hpp"
#include "../weightinit/InitWindowed3DGaussWeights.hpp"
#include "../weightinit/InitMTWeights.hpp"
#include "../normalizers/NormalizeBase.hpp"
#include "../normalizers/NormalizeSum.hpp"
#include "../normalizers/NormalizeL2.hpp"
#include "../normalizers/NormalizeScale.hpp"
#include "../normalizers/NormalizeMax.hpp"
#include "../normalizers/NormalizeContrastZeroMean.hpp"
#include "TransposeConn.hpp"
#include "PlasticCloneConn.hpp"
#ifdef USE_SHMGET
   #include <sys/shm.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

//void HyPerLayer_recv_synaptic_input (
//      int kx, int ky, int lidx, int lidy, int nxl, int nyl,
//          int nxPre,
//          int nyPre,
//          int nfPre,
//          int nbPre,
//          int nxp,
//          int nyp,
//          int nfp,
//          float fScale,
//          float xScale,
//          float yScale,
//          size_t offsetA,
//          int * p2dLUT,
//           float * A,
//           float * W,
//           int Gstart,
//           float   * G);


#ifdef __cplusplus
}
#endif // __cplusplus

namespace PV {

HyPerConn::HyPerConn()
{
   initialize_base();
}

HyPerConn::HyPerConn(const char * name, HyPerCol * hc) {
   initialize_base();
   initialize(name, hc);
}

HyPerConn::~HyPerConn()
{
   delete normalizer;
   //if (parent->columnId() == 0) {
   //   io_timer->fprint_time(stdout);
   //   update_timer->fprint_time(stdout);
   //   fflush(stdout);
   //}
   delete io_timer;      io_timer     = NULL;
   delete update_timer;  update_timer = NULL;

   free(pvpatchAccumulateTypeString);
   free(name);

#if defined(PV_USE_OPENCL) || defined(PV_USE_CUDA)
   //if(gpuAccelerateFlag) {

//   if((gpuAccelerateFlag)&&(!ignoreGPUflag)) {
//      delete krRecvSyn;
//
   if (d_WData) {
      delete d_WData;
      d_WData = NULL;
   }
   if (d_Patches){
      delete d_Patches;
      d_Patches = NULL;
   }
   if (localWPatches){
      free(localWPatches);
      localWPatches = NULL;
   }
   if(d_GSynPatchStart){
      delete d_GSynPatchStart;
      d_GSynPatchStart = NULL;
   }
   if(localGSynPatchStart){
      free(localGSynPatchStart);
      localGSynPatchStart = NULL;
   }
   if(d_PostToPreActivity){ 
      delete d_PostToPreActivity;
      d_PostToPreActivity = NULL;
   }
   if(d_Patch2DataLookupTable){
      delete d_Patch2DataLookupTable;
      d_Patch2DataLookupTable = NULL;
   }
   if(krRecvPost){
      delete krRecvPost;
      krRecvPost = NULL;
   }
   if(krRecvPre){
      delete krRecvPre;
      krRecvPre = NULL;
   }
//
//      free(evRecvSynWaitList);
//      evRecvSynWaitList=NULL;
//      //delete gSynSemaphors;
//      //gSynSemaphors=NULL;
//   }
#endif // PV_USE_OPENCL

   deleteWeights();

   // free the task information

   // Moved to deleteWeights()
   // free(*gSynPatchStart); // All gSynPatchStart[k]'s were allocated together in a single malloc call.
   // free(gSynPatchStart);
   // free(*aPostOffset); // All aPostOffset[k]'s were allocated together in a single malloc call.
   // free(aPostOffset);

   free(fDelayArray);
   free(delays);
   for (int i_probe = 0; i_probe < this->numProbes; i_probe++){
      delete probes[i_probe];
   }
   free(this->probes);
   free(this->preLayerName);
   free(this->postLayerName);
   // free(this->filename);
   free(this->normalizeMethod);

   free(this->weightInitTypeString);
   delete weightInitializer;
   delete randState;

   if(this->postToPreActivity){
      free(this->postToPreActivity);
      this->postToPreActivity = NULL;
   }

   if (triggerLayerName) {
      free(triggerLayerName);
      triggerLayerName = NULL;
   }
   free(numKernelActivations);
   if (mpiReductionBuffer) {
      free(mpiReductionBuffer);
      mpiReductionBuffer = NULL;
   }
}

//!
/*!
 *
 *
 *
 */
int HyPerConn::initialize_base()
{
   this->name = strdup("Unknown");
   this->nxp = 1;
   this->nyp = 1;
   this->nxpShrunken = nxp;
   this->nypShrunken = nyp;
   this->offsetShrunken = 0;
   this->nfp = -1; // A negative value for nfp will be converted to postsynaptic layer's nf.
   this->warnDefaultNfp = true;  // Issue a warning if default value of nfp (post's nf) is used.  Derived layers can set to false if only one nfp is allowed (e.g. IdentConn)
   this->sxp = 1;
   this->syp = 1;
   this->sfp = 1;
   this->parent = NULL;
   this->connId = 0;
   this->preLayerName = NULL;
   this->postLayerName = NULL;
   this->pre = NULL;
   this->post = NULL;
   // this->filename = NULL;
   this->numAxonalArborLists = 1;
   this->channel = CHANNEL_EXC;
   this->ioAppend = false;

   this->weightInitTypeString = NULL;
   this->weightInitializer = NULL;
   this->initializeFromCheckpointFlag = false;

   this->probes = NULL;
   this->numProbes = 0;

   this->io_timer     = NULL;
   this->update_timer = NULL;

   this->wMin = 0.0;
   this->wMax = 1.0;
   this->wPostTime = -1.0;
   this->wPostPatches = NULL;
   this->wPostDataStart = NULL;
   this->wPostPatchesp = NULL;
   this->wPostDataStartp = NULL;
   this->nxpPost = 0;
   this->nypPost = 0;
   this->nfpPost = 0;
   this->writeCompressedWeights = false;
   this->writeCompressedCheckpoints = false;
   this->fileType = PVP_WGT_FILE_TYPE; // Subclass's initialize_base() gets called after HyPerConn's initialize_base(), so this can be changed in subclasses.

   wDataStart = NULL;
   dwDataStart = NULL;
   wPatches=NULL;
   aPostOffset = NULL;
   gSynPatchStart = NULL;

   this->selfFlag = false;  // specifies whether connection is from a layer to itself (i.e. a self-connection)
   this->combine_dW_with_W_flag = false;
   this->normalizeMethod = NULL;
   this->normalizer = NULL;
   this->plasticityFlag = false;
   this->shrinkPatches_flag = false; // default value, overridden by params file parameter "shrinkPatches" in readShrinkPatches()
   this->shrinkPatchesThresh = 0;
   this->normalizeArborsIndividually = true;
   this->normalize_max = false;
   this->normalize_zero_offset = false;
   this->normalize_cutoff = 0.0f;
   this->normalize_RMS_amp = false;
   this->dWMax            = 1;
   this->strengthParamHasBeenWritten = false;

   this->fDelayArray = NULL;
   this->delays = NULL;

   //This flag is only set otherwise in kernelconn
   this->useWindowPost = false;

   this->updateGSynFromPostPerspective = false;

   this->pvpatchAccumulateTypeString = NULL;
   this->pvpatchAccumulateType = ACCUMULATE_CONVOLVE;

   this->initInfoCommunicatedFlag = false;
   this->dataStructuresAllocatedFlag = false;
   this->initialValuesSetFlag = false;

   this->randState = NULL;

   this->triggerFlag = false; //Default to update every timestamp
   this->triggerLayer = NULL;
   this->triggerLayerName = NULL;
   this->triggerOffset = 0;

   this->clones.clear();

   this->postToPreActivity = NULL;

   lastUpdateTime = 0.f;
   symmetrizeWeightsFlag = false;
   patch2datalookuptable = NULL;
   numKernelActivations = NULL;
   keepKernelsSynchronized_flag = false;
   mpiReductionBuffer = NULL;

#ifdef USE_SHMGET
   shmget_flag = false;
   shmget_owner = NULL;
   shmget_id = NULL;
#endif

#if defined(PV_USE_OPENCL) || defined(PV_USE_CUDA)
   receiveGpu = false;
   allocDeviceWeights = false;
   d_WData = NULL;
   d_Patches = NULL;
   d_GSynPatchStart = NULL;
   d_PostToPreActivity = NULL;
   d_Patch2DataLookupTable = NULL;
   localWPatches = NULL;
   localGSynPatchStart = NULL;
   krRecvPost = NULL;
   krRecvPre = NULL;
   updatedDeviceWeights = true; //Start off as always updated
   numXLocal= 1;
   numYLocal= 1;
   numFLocal = 1;
   postGroupXSize = 1;
   postGroupYSize = 1;
   numPostGroupX = 1;
   numPostGroupY = 1;
#endif

   return PV_SUCCESS;
}

int HyPerConn::createArbors() {
#ifdef USE_SHMGET
   if (shmget_flag){
      assert(sharedWeights);
      shmget_id = (int *) calloc(this->numberOfAxonalArborLists(),
            sizeof(int));
      assert(shmget_id != NULL);
      shmget_owner = (bool *) calloc(this->numberOfAxonalArborLists(),
            sizeof(bool));
      assert(shmget_owner != NULL);
   }
#endif
   wPatches = (PVPatch***) calloc(numAxonalArborLists, sizeof(PVPatch**));
   if( wPatches == NULL ) {
      createArborsOutOfMemory();
      assert(false);
   }
   // GTK:  gSynPatchStart redefined as offset form beginning of gSyn buffer for the corresponding channel
   gSynPatchStart = (size_t **) calloc( numAxonalArborLists, sizeof(size_t *) );
   if( gSynPatchStart == NULL ) {
      createArborsOutOfMemory();
      assert(false);
   }
   size_t * gSynPatchStartBuffer = (size_t *) calloc(
		   (this->shrinkPatches_flag ? numAxonalArborLists : 1)
		   * preSynapticLayer()->getNumExtended(), sizeof(size_t));
   if (gSynPatchStartBuffer == NULL) {
      createArborsOutOfMemory();
      assert(false);
   }
   for (int k = 0; k < numAxonalArborLists; k++) {
      gSynPatchStart[k] = gSynPatchStartBuffer
            + this->shrinkPatches_flag * k * preSynapticLayer()->getNumExtended();
   }

   aPostOffset = (size_t **) calloc(numAxonalArborLists, sizeof(size_t *));
   if( aPostOffset == NULL ) {
      createArborsOutOfMemory();
      assert(false);
   }
   size_t * aPostOffsetBuffer = (size_t *) calloc(
         (this->shrinkPatches_flag ? numAxonalArborLists : 1)
               * preSynapticLayer()->getNumExtended(), sizeof(size_t));
   if( aPostOffsetBuffer == NULL ) {
      createArborsOutOfMemory();
      assert(false);
   }
   for( int k=0; k<numAxonalArborLists; k++ ) {
      aPostOffset[k] = aPostOffsetBuffer
            + this->shrinkPatches_flag * k * preSynapticLayer()->getNumExtended();
   }
   wDataStart = (pvwdata_t **) calloc(numAxonalArborLists, sizeof(pvwdata_t *));
   if( wDataStart == NULL ) {
      createArborsOutOfMemory();
      assert(false);
   }
   dwDataStart = (pvwdata_t **) calloc(numAxonalArborLists, sizeof(pvwdata_t *));
   if( dwDataStart == NULL ) {
      createArborsOutOfMemory();
      assert(false);
   }
   return PV_SUCCESS;
}

void HyPerConn::createArborsOutOfMemory() {
   connOutOfMemory("HyPerConn::createArbors()");
}


//!
/*!
 * REMARKS:
 *      - Each neuron in the pre-synaptic layer can project "up"
 *      a number of arbors. Each arbor connects to a patch in the post-synaptic
 *      layer.
 *      - writeTime and writeStep are used to write post-synaptic patches.These
 *      patches are written every writeStep.
 *      .
 */
int HyPerConn::constructWeights()
{
   int sx = nfp;
   int sy = sx * nxp;
   int sp = sy * nyp;
   int nPatches = getNumDataPatches();
   int status = PV_SUCCESS;

   assert(!parent->parameters()->presentAndNotBeenRead(name, "shrinkPatches"));
   // createArbors() uses the value of shrinkPatches.  It should have already been read in ioParamsFillGroup.
   //allocate the arbor arrays:
   createArbors();

   setPatchStrides();

   //allocate weight patches and axonal arbors for each arbor
   //Allocate all the weights
   wDataStart[0] = allocWeights(nPatches, nxp, nyp, nfp);
   assert(this->get_wDataStart(0) != NULL);
   for (int arborId=0;arborId<numAxonalArborLists;arborId++) {
      status = createWeights(wPatches, arborId);
      assert(wPatches[arborId] != NULL);

      if (arborId > 0){  // wDataStart already allocated
         wDataStart[arborId] = (this->get_wDataStart(0) + sp * nPatches * arborId);
         assert(this->wDataStart[arborId] != NULL);
      }
      if (shrinkPatches_flag || arborId == 0){
         status |= adjustAxonalArbors(arborId);
      }
   }  // arborId

   //call to initializeWeights moved to initializeState()
   status |= initPlasticityPatches();
   assert(status == 0);
   if (shrinkPatches_flag) {
      for (int arborId=0;arborId<numAxonalArborLists;arborId++) {
         shrinkPatches(arborId);
      }
   }

   return status;
}

int HyPerConn::shrinkPatches(int arborId) {
   int numPatches = getNumWeightPatches();
   for (int kex = 0; kex < numPatches; kex++) {
      shrinkPatch(kex, arborId /* arbor */ );
   } // loop over pre-synaptic neurons

   return 0;
}

int HyPerConn::shrinkPatch(int kExt, int arborId) {

   int kIndex = patchToDataLUT(kExt);

   PVPatch *weights = getWeights(kExt,arborId);

   pvwdata_t * w = &get_wDataStart(arborId)[kIndex*nxp*nyp*nfp+weights->offset];

   int nx = weights->nx;
   int ny = weights->ny;

   int maxnx = INT_MIN;
   int minnx = INT_MAX;
   int maxny = INT_MIN;
   int minny = INT_MAX;

   bool nonZeroWeightFound = false;
   // loop over all post-synaptic cells in patch
   for (int y = 0; y < ny; y++) {
      for (int x = 0; x < nx; x++) {
         for (int f = 0; f < nfp; f++) {
            if(abs(w[x * sxp + y * syp + f * sfp]) <= shrinkPatchesThresh) {
               nonZeroWeightFound=true;
               maxnx = maxnx < x ? x : maxnx;
               minnx = minnx > x ? x : minnx;
               maxny = maxny < y ? y : maxny;
               minny = minny > y ? y : minny;
            }
         }
      }
   }
   
   if(nonZeroWeightFound) {
      //Plus one to capture all of the patch
      int nxNew = maxnx+1 - minnx;
      int nyNew = maxny+1 - minny;
      int dxNew = minnx;
      int dyNew = minny;

      // adjust patch size (shrink) to fit within interior of post-synaptic layer
      //
      pvpatch_adjust(weights, sxp, syp, nxNew, nyNew, dxNew, dyNew);

      gSynPatchStart[arborId][kExt] += dxNew*getPostNonextStrides()->sx + dyNew*getPostNonextStrides()->sy;
      aPostOffset[arborId][kExt] += dxNew*getPostExtStrides()->sx + dyNew*getPostExtStrides()->sy; // Someone who uses these routines, please check that this is correct.
   }
   return 0;
}


int HyPerConn::initialize(const char * name, HyPerCol * hc) {
   int status = PV_SUCCESS;
   if (status == PV_SUCCESS) status = setParent(hc);
   if (status == PV_SUCCESS) status = setName(name);
   if (status == PV_SUCCESS) status = ioParams(PARAMS_IO_READ);

   assert(parent);
   PVParams * inputParams = parent->parameters();

   //set accumulateFunctionPointer
   assert(!inputParams->presentAndNotBeenRead(name, "pvpatchAccumulateType"));
   switch (pvpatchAccumulateType) {
   case ACCUMULATE_CONVOLVE:
      accumulateFunctionPointer  = &pvpatch_accumulate;
      accumulateFunctionFromPostPointer = &pvpatch_accumulate_from_post;
      break;
   case ACCUMULATE_STOCHASTIC:
      accumulateFunctionPointer = &pvpatch_accumulate_stochastic;
      accumulateFunctionFromPostPointer = &pvpatch_accumulate_stochastic_from_post;
      break;
   case ACCUMULATE_MAXPOOLING:
      accumulateFunctionPointer = &pvpatch_max_pooling;
      accumulateFunctionFromPostPointer = &pvpatch_max_pooling_from_post;
      break;
   default:
      assert(0);
      break;
   }
   // if (filename!=NULL) {
   //    status |= readPatchSizeFromFile(filename);
   // }

   ioAppend = parent->getCheckpointReadFlag();

//This has been commented out because layers will decide if GPU acceleration
//will happen and they will call the init methods as necessary
//#ifdef PV_USE_OPENCL
//   initializeThreadBuffers("HyPerLayer_recv_synaptic_input");
//   initializeThreadKernels("HyPerLayer_recv_synaptic_input");
//#endif // PV_USE_OPENCL

//Post here is not set, moved to communicate
//#ifdef PV_USE_OPENCL
//   gpuAccelerateFlag=post->getUseGPUFlag();
//#endif

   this->connId = parent->addConnection(this);

   this->io_timer     = new Timer(getName(), "conn", "io     ");
   this->update_timer = new Timer(getName(), "conn", "update ");

   return status;
}

int HyPerConn::setPreAndPostLayerNames() {
   return getPreAndPostLayerNames(name, parent->parameters(), &preLayerName, &postLayerName);
}

//int HyPerConn::setFilename() {
//   PVParams * inputParams = parent->parameters();
//   return setFilename(inputParams->stringValue(name, "initWeightsFile"));
//}

int HyPerConn::setWeightInitializer() {
   weightInitializer = createInitWeightsObject(weightInitTypeString);
   if( weightInitializer == NULL ) {
      weightInitializer = getDefaultInitWeightsMethod(parent->parameters()->groupKeywordFromName(name));
   }
   return weightInitializer==NULL ? PV_FAILURE : PV_SUCCESS;
}

/*
 * This method parses the weightInitType parameter and creates an
 * appropriate InitWeight object for the chosen weight initialization.
 *
 */
InitWeights * HyPerConn::createInitWeightsObject(const char * weightInitTypeStr) {

   if(( weightInitTypeStr!=0 )&&(!strcmp(weightInitTypeStr, "Gauss2DWeight"))) {
      weightInitializer = new InitGauss2DWeights(this);
   }
   else if(( weightInitTypeStr!=0 )&&(!strcmp(weightInitTypeStr, "CoCircWeight"))) {
      weightInitializer = new InitCocircWeights(this);
   }
   else if(( weightInitTypeStr!=0 )&&(!strcmp(weightInitTypeStr, "UniformWeight"))) {
      weightInitializer = new InitUniformWeights(this);
   }
   else if(( weightInitTypeStr!=0 )&&(!strcmp(weightInitTypeStr, "SmartWeight"))) {
      weightInitializer = new InitSmartWeights(this);
   }
   else if(( weightInitTypeStr!=0 )&&(!strcmp(weightInitTypeStr, "DistributedWeight"))) {
      weightInitializer = new InitDistributedWeights(this);
   }
   else if(( weightInitTypeStr!=0 )&&(!strcmp(weightInitTypeStr, "ArborWeight"))) {
      weightInitializer = new InitByArborWeights(this);
   }
   else if(( weightInitTypeStr!=0 )&&(!strcmp(weightInitTypeStr, "BIDSLateral"))) {
      weightInitializer = new InitBIDSLateral(this);
   }
   else if(( weightInitTypeStr!=0 )&&(!strcmp(weightInitTypeStr, "UniformRandomWeight"))) {
      weightInitializer = new InitUniformRandomWeights(this);
   }
   else if(( weightInitTypeStr!=0 )&&(!strcmp(weightInitTypeStr, "GaussianRandomWeight"))) {
      weightInitializer = new InitGaussianRandomWeights(this);
   }
   else if(( weightInitTypeStr!=0 )&&(!strcmp(weightInitTypeStr, "GaborWeight"))) {
      weightInitializer = new InitGaborWeights(this);
   }
   else if(( weightInitTypeStr!=0 )&&(!strcmp(weightInitTypeStr, "PoolWeight"))) {
      weightInitializer = new InitPoolWeights(this);
   }
   else if(( weightInitTypeStr!=0 )&&(!strcmp(weightInitTypeStr, "RuleWeight"))) {
      weightInitializer = new InitRuleWeights(this);
   }
   else if(( weightInitTypeStr!=0 )&&(!strcmp(weightInitTypeStr, "SubUnitWeight"))) {
      weightInitializer = new InitSubUnitWeights(this);
   }
   else if(( weightInitTypeStr!=0 )&&(!strcmp(weightInitTypeStr, "IdentWeight"))) {
      weightInitializer = new InitIdentWeights(this);
   }
   else if(( weightInitTypeStr!=0 )&&(!strcmp(weightInitTypeStr, "OneToOneWeights"))) {
      weightInitializer = new InitOneToOneWeights(this);
   }
   else if(( weightInitTypeStr!=0 )&&(!strcmp(weightInitTypeStr, "OneToOneWeightsWithDelays"))) {
      weightInitializer = new InitOneToOneWeightsWithDelays(this);
   }
   else if(( weightInitTypeStr!=0 )&&(!strcmp(weightInitTypeStr, "SpreadOverArborsWeight"))) {
      weightInitializer = new InitSpreadOverArborsWeights(this);
   }
   else if(( weightInitTypeStr!=0 )&&(!strcmp(weightInitTypeStr, "Gauss3DWeight"))) {
      weightInitializer = new Init3DGaussWeights(this);
   }
   else if(( weightInitTypeStr!=0 )&&(!strcmp(weightInitTypeStr, "Windowed3DGaussWeights"))) {
      weightInitializer = new InitWindowed3DGaussWeights(this);
   }
   else if(( weightInitTypeStr!=0 )&&(!strcmp(weightInitTypeStr, "MTWeight"))) {
      weightInitializer = new InitMTWeights(this);
   }
   else if(( weightInitTypeStr!=0 )&&(!strcmp(weightInitTypeStr, "FileWeight"))) {
      weightInitializer = new InitWeights(this);
   }
   else {
      weightInitializer = NULL;
   }

   return weightInitializer;
}


int HyPerConn::setParent(HyPerCol * hc) {
   assert(parent==NULL);
   if(hc==NULL) {
      int rank = 0;
#ifdef PV_USE_MPI
      MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
      fprintf(stderr, "HyPerConn error in rank %d process: constructor called with HyPerCol set to the null pointer.\n", rank);
      exit(EXIT_FAILURE);
   }
   parent = hc;
   return PV_SUCCESS;
}

int HyPerConn::ioParams(enum ParamsIOFlag ioFlag)
{
   parent->ioParamsStartGroup(ioFlag, name);
   ioParamsFillGroup(ioFlag);
   parent->ioParamsFinishGroup(ioFlag);

   return PV_SUCCESS;
}

int HyPerConn::setName(const char * name) {
   assert(parent!=NULL);
   if(name==NULL) {
      fprintf(stderr, "HyPerConn error in rank %d process: constructor called with name set to the null pointer.\n", parent->columnId());
      exit(EXIT_FAILURE);
   }
   free(this->name);  // name will already have been set in initialize_base()
   this->name = strdup(name);
   if (this->name==NULL) {
      fprintf(stderr, "Connection \"%s\" error in rank %d process: unable to allocate memory for name of connection: %s\n",
            name, parent->columnId(), strerror(errno));
      exit(EXIT_FAILURE);
   }
   return PV_SUCCESS;
}

int HyPerConn::setPreLayerName(const char * pre_name) {
   assert(parent!=NULL);
   assert(this->preLayerName==NULL);
   if (pre_name != NULL) {
      this->preLayerName = strdup(pre_name);
      if (this->preLayerName==NULL) {
         fprintf(stderr, "Connection \"%s\" error in rank %d process: unable to allocate memory for name of presynaptic layer \"%s\": %s\n",
               name, parent->columnId(), pre_name, strerror(errno));
         exit(EXIT_FAILURE);
      }
   }
   return PV_SUCCESS;
}

int HyPerConn::setPostLayerName(const char * post_name) {
   assert(this->postLayerName==NULL);
   if (post_name != NULL) {
      this->postLayerName = strdup(post_name);
      if (this->postLayerName==NULL) {
         fprintf(stderr, "Connection \"%s\" error in rank %d process: unable to allocate memory for name of postsynaptic layer \"%s\": %s\n",
               name, parent->columnId(), post_name, strerror(errno));
         exit(EXIT_FAILURE);
      }
   }
   return PV_SUCCESS;
}

int HyPerConn::getPreAndPostLayerNames(const char * name, PVParams * params, char ** preLayerNamePtr, char ** postLayerNamePtr) {
   // Retrieves preLayerName and postLayerName from parameter group whose name is given in the functions first argument.
   // This routine uses strdup to fill *{pre,post}LayerNamePtr, so the routine calling this one is responsible for freeing them.
   int status = PV_SUCCESS;
   *preLayerNamePtr = NULL;
   *postLayerNamePtr = NULL;
   const char * preLayerNameParam = params->stringValue(name, "preLayerName", false);
   const char * postLayerNameParam = params->stringValue(name, "postLayerName", false);
   if (preLayerNameParam != NULL && postLayerNameParam != NULL) {
      *preLayerNamePtr = strdup(preLayerNameParam);
      *postLayerNamePtr = strdup(postLayerNameParam);
   }
   else if (preLayerNameParam==NULL && postLayerNameParam!=NULL) {
      status = PV_FAILURE;
      if (params->getInterColComm()->commRank()==0) {
         fprintf(stderr, "Connection \"%s\" error: if postLayerName is specified, preLayerName must be specified as well.\n", name);
      }
   }
   else if (preLayerNameParam!=NULL && postLayerNameParam==NULL) {
      status = PV_FAILURE;
      if (params->getInterColComm()->commRank()==0) {
         fprintf(stderr, "Connection \"%s\" error: if preLayerName is specified, postLayerName must be specified as well.\n", name);
      }
   }
   else {
      assert(preLayerNameParam==NULL && postLayerNameParam==NULL);
      if (params->getInterColComm()->commRank()==0) {
         printf("Connection \"%s\": preLayerName and postLayerName will be inferred in the communicateInitInfo stage.\n", name);
      }
   }
#ifdef PV_USE_MPI
   MPI_Barrier(params->getInterColComm()->communicator());
#endif
   if (status != PV_SUCCESS) {
      exit(EXIT_FAILURE);
   }
   return status;
}

int HyPerConn::handleMissingPreAndPostLayerNames() {
   return inferPreAndPostFromConnName(name, parent->parameters(), &preLayerName, &postLayerName);
}

int HyPerConn::inferPreAndPostFromConnName(const char * name, PVParams * params, char ** preLayerNamePtr, char ** postLayerNamePtr) {
   // If the connection name has the form "ABC to XYZ", then pre will be ABC and post will be XYZ.
   // If either of the intended pre- or post-layer names contains the string " to ", this method cannot be used to infer them.
   // This routine uses malloc to fill *{pre,post}LayerNamePtr, so the routine calling this one is responsible for freeing them.

   int status = PV_SUCCESS;
   // Check to see if the string " to " appears exactly once in name
   // If so, use part preceding " to " as pre-layer, and part after " to " as post.
   const char * separator = " to ";
   const char * locto = strstr(name, separator);
   if( locto != NULL ) {
      const char * nextto = strstr(locto+1, separator); // Make sure " to " doesn't appear again.
      if( nextto == NULL ) {
         int seplen = strlen(separator);

         int pre_len = locto - name;
         *preLayerNamePtr = (char *) malloc((size_t) (pre_len + 1));
         if( *preLayerNamePtr==NULL) {
            fprintf(stderr, "Error: unable to allocate memory for preLayerName in connection \"%s\": %s\n", name, strerror(errno));
            exit(EXIT_FAILURE);
         }
         const char * preInConnName = name;
         memcpy(*preLayerNamePtr, preInConnName, pre_len);
         (*preLayerNamePtr)[pre_len] = 0;

         int post_len = strlen(name)-pre_len-seplen;
         *postLayerNamePtr = (char *) malloc((size_t) (post_len + 1));
         if( *postLayerNamePtr==NULL) {
            fprintf(stderr, "Error: unable to allocate memory for postLayerName in connection \"%s\": %s\n", name, strerror(errno));
            exit(EXIT_FAILURE);
         }
         const char * postInConnName = &name[pre_len+seplen];
         memcpy(*postLayerNamePtr, postInConnName, post_len);
         (*postLayerNamePtr)[post_len] = 0;
      }
      else {
         status = PV_FAILURE;
         if (params->getInterColComm()->commRank()==0) {
            fprintf(stderr, "Unable to infer pre and post from connection name \"%s\":\n", name);
            fprintf(stderr, "The string \" to \" cannot appear in the name more than once.\n");
         }
      }
   }
   else {
      status = PV_FAILURE;
      if (params->getInterColComm()->commRank()==0) {
         fprintf(stderr, "Unable to infer pre and post from connection name \"%s\".\n", name);
         fprintf(stderr, "The connection name must have the form \"ABC to XYZ\", to infer the names,\n");
         fprintf(stderr, "but the string \" to \" does not appear.\n");
      }
   }
   return status;
}

int HyPerConn::initNumWeightPatches() {
   numWeightPatches = pre->getNumExtended();
   return PV_SUCCESS;
}

int HyPerConn::initNumDataPatches() {
   if (sharedWeights) {
      int nxKernel = (pre->getXScale() < post->getXScale()) ? (int)pow(2,
            post->getXScale() - pre->getXScale()) : 1;
      int nyKernel = (pre->getYScale() < post->getYScale()) ? (int)pow(2,
            post->getYScale() - pre->getYScale()) : 1;
      numDataPatches = pre->getLayerLoc()->nf * nxKernel * nyKernel;
      return PV_SUCCESS;
   }
   else {
      numDataPatches = getNumWeightPatches();
   }
   return PV_SUCCESS;
}

int HyPerConn::initPlasticityPatches()
{
// Copied over from KernelConn, but do we need it?
//    assert(!parent->parameters()->presentAndNotBeenRead(name, "sharedWeights"));
//    assert(!parent->parameters()->presentAndNotBeenRead(name, "plasticityFlag"));
// #ifdef PV_USE_MPI
//    if( usingSharedWeights() && getPlasticityFlag() ) {
//       assert(!parent->parameters()->presentAndNotBeenRead(name, "keepKernelsSynchronized"));
//    }
// #endif // PV_USE_MPI
   if (!plasticityFlag) return PV_SUCCESS;
   int sx = nfp;
   int sy = sx * nxp;
   int sp = sy * nyp;
   int nPatches = getNumDataPatches();

   const int numAxons = numberOfAxonalArborLists();

   if (this->combine_dW_with_W_flag){
      dwDataStart = wDataStart;
      return PV_SUCCESS;
   }
   dwDataStart[0] = allocWeights(nPatches, nxp, nyp, nfp);
   assert(this->get_dwDataStart(0) != NULL);
   for (int arborId = 0; arborId < numAxons; arborId++) {
      dwDataStart[arborId] = (dwDataStart[0] + sp * nPatches * arborId);
      assert(get_dwDataStart(arborId) != NULL);
   } // loop over arbors

   return PV_SUCCESS;
}

// set member variables specified by user
int HyPerConn::ioParamsFillGroup(enum ParamsIOFlag ioFlag)
{
   ioParam_preLayerName(ioFlag);
   ioParam_postLayerName(ioFlag);
   ioParam_channelCode(ioFlag);
   // ioParam_initWeightsFile(ioFlag);
   ioParam_sharedWeights(ioFlag);
   ioParam_weightInitType(ioFlag);
   if (weightInitializer != NULL) {
      weightInitializer->ioParamsFillGroup(ioFlag);
   }
   ioParam_initializeFromCheckpointFlag(ioFlag);
   ioParam_numAxonalArbors(ioFlag);
   ioParam_plasticityFlag(ioFlag);
   ioParam_weightUpdatePeriod(ioFlag);
   ioParam_initialWeightUpdateTime(ioFlag);
   ioParam_triggerFlag(ioFlag);
   ioParam_triggerLayerName(ioFlag);
   ioParam_triggerOffset(ioFlag);
   ioParam_pvpatchAccumulateType(ioFlag);
   ioParam_preActivityIsNotRate(ioFlag);
   ioParam_writeStep(ioFlag);
   ioParam_initialWriteTime(ioFlag);
   ioParam_writeCompressedWeights(ioFlag);
   ioParam_writeCompressedCheckpoints(ioFlag);
   ioParam_selfFlag(ioFlag);
   ioParam_combine_dW_with_W_flag(ioFlag);
   ioParam_delay(ioFlag);
   ioParam_nxp(ioFlag);
   ioParam_nyp(ioFlag);
   ioParam_nxpShrunken(ioFlag);
   ioParam_nypShrunken(ioFlag);
   ioParam_nfp(ioFlag);
   ioParam_shrinkPatches(ioFlag);
   ioParam_updateGSynFromPostPerspective(ioFlag);
   ioParam_normalizeMethod(ioFlag);
   if (normalizer != NULL) {
      normalizer->ioParamsFillGroup(ioFlag);
   }
   ioParam_dWMax(ioFlag);
   ioParam_shmget_flag(ioFlag);
   ioParam_keepKernelsSynchronized(ioFlag);
   ioParam_useWindowPost(ioFlag);
#if defined(PV_USE_OPENCL) || defined(PV_USE_CUDA)
   ioParam_receiveGpu(ioFlag);
   ioParam_postGroupXSize(ioFlag);
   ioParam_postGroupYSize(ioFlag);
   ioParam_numXLocal(ioFlag);
   ioParam_numYLocal(ioFlag);
   ioParam_numFLocal(ioFlag);
#endif
   return PV_SUCCESS;
}

#if defined(PV_USE_OPENCL) || defined(PV_USE_CUDA)
void HyPerConn::ioParam_receiveGpu(enum ParamsIOFlag ioFlag) {
   parent->ioParamValue(ioFlag, name, "receiveGpu", &receiveGpu, false/*default*/, false/*warn if absent*/);
}

void HyPerConn::ioParam_postGroupXSize(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "receiveGpu"));
   assert(!parent->parameters()->presentAndNotBeenRead(name, "updateGSynFromPostPerspective"));
   if(receiveGpu && !updateGSynFromPostPerspective){
      parent->ioParamValue(ioFlag, name, "postGroupXSize", &postGroupXSize, 1, true);
   }
}

void HyPerConn::ioParam_postGroupYSize(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "receiveGpu"));
   assert(!parent->parameters()->presentAndNotBeenRead(name, "updateGSynFromPostPerspective"));
   if(receiveGpu && !updateGSynFromPostPerspective){
      parent->ioParamValue(ioFlag, name, "postGroupYSize", &postGroupYSize, 1, true);
   }
}

void HyPerConn::ioParam_numXLocal(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "receiveGpu"));
   if(receiveGpu){
      parent->ioParamValue(ioFlag, name, "numXLocal", &numXLocal, 1, true);
   }
}

void HyPerConn::ioParam_numYLocal(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "receiveGpu"));
   if(receiveGpu){
      parent->ioParamValue(ioFlag, name, "numYLocal", &numYLocal, 1, true);
   }
}

void HyPerConn::ioParam_numFLocal(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "receiveGpu"));
   assert(!parent->parameters()->presentAndNotBeenRead(name, "updateGSynFromPostPerspective"));
   if(receiveGpu && updateGSynFromPostPerspective){
      parent->ioParamValue(ioFlag, name, "numFLocal", &numFLocal, 1, true);
   }
}


#endif

void HyPerConn::ioParam_preLayerName(enum ParamsIOFlag ioFlag) {
   parent->ioParamString(ioFlag, name, "preLayerName", &preLayerName, NULL, false/*warnIfAbsent*/);
}

void HyPerConn::ioParam_postLayerName(enum ParamsIOFlag ioFlag) {
   parent->ioParamString(ioFlag, name, "postLayerName", &postLayerName, NULL, false/*warnIfAbsent*/);
}

void HyPerConn::ioParam_channelCode(enum ParamsIOFlag ioFlag) {
   if (ioFlag==PARAMS_IO_READ) {
      int ch = 0;
      parent->ioParamValueRequired(ioFlag, name, "channelCode", &ch);
      int status = decodeChannel(ch, &channel);
      if (status != PV_SUCCESS) {
         if (parent->columnId()==0) {
            fprintf(stderr, "%s \"%s\": channelCode %d is not a valid channel.\n",
                  parent->parameters()->groupKeywordFromName(name), name,  ch);
         }
#ifdef PV_USE_MPI
         MPI_Barrier(parent->icCommunicator()->communicator());
#endif
         exit(EXIT_FAILURE);
      }
   }
   else if (ioFlag==PARAMS_IO_WRITE) {
      int ch = (int) channel;
      parent->ioParamValueRequired(ioFlag, name, "channelCode", &ch);
   }
   else {
      assert(0); // All possibilities of ioFlag are covered above.
   }
}

void HyPerConn::ioParam_sharedWeights(enum ParamsIOFlag ioFlag) {
   parent->ioParamValue(ioFlag, name, "sharedWeights", &sharedWeights, false/*default*/, true/*warn if absent*/);
}

void HyPerConn::ioParam_initializeFromCheckpointFlag(enum ParamsIOFlag ioFlag) {
   assert(parent->getInitializeFromCheckpointDir()); // If we're not initializing any layers or connections from a checkpoint, this should be the empty string, not null.
   if (parent->getInitializeFromCheckpointDir() && parent->getInitializeFromCheckpointDir()[0]) {
      parent->ioParamValue(ioFlag, name, "initializeFromCheckpointFlag", &initializeFromCheckpointFlag, parent->getDefaultInitializeFromCheckpointFlag(), true/*warnIfAbsent*/);
   }
}

void HyPerConn::ioParam_weightInitType(enum ParamsIOFlag ioFlag) {
   parent->ioParamString(ioFlag, name, "weightInitType", &weightInitTypeString, NULL, true/*warnIfAbsent*/);
   if (ioFlag==PARAMS_IO_READ) {
      int status = setWeightInitializer();
      if (status != PV_SUCCESS) {
         fprintf(stderr, "%s \"%s\": Rank %d process unable to construct weightInitializer\n",
               parent->parameters()->groupKeywordFromName(name), name, parent->columnId());
         exit(EXIT_FAILURE);
      }
   }
}

void HyPerConn::ioParam_numAxonalArbors(enum ParamsIOFlag ioFlag) {
   parent->ioParamValue(ioFlag, name, "numAxonalArbors", &numAxonalArborLists, 1);
   if (ioFlag == PARAMS_IO_READ && numAxonalArborLists==0 && parent->columnId()==0) {
      fprintf(stdout, "HyPerConn:: Warning: Connection %s: Variable numAxonalArbors is set to 0. No connections will be made.\n",name);
   }
}

void HyPerConn::ioParam_plasticityFlag(enum ParamsIOFlag ioFlag) {
   parent->ioParamValue(ioFlag, name, "plasticityFlag", &plasticityFlag, true/*default value*/);
}

void HyPerConn::ioParam_weightUpdatePeriod(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "plasticityFlag"));
   if (plasticityFlag) {
      parent->ioParamValue(ioFlag, name, "weightUpdatePeriod", &weightUpdatePeriod, parent->getDeltaTime());
   }
}

void HyPerConn::ioParam_initialWeightUpdateTime(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "plasticityFlag"));
   initialWeightUpdateTime = parent->getStartTime();
   if (plasticityFlag) {
      parent->ioParamValue(ioFlag, name, "initialWeightUpdateTime", &initialWeightUpdateTime, initialWeightUpdateTime, true/*warnIfAbsent*/);
   }
   if (ioFlag==PARAMS_IO_READ) {
      weightUpdateTime=initialWeightUpdateTime;
   }
}

void HyPerConn::ioParam_triggerFlag(enum ParamsIOFlag ioFlag){
   assert(!parent->parameters()->presentAndNotBeenRead(name, "plasticityFlag"));
   if(plasticityFlag){
      parent->ioParamValue(ioFlag, name, "triggerFlag", &triggerFlag, triggerFlag);
   }
   else {
      triggerFlag = false;
      parent->parameters()->handleUnnecessaryParameter(name, "triggerFlag", triggerFlag);
   }
}

void HyPerConn::ioParam_triggerLayerName(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "triggerFlag"));
   if (triggerFlag) {
      parent->ioParamStringRequired(ioFlag, name, "triggerLayerName", &triggerLayerName);
   }
}

void HyPerConn::ioParam_triggerOffset(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "triggerFlag"));
   if (triggerFlag) {
      parent->ioParamValue(ioFlag, name, "triggerOffset", &triggerOffset, triggerOffset);
      if(triggerOffset < 0){
         fprintf(stderr, "%s \"%s\" error in rank %d process: TriggerOffset (%f) must be positive\n", parent->parameters()->groupKeywordFromName(name), name, parent->columnId(), triggerOffset);
         exit(EXIT_FAILURE);
      }
   }
}

void HyPerConn::ioParam_pvpatchAccumulateType(enum ParamsIOFlag ioFlag) {
   PVParams * params = parent->parameters();
   // stochasticReleaseFlag deprecated on Aug 22, 2013.
   if (ioFlag==PARAMS_IO_READ && params->present(name, "stochasticReleaseFlag")) {
      bool stochasticReleaseFlag = params->value(name, "stochasticReleaseFlag")!=0.0;
      const char * pvpatch_accumulate_string = stochasticReleaseFlag ? "stochastic" : "convolve";
      if (parent->columnId()==0) {
         fprintf(stderr, "%s \"%s\" warning: parameter stochasticReleaseFlag is deprecated.  Instead, set pvpatchAccumulateType to one of \"convolve\" (the default), \"stochastic\", or \"maxpooling\".\n", parent->parameters()->groupKeywordFromName(name), name);
         fprintf(stderr, "    pvpatchAccumulateType set to \"%s\" \n", pvpatch_accumulate_string);
      }
      pvpatchAccumulateTypeString = strdup(pvpatch_accumulate_string);
      if (pvpatchAccumulateTypeString==NULL) {
         fprintf(stderr, "%s \"%s\": rank %d process unable to set pvpatchAccumulateType string: %s.\n",
               params->groupKeywordFromName(name), name, parent->columnId(), strerror(errno));
         exit(EXIT_FAILURE);
      }
      pvpatchAccumulateType = stochasticReleaseFlag ? ACCUMULATE_STOCHASTIC : ACCUMULATE_CONVOLVE;
      return;
   }
   parent->ioParamString(ioFlag, name, "pvpatchAccumulateType", &pvpatchAccumulateTypeString, "convolve");
   if (ioFlag==PARAMS_IO_READ) {
      // Convert string to lowercase so that capitalization doesn't matter.
      for (char * c = pvpatchAccumulateTypeString; *c!='\0'; c++) {
         *c = (char) tolower((int) *c);
      }

      if (strcmp(pvpatchAccumulateTypeString,"convolve")==0) {
         pvpatchAccumulateType = ACCUMULATE_CONVOLVE;
      }
      else if (strcmp(pvpatchAccumulateTypeString,"stochastic")==0) {
         pvpatchAccumulateType = ACCUMULATE_STOCHASTIC;
      }
      else if (strcmp(pvpatchAccumulateTypeString,"maxpooling")==0 ||
            strcmp(pvpatchAccumulateTypeString,"max pooling")==0) {
         pvpatchAccumulateType = ACCUMULATE_MAXPOOLING;
      }
      else {
         if (parent->columnId()==0) {
            fprintf(stderr, "%s \"%s\" error: pvpatchAccumulateType \"%s\" unrecognized.  Allowed values are \"convolve\", \"stochastic\", or \"maxpooling\"\n",
                  parent->parameters()->groupKeywordFromName(name), name, pvpatchAccumulateTypeString);
         }
#ifdef PV_USE_MPI
         MPI_Barrier(parent->icCommunicator()->communicator());
#endif
         exit(EXIT_FAILURE);
      }
   }
}

void HyPerConn::ioParam_preActivityIsNotRate(enum ParamsIOFlag ioFlag) {
   parent->ioParamValue(ioFlag, name, "preActivityIsNotRate", &preActivityIsNotRate, false/*default value*/, true/*warn if absent*/);
}

void HyPerConn::ioParam_writeStep(enum ParamsIOFlag ioFlag) {
   parent->ioParamValue(ioFlag, name, "writeStep", &writeStep, parent->getDeltaTime());
}

void HyPerConn::ioParam_initialWriteTime(enum ParamsIOFlag ioFlag) {
   PVParams * params = parent->parameters();
   assert(!parent->parameters()->presentAndNotBeenRead(name, "writeStep"));
   if (writeStep>=0) {
      double start_time = parent->getStartTime();
      parent->ioParamValue(ioFlag, name, "initialWriteTime", &initialWriteTime, start_time);
      if (ioFlag == PARAMS_IO_READ) {
         if (writeStep>0 && initialWriteTime < start_time) {
            if (parent->columnId()==0) {
               printf("%s \"%s\": initialWriteTime %f earlier than starting time %f.  Adjusting initialWriteTime:\n",
                     parent->parameters()->groupKeywordFromName(name), name, initialWriteTime, start_time);
               fflush(stdout);
            }
            while (initialWriteTime < start_time) {
               initialWriteTime += writeStep;
            }
            if (parent->columnId()==0) {
               printf("%s \"%s\": initialWriteTime adjusted to %f\n",
                     parent->parameters()->groupKeywordFromName(name), name, initialWriteTime);
            }
         }
         writeTime = initialWriteTime;
      }
   }
}

void HyPerConn::ioParam_writeCompressedWeights(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "writeStep"));
   if (writeStep>=0) {
      parent->ioParamValue(ioFlag, name, "writeCompressedWeights", &writeCompressedWeights, writeCompressedWeights, /*warnifabsent*/true);
   }
}

void HyPerConn::ioParam_writeCompressedCheckpoints(enum ParamsIOFlag ioFlag) {
   if (parent->getCheckpointWriteFlag() || !parent->getSuppresLastOutputFlag()) {
      parent->ioParamValue(ioFlag, name, "writeCompressedCheckpoints", &writeCompressedCheckpoints, writeCompressedCheckpoints, /*warnifabsent*/true);
   }
}

void HyPerConn::ioParam_selfFlag(enum ParamsIOFlag ioFlag) {
   // selfFlag indicates whether pre and post layers refer to the same neurons.
   // The default value for selfFlag should be pre==post, but at the time ioParams(PARAMS_IO_READ) is called,
   // pre and post have not been set.  So we read the value with no warning if it's present;
   // if it's absent, set the value to pre==post in the communicateInitInfo stage and issue
   // the using-default-value warning then.
   parent->ioParamValue(ioFlag, name, "selfFlag", &selfFlag, selfFlag, false/*warnIfAbsent*/);
}

void HyPerConn::ioParam_combine_dW_with_W_flag(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "plasticityFlag"));
   if (plasticityFlag){
      parent->ioParamValue(ioFlag, name, "combine_dW_with_W_flag", &combine_dW_with_W_flag, combine_dW_with_W_flag, true/*warnIfAbsent*/);
   }

}

void HyPerConn::ioParam_delay(enum ParamsIOFlag ioFlag) {
   //Grab delays in ms and load into fDelayArray.
   //initializeDelays() will convert the delays to timesteps store into delays.
   parent->ioParamArray(ioFlag, name, "delay", &fDelayArray, &delayArraySize);
   if (ioFlag==PARAMS_IO_READ && delayArraySize==0) {
      assert(fDelayArray==NULL);
      fDelayArray = (float *) malloc(sizeof(float));
      if (fDelayArray == NULL) {
         fprintf(stderr, "%s \"%s\" error setting default delay: %s\n",
               parent->parameters()->groupKeywordFromName(name), name, strerror(errno));
         exit(EXIT_FAILURE);
      }
      *fDelayArray = 0.0f; // Default delay
      delayArraySize = 1;
      if (parent->columnId()==0) {
         printf("%s \"%s\": Using default value of zero for delay.\n",
               parent->parameters()->groupKeywordFromName(name), name);
      }
   }
}

void HyPerConn::ioParam_nxp(enum ParamsIOFlag ioFlag) {
   parent->ioParamValue(ioFlag, name, "nxp", &nxp, 1);
}

void HyPerConn::ioParam_nyp(enum ParamsIOFlag ioFlag) {
   parent->ioParamValue(ioFlag, name, "nyp", &nyp, 1);
}

void HyPerConn::ioParam_nxpShrunken(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "nxp"));
   parent->ioParamValue(ioFlag, name, "nxpShrunken", &nxpShrunken, nxp);
}

void HyPerConn::ioParam_nypShrunken(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "nyp"));
   parent->ioParamValue(ioFlag, name, "nypShrunken", &nypShrunken, nyp);
}

void HyPerConn::ioParam_nfp(enum ParamsIOFlag ioFlag) {
   parent->ioParamValue(ioFlag, name, "nfp", &nfp, -1, false);
   if (ioFlag==PARAMS_IO_READ && nfp==-1 && !parent->parameters()->present(name, "nfp") && parent->columnId()==0) {
      printf("%s \"%s\": nfp will be set in the communicateInitInfo() stage.\n",
            parent->parameters()->groupKeywordFromName(name), name);
   }
}

void HyPerConn::ioParam_shrinkPatches(enum ParamsIOFlag ioFlag) {
   parent->ioParamValue(ioFlag, name, "shrinkPatches", &shrinkPatches_flag, shrinkPatches_flag);
}

void HyPerConn::ioParam_shrinkPatchesThresh(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "shrinkPatches"));
   if (shrinkPatches_flag) {
      parent->ioParamValue(ioFlag, name, "shrinkPatchesThresh", &shrinkPatchesThresh, shrinkPatchesThresh);
   }
}

void HyPerConn::ioParam_updateGSynFromPostPerspective(enum ParamsIOFlag ioFlag) {
   parent->ioParamValue(ioFlag, name, "updateGSynFromPostPerspective", &updateGSynFromPostPerspective, updateGSynFromPostPerspective);
}

void HyPerConn::ioParam_dWMax(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "plasticityFlag"));
   if (plasticityFlag) {
      parent->ioParamValue(ioFlag, name, "dWMax", &dWMax, dWMax, true/*warnIfAbsent*/);
   }
}

void HyPerConn::ioParam_normalizeMethod(enum ParamsIOFlag ioFlag) {
   parent->ioParamStringRequired(ioFlag, name, "normalizeMethod", &normalizeMethod);
   PVParams * params = parent->parameters();
   if (ioFlag == PARAMS_IO_READ) {
#ifdef OBSOLETE // Marked obsolete July 17, 2014.  normalizeMethod is now a required parameter
      if (!normalizeMethod) {
         const char * normalize_method = NULL;
         if (params->present(name, "normalize")) {
            if (parent->columnId()==0) {
               fprintf(stderr, "%s \"%s\" warning: normalize_flag is deprecated.\n",
                     parent->parameters()->groupKeywordFromName(name), name);
               fprintf(stderr, "Please use the string parameter normalizeMethod.\n");
               fprintf(stderr, "'normalize = false;' should be replaced by 'normalizeMethod = \"none\"';\n");
               fprintf(stderr, "and 'normalize = true;' should be replaced by setting normalizeMethod to one of\n");
               fprintf(stderr, "\"normalizeSum\", \"normalizeL2\", \"normalizeScale\" ,\"normalizeMax\", or \"normalizeContrastZeroMean\".\n");
            }
         }
         bool normalize_flag = params->value(name, "normalize", true/*default*/);
         if (normalize_flag) {
            if (params->value(name, "normalize_max", false/*default*/) != 0.0f) {
               normalize_method = "normalizeMax";
            }
            if (params->value(name, "nomalize_RMS_amp", false/*default*/) != 0.0f) {
               normalize_method = "normalizeL2";
            }
            else {
               normalize_method = "normalizeSum";
            }
         }
         else {
            normalize_method = "";
         }
         normalizeMethod = strdup(normalize_method);
      }
#endif // OBSOLETE
      assert(normalizeMethod);
      if (normalizeMethod[0]!='\0') {
         if (!strcmp(normalizeMethod, "normalizeSum")) {
            normalizer = new NormalizeSum(this);
         }
         else if (!strcmp(normalizeMethod, "normalizeL2"))  {
            normalizer = new NormalizeL2(this);
         }
         else if (!strcmp(normalizeMethod, "normalizeMax")) {
            normalizer = new NormalizeMax(this);
         }
         else if (!strcmp(normalizeMethod, "normalizeContrastZeroMean")) {
            normalizer = new NormalizeContrastZeroMean(this);
         }
         else if (!strcmp(normalizeMethod, "normalizeScale")) {
            if (plasticityFlag) {
                fprintf(stdout, "HyPerConn:: Warning: Connection %s: Setting both plastic weights and normalization by scaling. The weights will be multiplied by a factor strength after each learning step. Generally not a good idea. Make sure you know what you are doing!\n",name);
            }
            normalizer = new NormalizeScale(this);
         }
         else if (!strcmp(normalizeMethod, "none")) {
            normalizer = NULL;
         }
         else {
            if (parent->columnId()==0) {
               fprintf(stderr, "%s \"%s\": unrecognized normalizeMethod \"%s\".\n",
                     parent->parameters()->groupKeywordFromName(name), name, normalizeMethod);
               exit(EXIT_FAILURE);
            }
         }
      }
      else {
         free(normalizeMethod);
         normalizeMethod = strdup("none");
         if (parent->columnId()==0) {
            printf("%s \"%s\": empty normalizeMethod string will be set to \"none\"\n", parent->parameters()->groupKeywordFromName(name), name);
         }
         normalizer = NULL;
      }
   }
}

void HyPerConn::ioParam_strength(enum ParamsIOFlag ioFlag, float * strength, bool warnIfAbsent) {
   // Not called by HyPerConn directly, but as both the normalizer and
   // weightInitializer hierarchies use the strength parameter,
   // it is put here so that both can use the same function.
   // This also means that we can make sure that outputParams only
   // writes the strength parameter once, even if it's used in two
   // different contexts.
   if (ioFlag != PARAMS_IO_WRITE || !strengthParamHasBeenWritten) {
      parent->ioParamValue(ioFlag, name, "strength", strength, *strength, warnIfAbsent);
   }
   if (ioFlag == PARAMS_IO_WRITE) {
      strengthParamHasBeenWritten = true;
   }
}

void HyPerConn::ioParam_shmget_flag(enum ParamsIOFlag ioFlag) {
#ifdef USE_SHMGET
   assert(!parent->parameters()->presentAndNotBeenRead(name, "sharedWeights"));
   if (!sharedWeights) return;
   assert(!parent->parameters()->presentAndNotBeenRead(name, "plasticityFlag"));
   parent->ioParamValue(ioFlag, name, "shmget_flag", &shmget_flag, shmget_flag, true/*warnIfAbsent*/);
   if (plasticityFlag && shmget_flag) {
       shmget_flag = false;
       if (parent->columnId()==0) {
          std::cout << "in HyPerConn::initialize: " << this->name
                    << ", shmget_flag parameter specified as true, reset to false because plasticity_flag is true"
                    << std::endl;
       }
   }
#else
   if (ioFlag == PARAMS_IO_READ) {
      // mark as read so that shmget_flag doesn't get an unread-parameter warning.
      // This way the same params file can be used with USE_SHMGET on or off.
      parent->parameters()->value(name, "shmget_flag", false, false);
   }
#endif // USE_SHMGET
}

void HyPerConn::ioParam_keepKernelsSynchronized(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "sharedWeights"));
   assert(!parent->parameters()->presentAndNotBeenRead(name, "plasticityFlag"));
   if (sharedWeights && plasticityFlag) {
      parent->ioParamValue(ioFlag, name, "keepKernelsSynchronized", &keepKernelsSynchronized_flag, sharedWeights, true/*warnIfAbsent*/);
   }
}

void HyPerConn::ioParam_useWindowPost(enum ParamsIOFlag ioFlag) {
   assert(!parent->parameters()->presentAndNotBeenRead(name, "sharedWeights"));
   assert(!parent->parameters()->presentAndNotBeenRead(name, "numAxonalArbors"));
   assert(!parent->parameters()->presentAndNotBeenRead(name, "plasticityFlag"));
   if (sharedWeights && plasticityFlag && numAxonalArborLists>1) {
      initialWeightUpdateTime = 1.0;
      parent->ioParamValue(ioFlag, name, "useWindowPost", &useWindowPost, useWindowPost);
   }
}

int HyPerConn::decodeChannel(int channel_code, ChannelType * channel_type) {
   int status = PV_SUCCESS;
   switch( channel_code ) {
   case CHANNEL_EXC:
      *channel_type = CHANNEL_EXC;
      break;
   case CHANNEL_INH:
      *channel_type = CHANNEL_INH;
      break;
   case CHANNEL_INHB:
      *channel_type = CHANNEL_INHB;
      break;
   case CHANNEL_GAP:
      *channel_type = CHANNEL_GAP;
      break;
   case CHANNEL_NORM:
      *channel_type = CHANNEL_NORM;
      break;
   case CHANNEL_NOUPDATE:
      *channel_type = CHANNEL_NOUPDATE;
      break;
   default:
      status = PV_FAILURE;
      break;
   }
   return status;
}

int HyPerConn::initializeDelays(const float * fDelayArray, int size){

   int status = PV_SUCCESS;
   assert(!parent->parameters()->presentAndNotBeenRead(name, "numAxonalArbors"));
   //Allocate delay data structure
   delays = (int *) calloc(numAxonalArborLists, sizeof(int));
   if( delays == NULL ) {
      createArborsOutOfMemory();
      assert(false);
   }

   //Initialize delays for each arbor
   //Using setDelay to convert ms to timesteps
   for (int arborId=0;arborId<numAxonalArborLists;arborId++) {
      if (size == 0){
         //No delay
         setDelay(arborId, 0);
      }
      else if (size == 1){
         setDelay(arborId, fDelayArray[0]);
      }
      else if (size == numAxonalArborLists){
         setDelay(arborId, fDelayArray[arborId]);
      }
      else{
         fprintf(stderr, "Delay must be either a single value or the same length as the number of arbors\n");
         abort();
      }
   }
   return status;
}

#ifdef OBSOLETE // Marked obsolete Mar 19, 2014
int HyPerConn::readPatchSizeFromFile(const char * filename) {
   assert(filename != NULL);
   int status = PV_SUCCESS;
   readUseListOfArborFiles(parent->parameters());
   readCombineWeightFiles(parent->parameters());
   if( !useListOfArborFiles && !combineWeightFiles) { // Should still get patch size from file if either of these flags is true
      status = patchSizeFromFile(filename);
   }
   // else {
   //    status = readPatchSizeFromParams(parent->parameters());
   // }
   return status;
}

void HyPerConn::readUseListOfArborFiles(PVParams * params) {
   assert(filename!=NULL);
   useListOfArborFiles = params->value(name, "useListOfArborFiles", false)!=0;
}

void HyPerConn::readCombineWeightFiles(PVParams * params) {
   assert(filename!=NULL);
   combineWeightFiles = params->value(name, "combineWeightFiles", false)!=0;
}
#endif // OBSOLETE

int HyPerConn::communicateInitInfo() {
   // HyPerConns need to tell the parent HyPerCol how many random number
   // seeds they need.  At the start of HyPerCol::run, the parent HyPerCol
   // calls each layer's and each connection's communicateInitInfo() sequentially in
   // a repeatable order (probably the order they appear in the params
   // file) to make sure that the same runs use the same RNG seeds in the
   // same way.
   //
   // HyPerConns need RNGs if they are using stochastic release flag, or if
   // their InitWeights method is random (e.g. UniformRandomWeights or
   // GaussianRandomWeights).
   //
   // HyPerConn also tells:
   // - its pre-synaptic layer how big a margin is needed
   // - its pre-synaptic layer how long a delay is needed in the data store
   // - its post-synaptic layer which channel it will deliver GSyn to.
   //
   // The routine also checks that nxp and nyp are consistent with
   // the relative densities of the pre and post layers, and that nfp is
   // consistent with the number of features of post.
   //
   // Subclasses (e.g. CloneKernelConn) may also need
   // to send messages to related layers and connections before the allocation
   // phase.  These subclasses should override communicateInitInfo(), and the
   // subclass's communicateInitInfo() should call the parent class's communicateInitInfo().

   int status = PV_SUCCESS;

   if (preLayerName==NULL) {
      assert(postLayerName==NULL);
      status = handleMissingPreAndPostLayerNames();
   }
#ifdef PV_USE_MPI
   MPI_Barrier(parent->icCommunicator()->communicator());
#endif
   if (status != PV_SUCCESS) {
      assert(preLayerName==NULL && postLayerName==NULL);
      if (parent->columnId()==0) {
         fprintf(stderr, "%s \"%s\" error: Unable to determine pre- and post-layer names.  Exiting.\n", parent->parameters()->groupKeywordFromName(name), name);
      }
      exit(EXIT_FAILURE);
   }
   this->pre = parent->getLayerFromName(preLayerName);
   this->post = parent->getLayerFromName(postLayerName);
   if (this->pre==NULL) {
      if (parent->columnId()==0) {
         fprintf(stderr, "Connection \"%s\": preLayerName \"%s\" does not correspond to a layer in the column.\n", name, preLayerName);
      }
      status = PV_FAILURE;
   }

   if (this->post==NULL) {
      if (parent->columnId()==0) {
         fprintf(stderr, "Connection \"%s\": postLayerName \"%s\" does not correspond to a layer in the column.\n", name, postLayerName);
      }
      status = PV_FAILURE;
   }
#ifdef PV_USE_MPI
   MPI_Barrier(parent->icCommunicator()->communicator());
#endif
   if (status != PV_SUCCESS) {
      exit(EXIT_FAILURE);
   }

   handleDefaultSelfFlag();

   // Find maximum delay over all the arbors and send it to the presynaptic layer
   int maxdelay = 0;
   for (int delayi = 0; delayi < delayArraySize; delayi++){
      if (fDelayArray[delayi] > maxdelay){
         maxdelay = fDelayArray[delayi];
      }
   }
   //for( int arborId=0; arborId<numberOfAxonalArborLists(); arborId++ ) {
   //   int curdelay = this->getDelay(arborId);
   //   if( maxdelay < curdelay ) maxdelay = curdelay;
   //}
   int allowedDelay = pre->increaseDelayLevels(maxdelay);
   if( allowedDelay < maxdelay ) {
      if( parent->icCommunicator()->commRank() == 0 ) {
         fflush(stdout);
         fprintf(stderr, "Connection \"%s\": attempt to set delay to %d, but the maximum allowed delay is %d.  Exiting\n", name, maxdelay, allowedDelay);
      }
      exit(EXIT_FAILURE);
   }

   // Make sure post-synaptic layer has enough channels.
   int num_channels_check;
   status = post->requireChannel((int) channel, &num_channels_check);
   if (status != PV_SUCCESS) { return status; }

   if(num_channels_check <= (int) channel) {
      if (parent->columnId()==0) {
         fprintf(stderr, "%s \"%s\" error: postsynaptic layer \"%s\" failed to add channel %d\n",
               parent->parameters()->groupKeywordFromName(name), name, post->getName(), (int) channel);
      }
#ifdef PV_USE_MPI
      MPI_Barrier(parent->icCommunicator()->communicator());
#endif
      exit(EXIT_FAILURE);
   }

   status = setPatchSize();
   status = checkPatchDimensions();

   if (nfp == -1) {
      nfp = post->getCLayer()->loc.nf;
      if (warnDefaultNfp && parent->columnId()==0) {
         printf("Connection \"%s\" setting nfp to number of postsynaptic features = %d.\n", name, nfp);
      }
   }
   if (nfp != post->getCLayer()->loc.nf) {
      if (parent->columnId()==0) {
         fprintf( stderr, "Params file specifies %d features for connection \"%s\",\n", nfp, name );
         fprintf( stderr, "but %d features for post-synaptic layer %s\n",
               post->getCLayer()->loc.nf, post->getName() );
      }
#ifdef PV_USE_MPI
      MPI_Barrier(parent->icCommunicator()->communicator());
#endif
      exit(PV_FAILURE);
   }
   // Currently, the only acceptable number for nfp is the number of post-synaptic features.
   // However, we may add flexibility on this score in the future, e.g. MPI in feature space
   // with each feature connecting to only a few nearby features.
   // Accordingly, we still keep readNfp.

   int xmargin = computeMargin(pre->getXScale(), post->getXScale(), nxp);
   int ymargin = computeMargin(pre->getYScale(), post->getYScale(), nyp);
   int receivedxmargin = 0;
   int statusx = pre->requireMarginWidth(xmargin, &receivedxmargin, 'x');
   int receivedymargin = 0;
   int statusy = pre->requireMarginWidth(ymargin, &receivedymargin, 'y');
   if (statusx != PV_SUCCESS) {
      fprintf(stderr,"Margin Failure for layer %s.  Received x-margin is %d, but connection \"%s\" requires margin of at least %d\n", pre->getName(),receivedxmargin, name, xmargin);
      status = PV_MARGINWIDTH_FAILURE;
   }
   if (statusy != PV_SUCCESS) {
      fprintf(stderr,"Margin Failure for layer %s.  Received y-margin is %d, but connection \"%s\" requires margin of at least %d\n", pre->getName(),receivedymargin, name, ymargin);
      status = PV_MARGINWIDTH_FAILURE;
   }

   //Trigger stuff
   if(triggerFlag){
      triggerLayer = parent->getLayerFromName(triggerLayerName);
      if (triggerLayer==NULL) {
         if (parent->columnId()==0) {
            fprintf(stderr, "%s \"%s\" error: triggerLayer \"%s\" is not a layer in the HyPerCol.\n",
                    parent->parameters()->groupKeywordFromName(name), name, triggerLayerName);
         }
#ifdef PV_USE_MPI
         MPI_Barrier(parent->icCommunicator()->communicator());
#endif
         exit(EXIT_FAILURE);
      }
      //Use either triggering or weightUpdatePeriod
      //If not default values, print warning
      if((weightUpdatePeriod != 1 || weightUpdateTime != 0) && parent->columnId()==0 ){
         std::cout << "Warning: Connection " << name << " trigger flag is set, ignoring weightUpdatePeriod and initialWeightUpdateTime\n";
      }
      //Although weightUpdatePeriod and weightUpdateTime is being set here, if trigger flag is set, they are not being used
      //Only updating for backwards compatibility
      //getDeltaUpdateTime can return -1 (if it never updates), so set plasticity flag off if so
      weightUpdatePeriod = triggerLayer->getDeltaUpdateTime();
      if(weightUpdatePeriod <= 0){
         if(plasticityFlag == true){
            std::cout << "Warning: Connection " << name << "triggered layer " << triggerLayerName << " never updates, turning placisity flag off\n";
            plasticityFlag = false;
         }
      }
      if(weightUpdatePeriod != -1 && triggerOffset >= weightUpdatePeriod){
         fprintf(stderr, "%s \"%s\" error in rank %d process: TriggerOffset (%f) must be lower than the change in update time (%f) of the attached trigger layer\n", parent->parameters()->groupKeywordFromName(name), name, parent->columnId(), triggerOffset, weightUpdatePeriod);
         exit(EXIT_FAILURE);
      }
      weightUpdateTime = 1;
   }

   if (weightInitializer) weightInitializer->communicateParamsInfo();

   if (sharedWeights) {
      if (pre->getNumWindows() != 1 && pre->getNumWindows() != this->numberOfAxonalArborLists()){
         fprintf(stderr, "HyPerConn::Number of windows in %s is %d (calculated from symmetry), while number of arbors in %s is %d. Either some windows or arbors will not be used\n", pre->getName(), pre->getNumWindows(), name, this->numberOfAxonalArborLists());
      }
      fileType = PVP_KERNEL_FILE_TYPE;
   }
   else {
      fileType = PVP_WGT_FILE_TYPE;
   }

//GPU stuff
#if defined(PV_USE_OPENCL) || defined(PV_USE_CUDA)
   //Here, the connection tells all participating recev layers to allocate memory on gpu
   //if receive from gpu is set. These buffers should be set in allocate
   if(receiveGpu){
      //we need pre activity, this conn's weights, and post gsyn on the channel of this connection
      pre->setAllocDeviceActivity();
      this->setAllocDeviceWeights();
      post->setAllocDeviceGSyn(getChannel());
   }
#endif

   return status;
}

void HyPerConn::handleDefaultSelfFlag() {
   if (!parent->parameters()->present(name, "selfFlag")) {
      selfFlag = (pre == post);
   }
   else {
      // parameter was specified in params; use that value.
   }
}

int HyPerConn::setPatchSize() {
   int status = PV_SUCCESS;
   // Some subclasses determine some of {nxp, nyp, nfp} from other layers or connections (e.g. TransposeConn, CloneKernelConn)
   // instead of reading them from params.  They should override setPatchSize() to set those params.
   return status;
}

// returns handle to initialized weight patches
PVPatch *** HyPerConn::initializeWeights(PVPatch *** patches, pvwdata_t ** dataStart)
{
   PVPatch *** patches_arg = sharedWeights ? NULL : patches;
   weightInitializer->initializeWeights(patches_arg, dataStart);
#ifdef USE_SHMGET
#ifdef PV_USE_MPI
   // insert synchronization barrier to ensure that all processes have finished loading portions of shared memory for which they
   // might be responsible
   //std::cout << "starting MPI_Barrier in HyPerConn::initializeWeights: " << this->name << ", rank = " << getParent()->icCommunicator()->commRank() << std::endl;
#ifdef PV_USE_MPI
   MPI_Barrier(getParent()->icCommunicator()->communicator());
#endif
   //std::cout << "leaving MPI_Barrier in HyPerConn::initializeWeights: " << this->name << ", rank = " << getParent()->icCommunicator()->commRank() << std::endl;
#endif // PV_USE_MPI
#endif // USE_SHMGET
   normalizeWeights();
#ifdef PV_USE_OPENCL
// Copied over from KernelConn.
//   //don't support GPU acceleration in kernelconn yet
//   ignoreGPUflag=false;
//   //tell the recieving layer to copy gsyn to the gpu, because kernelconn won't be calculating it
//   post->copyChannelToDevice();
#endif
   return patches;
}

int HyPerConn::allocateDataStructures() {
   initNumWeightPatches();
   initNumDataPatches();
   initPatchToDataLUT();
   initializeDelays(fDelayArray, delayArraySize);

   if (pvpatchAccumulateType == ACCUMULATE_STOCHASTIC) {
      bool from_post = getUpdateGSynFromPostPerspective();
      if (from_post) {
         randState = new Random(parent, postSynapticLayer()->getLayerLoc(), false/*isExtended*/);
      }
      else {
         randState = new Random(parent, preSynapticLayer()->getLayerLoc(), true/*isExtended*/);
      }
   }

   if (plasticityFlag) {
      if (parent->getCheckpointReadFlag()==false && weightUpdateTime < parent->simulationTime()) {
         while(weightUpdateTime <= parent->simulationTime()) {weightUpdateTime += weightUpdatePeriod;}
         if (parent->columnId()==0) {
            fprintf(stderr, "Warning: initialWeightUpdateTime of %s \"%s\" less than simulation start time.  Adjusting weightUpdateTime to %f\n",
                  parent->parameters()->groupKeywordFromName(name), name, weightUpdateTime);
         }
      }
      lastUpdateTime = weightUpdateTime - parent->getDeltaTime();
   }

   int status = constructWeights();

   if (sharedWeights) {
#ifdef PV_USE_MPI
      if (plasticityFlag) {
         const int numPatches = getNumDataPatches();
         const size_t patchSize = nxp*nyp*nfp;
         const size_t localSize = numPatches * patchSize;
         mpiReductionBuffer = (pvwdata_t *) malloc(localSize*sizeof(pvwdata_t));
         if(mpiReductionBuffer == NULL) {
            fprintf(stderr, "Connection \"%s\" unable to allocate memory for mpiReductionBuffer in rank %d process: %s\n", getName(), getParent()->columnId(), strerror(errno));
            exit(PV_FAILURE);
         }
      }
#endif // PV_USE_MPI
      numKernelActivations = (int *) malloc(getNumDataPatches() * sizeof(int));
      if(numKernelActivations == NULL) {
         fprintf(stderr, "Connection \"%s\" unable to allocate memory for numKernelActivations in rank %d process: %s\n", getName(), getParent()->columnId(), strerror(errno));
         exit(PV_FAILURE);
      }
      for (int ki = 0; ki < getNumDataPatches(); ki++) {
         numKernelActivations[ki] = 0;
      }
   }
   // do allocation stage for probes
   for (int i=0; i<numProbes; i++) {
      BaseConnectionProbe * p = probes[i];
      if (p==NULL) continue;
      int pstatus = p->allocateDataStructures();
      if (pstatus==PV_SUCCESS) {
         if (parent->columnId()==0) printf("Probe \"%s\" allocateDataStructures completed.\n", p->getName());
      }
      else {
         assert(pstatus == PV_FAILURE); // PV_POSTPONE etc. hasn't been implemented for probes yet.
         exit(EXIT_FAILURE); // Any error message should be printed by probe's communicateInitInfo function
      }
   }

   //Allocate a post to pre activity buffer needed for gpu and receive from post
   //Note that this has to be a transpose conn to do this, TODO take out this restriction
   //Cast to transpose conn
   TransposeConn * sourceToTargetConn = dynamic_cast <TransposeConn*> (this);
   //Can't do this with shrink patches flag
   if(sourceToTargetConn && !shrinkPatches_flag){
      //update conn to original connection
      HyPerConn * targetToSourceConn = sourceToTargetConn->getOriginalConn();
      const PVLayerLoc * oSourceLoc = targetToSourceConn->postSynapticLayer()->getLayerLoc();
      const PVLayerLoc * oTargetLoc = targetToSourceConn->preSynapticLayer()->getLayerLoc();
      const PVLayerLoc * aSourceLoc = preSynapticLayer()->getLayerLoc();
      const PVLayerLoc * aTargetLoc = postSynapticLayer()->getLayerLoc();

      const int sourceNx = aSourceLoc->nx;
      const int sourceNy = aSourceLoc->ny;
      const int sourceNf = aSourceLoc->nf;
      const int targetNx = aTargetLoc->nx;
      const int targetNy = aTargetLoc->ny;
      const int targetNf = aTargetLoc->nf;

      const PVHalo * aSourceHalo = &aSourceLoc->halo;
      const PVHalo * oSourceHalo = &oSourceLoc->halo;
      const PVHalo * aTargetHalo = &aTargetLoc->halo;
      const PVHalo * oTargetHalo = &oTargetLoc->halo;

      const int numRestricted = postSynapticLayer()->getNumNeurons();

      postToPreActivity = (long*)malloc(sizeof(long) * numRestricted);
      for (int kTargetRes = 0; kTargetRes < numRestricted; kTargetRes++){
         int okTargetExt = kIndexExtended(kTargetRes, targetNx, targetNy, targetNf, oTargetHalo->lt, oTargetHalo->rt, oTargetHalo->dn, oTargetHalo->up);
         int akTargetExt = kIndexExtended(kTargetRes, targetNx, targetNy, targetNf, aTargetHalo->lt, aTargetHalo->rt, aTargetHalo->dn, aTargetHalo->up);
         //Get start index of source from gsyn in restricted
         // We have to use gSynPatchStart instead of aPostOffset because originalConn's post-synaptic layer's nb may not be the same as conn's pre-layer's nb.
         int sourceRes = targetToSourceConn->getGSynPatchStart(okTargetExt, 0);
         int sourceExt= kIndexExtended(sourceRes, sourceNx, sourceNy, sourceNf, aSourceHalo->lt, aSourceHalo->rt, aSourceHalo->dn, aSourceHalo->up);
         int sourceXExt = kxPos(sourceExt, sourceNx + aSourceHalo->lt + aSourceHalo->rt, sourceNy + aSourceHalo->dn + aSourceHalo->up, sourceNf);
         int sourceYExt = kyPos(sourceExt, sourceNx + aSourceHalo->lt + aSourceHalo->rt, sourceNy + aSourceHalo->dn + aSourceHalo->up, sourceNf);
         int sourceF = featureIndex(sourceExt, sourceNx + aSourceHalo->lt + aSourceHalo->rt, sourceNy + aSourceHalo->dn + aSourceHalo->up, sourceNf);

         //Grab patch given the post
         //We grab this value from host memory since all we're taking from it is the offset
         //Note that we're grabbing only arbor 0, since without the shrink patches flag, all arbors must have the same geometry
         PVPatch * shrunkenWeights = targetToSourceConn->getWeights(okTargetExt, 0);
         //Grab offset
         int offset = shrunkenWeights->offset;
         //Get x and y in patch space
         //conn is target to source
         int patchX = kxPos(offset, targetToSourceConn->xPatchSize(), targetToSourceConn->yPatchSize(), targetToSourceConn->fPatchSize());
         int patchY = kyPos(offset, targetToSourceConn->xPatchSize(), targetToSourceConn->yPatchSize(), targetToSourceConn->fPatchSize());

         //Move source X and Y to offset
         sourceXExt -= patchX; 
         sourceYExt -= patchY; 

         //Change sourceExt back to extended source index, but unshrunken
         //Store this value in a buffer to avoid recalculation
         postToPreActivity[kTargetRes] = kIndex(sourceXExt, sourceYExt, sourceF, sourceNx + aSourceHalo->lt + aSourceHalo->rt, sourceNy + aSourceHalo->dn + aSourceHalo->up, sourceNf);
      }
   }

#if defined(PV_USE_OPENCL) || defined(PV_USE_CUDA)
   status = allocateDeviceBuffers();
   if(receiveGpu){
      if(updateGSynFromPostPerspective){
         status |= allocateReceivePostKernel();
      }
      else{
         status |= allocateReceivePreKernel();
      }
   }
   if(status == 0){
      status = PV_SUCCESS;
   }
   else{
      fprintf(stderr, "Connection \"%s\" unable to allocate device memory in rank %d process: %s\n", getName(), getParent()->columnId(), strerror(errno));
      exit(PV_FAILURE);
   }
#endif

   return status;
}

void HyPerConn::initPatchToDataLUT() {
   assert(patch2datalookuptable==NULL);
   if (sharedWeights) {
      int numWeightPatches=getNumWeightPatches();

      patch2datalookuptable=(int *) calloc(numWeightPatches, sizeof(int));
      for(int i=0; i<numWeightPatches; i++) {
         int kernelindex=patchIndexToDataIndex(i);
         patch2datalookuptable[i]=kernelindex;
      }
   }
   else {
      // lookuptable just returns the patchindex
   }
}

uint4 * HyPerConn::getRandState(int index) {
   uint4 * state = NULL;
   if (pvpatchAccumulateType==ACCUMULATE_STOCHASTIC) {
      state = randState->getRNG(index);
   }
   return state;
}


InitWeights * HyPerConn::getDefaultInitWeightsMethod(const char * keyword) {
   fprintf(stderr, "weightInitType not set or unrecognized.  Using default method.\n");
   InitWeights * initWeightsObj = new InitWeights(this);
   return initWeightsObj;
}

InitWeights * HyPerConn::handleMissingInitWeights(PVParams * params) {
   return new InitWeights(this);
}

//#ifdef PV_USE_OPENCL
//void HyPerConn::initIgnoreGPUFlag() {
//   PVParams * params = parent->parameters();
//   ignoreGPUflag=false;
//   ignoreGPUflag = params->value(name, "ignoreGPU", ignoreGPUflag);
//}
////this method sets up GPU related variables and calls the
////initializeThreadBuffers and initializeThreadKernels
//int HyPerConn::initializeGPU() {
//   initIgnoreGPUFlag();
//   //if((gpuAccelerateFlag)&&(ignoreGPUflag)) post->copyChannelToDevice();
//   int totwait = numberOfAxonalArborLists();
//   evRecvSynWaitList = (cl_event *) malloc(totwait*sizeof(cl_event));
//   numWait = 0;
//
//   nxl = 16;
//   nyl = 8;
//
//   const char* kernel_name = "HyPerLayer_recv_synaptic_input";
//   initializeThreadBuffers(kernel_name);
//   initializeThreadKernels(kernel_name);
//   //pre->initializeDataStoreThreadBuffers();
//
//   return PV_SUCCESS;
//}


///**
// * Initialize OpenCL buffers.  This must be called after weights have
// * been allocated.
// */


#if defined(PV_USE_OPENCL) || defined(PV_USE_CUDA)
int HyPerConn::allocateDeviceBuffers()
{
   int status = 0;

#ifdef PV_USE_OPENCL
   CLDevice * device = parent->getCLDevice();
#endif
#ifdef PV_USE_CUDA
   PVCuda::CudaDevice * device = parent->getCudaDevice();
#endif

   //We need orig to set syp
   //This is a patch, the real fix is to store post weights if receiving from post
   if(receiveGpu){
      if(updateGSynFromPostPerspective){
         HyPerConn * origConn;

         TransposeConn* thisTranspose = dynamic_cast<TransposeConn*>(this);
         if(!thisTranspose){
            std::cout << "HyPerConn " << name << " must be a transpose conn to receive with gpus for now\n";
            exit(EXIT_FAILURE);
         }
         origConn = thisTranspose->getOriginalConn();

         if(allocDeviceWeights){
            const size_t size = origConn->getNumDataPatches() * origConn->xPatchSize()*origConn->yPatchSize()*origConn->fPatchSize() * sizeof(pvwdata_t);
            //pvwdata_t * wBuf = get_wDataStart(arbor);
            //clWeights[arbor] = device->createReadBuffer(size, (void*)wBuf);
            //TODO change to read only
#ifdef PV_USE_OPENCL
            origConn->setDeviceWData(device->createBuffer(CL_MEM_READ_ONLY, size, NULL));
#endif
#ifdef PV_USE_CUDA
            origConn->setDeviceWData(device->createBuffer(size));
#endif
         }

         int numPostRes = post->getNumNeurons();
#ifdef PV_USE_OPENCL
         d_PostToPreActivity = device->createBuffer(CL_MEM_READ_ONLY, numPostRes*sizeof(long), NULL); 
#endif
#ifdef PV_USE_CUDA
         d_PostToPreActivity = device->createBuffer(numPostRes*sizeof(long)); 
#endif

         if(sharedWeights){
            int numWeightPatches = origConn->getNumWeightPatches();
#ifdef PV_USE_OPENCL
            d_Patch2DataLookupTable = device->createBuffer(CL_MEM_READ_ONLY, numWeightPatches * sizeof(int), NULL);  
#endif
#ifdef PV_USE_CUDA
            d_Patch2DataLookupTable = device->createBuffer(numWeightPatches * sizeof(int));  
#endif
         }
      }
      else{
         if(allocDeviceWeights){
            const size_t size = getNumDataPatches() * xPatchSize() * yPatchSize() * fPatchSize() * sizeof(pvwdata_t);
#ifdef PV_USE_OPENCL
            d_WData = device->createBuffer(CL_MEM_READ_ONLY, size, NULL);
#endif
#ifdef PV_USE_CUDA
            d_WData = device->createBuffer(size);
#endif
            assert(d_WData);
         }

         //Calculate local pre size here
         const PVLayerLoc * preLoc = pre->getLayerLoc();
         const PVLayerLoc * postLoc = post->getLayerLoc();
         PVHalo haloPre;
         PVHalo haloPost;
         
         //Set local sizes here
         float preToPostScaleX = (float)preLoc->nx/((float)postLoc->nx);
         float preToPostScaleY = (float)preLoc->ny/((float)postLoc->ny);

         //See the size of buffer needed based on x and y
         if(preToPostScaleX >= 1){
            localPreX = (nxp * preToPostScaleX) + (preToPostScaleX * (postGroupXSize - 1));
         }
         else{
            localPreX = nxp + ((preToPostScaleX * postGroupXSize) - 1);
         }
         if(preToPostScaleY >= 1){
            localPreY = (nyp * preToPostScaleY) + (preToPostScaleY * (postGroupYSize - 1));
         }
         else{
            localPreY = nyp + ((preToPostScaleY * postGroupYSize) - 1);
         }

         std::cout << "nxp: " << nxp << " nyp: " << nyp << " preToPostScaleX:" << preToPostScaleX << " preToPostScaleY:" << preToPostScaleY << " postGroupXSize: " << postGroupXSize << " postGroupYSize: " << postGroupYSize << "\n";

         //Can only do squares because of the nb
         if(localPreX != localPreY){
            std::cout << "Local pre size x of " << localPreX << " must be the same as local pre size y of " << localPreY << ", so post group sizes must be square\n";
            exit(EXIT_FAILURE);
         }

         int preNf = preLoc->nf;
         int postNf = postLoc->nf;

         std::cout << "preToPostScale: (" << preToPostScaleX << "," << preToPostScaleY << ")\n";
         std::cout << "local sizes: (" << localPreX << "," << localPreY << ")\n";

         //This should be the case with petavision restrictions
         assert(postNf == nfp);

         int localPreNb;
         //Find border of pre to post
         if(preToPostScaleX > 1){
            localPreNb = ((nxp-1) * preToPostScaleX)/2;
         }
         else{
            localPreNb = ((nxp*preToPostScaleX) - 1)/2;
         }
         std::cout << "localPreNb: " << localPreNb << " preToPostScaleX: " << preToPostScaleX << "\n";
         
         //Need a buffer for PVPatch for one arbor, one for each local pre 
         //localPreX and Y are in extended space
         int numWeightPatches = localPreX * localPreY * preNf;
         int patchSize = numWeightPatches * sizeof(PVPatch);
#ifdef PV_USE_OPENCL
         d_Patches = device->createBuffer(CL_MEM_READ_ONLY, patchSize, NULL); 
#endif
#ifdef PV_USE_CUDA
         d_Patches = device->createBuffer(patchSize); 
#endif

         //Need a buffer for gsynpatch start for one arbor
         int gsynPatchStartIndexSize = numWeightPatches * sizeof(size_t);
#ifdef PV_USE_OPENCL
         d_GSynPatchStart = device->createBuffer(CL_MEM_READ_ONLY, gsynPatchStartIndexSize, NULL); 
#endif
#ifdef PV_USE_CUDA
         d_GSynPatchStart = device->createBuffer(gsynPatchStartIndexSize); 
#endif

         //Need a local host buffer for the new localGSynPatchStart
         localGSynPatchStart = (size_t*) malloc(gsynPatchStartIndexSize);

         haloPre.lt = localPreNb;
         haloPre.rt = localPreNb;
         haloPre.dn = localPreNb;
         haloPre.up = localPreNb;
         haloPost.lt = 0;
         haloPost.rt = 0;
         haloPost.dn = 0;
         haloPost.up = 0;

         //Create clPatches for local space
         localWPatches = createPatches(numWeightPatches, nxp, nyp);
         //Shrink weights with spoofed sizes 
         adjustAllPatches(
               localPreX - 2*localPreNb,
               localPreY - 2*localPreNb,
               preNf,
               &haloPre,
               postGroupXSize,
               postGroupYSize,
               postNf,
               &haloPost,
               &(localWPatches),
               &(localGSynPatchStart),
               NULL,
               0); //Doing only the 0th arbor

         //Set device patches and gsynpatchstart
         d_Patches->copyToDevice(localWPatches[0]);
         d_GSynPatchStart->copyToDevice(localGSynPatchStart);

         int numPostRes = post->getNumNeurons();
#ifdef PV_USE_OPENCL
         d_PostToPreActivity = device->createBuffer(CL_MEM_READ_ONLY, numPostRes*sizeof(long), NULL); 
#endif
#ifdef PV_USE_CUDA
         d_PostToPreActivity = device->createBuffer(numPostRes*sizeof(long)); 
#endif

         if(sharedWeights){
            int numWeightPatches = getNumWeightPatches();
#ifdef PV_USE_OPENCL
            d_Patch2DataLookupTable = device->createBuffer(CL_MEM_READ_ONLY, numWeightPatches * sizeof(int), NULL);  
#endif
#ifdef PV_USE_CUDA
            d_Patch2DataLookupTable = device->createBuffer(numWeightPatches * sizeof(int));  
#endif
         }
      }
   }
   return status;
}

int HyPerConn::allocateReceivePreKernel()
{
   std::cout << name << " setting up pre kernel\n";

#ifdef PV_USE_OPENCL
   int status = CL_SUCCESS;
   const char* kernel_name = "HyPerLayer_recv_pre";
   char kernelPath[PV_PATH_MAX+128];
   char kernelFlags[PV_PATH_MAX+128];

   CLDevice * device = parent->getCLDevice();

   sprintf(kernelPath, "%s/../src/kernels/%s.cl", parent->getSrcPath(), kernel_name);
   sprintf(kernelFlags, "-D PV_USE_OPENCL -cl-fast-relaxed-math -I %s/../src/kernels/", parent->getSrcPath());

   // create kernels
   krRecvPre = device->createKernel(kernelPath, kernel_name, kernelFlags);
#endif

#ifdef PV_USE_CUDA
   int status = 0;
   PVCuda::CudaDevice * device = parent->getCudaDevice();
   krRecvPre = new PVCuda::CudaRecvPre(device);
#endif

   const PVLayerLoc * preLoc = pre->getLayerLoc();
   const PVLayerLoc * postLoc = post->getLayerLoc();
   const PVHalo * preHalo = &pre->getLayerLoc()->halo;
   const PVHalo * postHalo = &post->getLayerLoc()->halo;

#ifdef PV_USE_OPENCL
   CLBuffer* d_PreData = pre->getDeviceActivity();
   CLBuffer* d_PostGSyn = post->getDeviceGSyn(channel);
#endif

#ifdef PV_USE_CUDA
   PVCuda::CudaBuffer* d_PreData = pre->getDeviceActivity();
   PVCuda::CudaBuffer* d_PostGSyn = post->getDeviceGSyn(channel);
#endif

   assert(d_PreData);
   assert(d_PostGSyn);

   assert(d_WData);
   assert(d_Patches);
   assert(d_GSynPatchStart);


   int preNxExt = preLoc->nx + preHalo->lt + preHalo->rt;
   int preNyExt = preLoc->ny + preHalo->up + preHalo->dn;
   int preNf = preLoc->nf;
   int postNxRes = postLoc->nx;
   int postNyRes = postLoc->ny;
   int postNf = postLoc->nf;

   int nxp = xPatchSize();
   int nyp = yPatchSize();
   int nfp = fPatchSize();
   float dt_factor = post->getConvertToRateDeltaTimeFactor(this);
   int i_sharedWeights = sharedWeights;

   int sy = getPostNonextStrides()->sy;
   int syw = yPatchStride();

   //Since it never changes, set this buffer here
   d_PostToPreActivity->copyToDevice(postToPreActivity);
   d_Patch2DataLookupTable->copyToDevice(getPatchToDataLUT());

   //Need to calculate new patches for weights
   //= conn->weights(arborID)[0]; //0 beacuse it's one block of memory
   //CLBuffer * d_patches = getClPatches();
   //

   if(numFLocal != 1){
      std::cout << "numFLocal must be 1 for recv from pre\n";
      exit(EXIT_FAILURE);
   }

   //In receive from pre, we need to make sure x, y post group size is divisible by the actual number of post neurons
   if(postLoc->nx % postGroupXSize != 0){
      std::cout << "X post group size of " << postGroupXSize << " is not divisible by post nx of " << postLoc->nx << "\n";
      exit(EXIT_FAILURE);
   }

   if(postLoc->ny % postGroupYSize != 0){
      std::cout << "Y post group size of " << postGroupYSize << " is not divisible by post ny of " << postLoc->ny << "\n";
      exit(EXIT_FAILURE);
   }

   numPostGroupX = postLoc->nx / postGroupXSize;
   numPostGroupY = postLoc->ny / postGroupYSize;

   //In receive from pre, we need to make sure local size x and y are divisible by numPostGroup

   if(numPostGroupX % numXLocal!= 0){
      std::cout << "X local size of " << numXLocal << " is not divisible by number of post groups of " << numPostGroupX << "\n";
      exit(EXIT_FAILURE);
   }

   if(numPostGroupY % numYLocal != 0){
      std::cout << "Y local size of " << numYLocal << " is not divisible by number of post groups of " << numPostGroupY << "\n";
      exit(EXIT_FAILURE);
   }

   //Set local sizes here
   float preToPostScaleX = (float)preLoc->nx/((float)postLoc->nx);
   float preToPostScaleY = (float)preLoc->ny/((float)postLoc->ny);

   int localBufSizeX;
   int localBufSizeY;
   //See the size of buffer needed based on x and y
   if(preToPostScaleX >= 1){
      localBufSizeX = (nxp*preToPostScaleX) + (preToPostScaleX * ((numXLocal *postGroupXSize) - 1));
   }
   else{
      localBufSizeX = nxp + ((preToPostScaleX * numXLocal * postGroupXSize) - 1);
   }
   if(preToPostScaleY >= 1){
      localBufSizeY = (nyp*preToPostScaleY) + (preToPostScaleY * ((numYLocal*postGroupYSize) - 1));
   }
   else{
      localBufSizeY = nyp + ((preToPostScaleY * numYLocal * postGroupYSize) - 1);
   }

#ifdef PV_USE_OPENCL   
   //Set arguments
   int argid = 0;


   std::cout << "localPre: " << localPreX << "," << localPreY << "," << preNf << "\n";

   status |= krRecvPre->setKernelArg(argid++, sizeof(int), &preNxExt);
   status |= krRecvPre->setKernelArg(argid++, sizeof(int), &preNyExt);
   status |= krRecvPre->setKernelArg(argid++, sizeof(int), &preNf);
   status |= krRecvPre->setKernelArg(argid++, sizeof(int), &postNxRes);
   status |= krRecvPre->setKernelArg(argid++, sizeof(int), &postNyRes);
   status |= krRecvPre->setKernelArg(argid++, sizeof(int), &postNf);

   status |= krRecvPre->setKernelArg(argid++, sizeof(int), &nxp);
   status |= krRecvPre->setKernelArg(argid++, sizeof(int), &nyp);
   status |= krRecvPre->setKernelArg(argid++, sizeof(int), &nfp);
   status |= krRecvPre->setKernelArg(argid++, sizeof(int), &postGroupXSize);
   status |= krRecvPre->setKernelArg(argid++, sizeof(int), &postGroupYSize);
   status |= krRecvPre->setKernelArg(argid++, sizeof(int), &localPreX);
   status |= krRecvPre->setKernelArg(argid++, sizeof(int), &localPreY);
   status |= krRecvPre->setKernelArg(argid++, sizeof(int), &localBufSizeX);
   status |= krRecvPre->setKernelArg(argid++, sizeof(int), &localBufSizeY);

   status |= krRecvPre->setKernelArg(argid++, sizeof(int), &sy);
   status |= krRecvPre->setKernelArg(argid++, sizeof(int), &syw);
   status |= krRecvPre->setKernelArg(argid++, sizeof(float), &dt_factor);
   status |= krRecvPre->setKernelArg(argid++, sizeof(int), &i_sharedWeights);

   status |= krRecvPre->setKernelArg(argid++, d_Patches);
   status |= krRecvPre->setKernelArg(argid++, d_GSynPatchStart);

   status |= krRecvPre->setKernelArg(argid++, d_PostToPreActivity);
   status |= krRecvPre->setKernelArg(argid++, d_PreData);
   status |= krRecvPre->setKernelArg(argid++, d_WData);
   status |= krRecvPre->setKernelArg(argid++, d_PostGSyn);
   status |= krRecvPre->setKernelArg(argid++, d_Patch2DataLookupTable);

   //Local buffers
   //status |= krRecvPre->setKernelArg(argid++, sizeof(float) * localBufSizeX * localBufSizeY * preNf, NULL);
   //status |= krRecvPre->setKernelArg(argid++, sizeof(float) * (postGroupXSize * numXLocal) * (postGroupYSize * numYLocal) * postNf, NULL);
#endif
#ifdef PV_USE_CUDA
   krRecvPre->setArgs(
      preNxExt,
      preNyExt,
      preNf,
      postNxRes,
      postNyRes,
      postNf,

      nxp,
      nyp,
      nfp,
      postGroupXSize,
      postGroupYSize,
      localPreX,
      localPreY,
      localBufSizeX,
      localBufSizeY,
      
      sy,
      syw,
      dt_factor,
      i_sharedWeights,
      d_Patches,
      d_GSynPatchStart,
      d_PostToPreActivity,
      d_PreData,
      d_WData,
      d_PostGSyn,
      d_Patch2DataLookupTable
   );
#endif
   return status;
}

int HyPerConn::allocateReceivePostKernel()
{
   std::cout << name << " setting up post kernel\n";
#ifdef PV_USE_OPENCL
   int status = CL_SUCCESS;
   const char* kernel_name = "HyPerLayer_recv_post";
   char kernelPath[PV_PATH_MAX+128];
   char kernelFlags[PV_PATH_MAX+128];

   CLDevice * device = parent->getCLDevice();

   sprintf(kernelPath, "%s/../src/kernels/%s.cl", parent->getSrcPath(), kernel_name);
   sprintf(kernelFlags, "-D PV_USE_OPENCL -cl-fast-relaxed-math -I %s/../src/kernels/", parent->getSrcPath());

   // create kernels
   krRecvPost = device->createKernel(kernelPath, kernel_name, kernelFlags);
#endif

#ifdef PV_USE_CUDA
   int status = 0;
   PVCuda::CudaDevice * device = parent->getCudaDevice();
   krRecvPost = new PVCuda::CudaRecvPost(device);
#endif

   //We need orig to set syp
   //This is a patch, the real fix is to store post weights if receiving from post
   TransposeConn* thisTranspose = dynamic_cast<TransposeConn*>(this);
   if(!thisTranspose){
      std::cout << "HyPerConn " << name << " must be a transpose conn to receive with gpus for now\n";
      exit(EXIT_FAILURE);
   }

   HyPerConn * origConn = thisTranspose->getOriginalConn();
   const PVLayerLoc * preLoc = pre->getLayerLoc();
   const PVLayerLoc * postLoc = post->getLayerLoc();
   const PVHalo* preHalo = &pre->getLayerLoc()->halo;
   const PVHalo* postHalo = &post->getLayerLoc()->halo;




#ifdef PV_USE_OPENCL
   CLBuffer* d_PreData = pre->getDeviceActivity();
   CLBuffer* d_PostGSyn = post->getDeviceGSyn(channel);
   CLBuffer * d_origWData = origConn->getDeviceWData();
#endif
#ifdef PV_USE_CUDA
   PVCuda::CudaBuffer* d_PreData = pre->getDeviceActivity();
   PVCuda::CudaBuffer* d_PostGSyn = post->getDeviceGSyn(channel);
   PVCuda::CudaBuffer * d_origWData = origConn->getDeviceWData();
#endif

   assert(d_PreData);
   assert(d_PostGSyn);
   assert(d_origWData);

   int sy  = (preLoc->nx+preHalo->rt+preHalo->lt)*preLoc->nf;
   int syp = origConn->yPatchStride();
   int numPerStride = origConn->xPatchSize() * origConn->fPatchSize();
   float dt_factor = post->getConvertToRateDeltaTimeFactor(this);
   int i_sharedWeights = sharedWeights;

   const PVHalo* oHalo = &origConn->preSynapticLayer()->getLayerLoc()->halo;
   int oNblt = oHalo->lt;
   int oNbrt = oHalo->rt;
   int oNbup = oHalo->up;
   int oNbdn = oHalo->dn;

   //nxp, nyp, and nfp should be orig conn's
   int oNxp = origConn->xPatchSize();
   int oNyp = origConn->yPatchSize();
   int oNfp = origConn->fPatchSize();
   int postNx = postLoc->nx;
   int postNy = postLoc->ny;
   int postNf = postLoc->nf;

   //Set local sizes here
   float preToPostScaleX = (float)preLoc->nx/((float)postLoc->nx);
   float preToPostScaleY = (float)preLoc->ny/((float)postLoc->ny);

   //Since it never changes, set this buffer here
   //Need to set orig connection's patch2datalookuptable
   d_PostToPreActivity->copyToDevice(postToPreActivity);

   d_Patch2DataLookupTable->copyToDevice(origConn->getPatchToDataLUT());

   //In receive from post, we need to make sure x, y, and f local size is divisible by the actual number of post neurons
   if(postLoc->nx % numXLocal != 0){
      std::cout << "X local size of " << numXLocal << " is not divisible by post nx of " << postLoc->nx << "\n";
      exit(EXIT_FAILURE);
   }

   if(postLoc->ny % numYLocal != 0){
      std::cout << "Y local size of " << numYLocal << " is not divisible by post ny of " << postLoc->ny << "\n";
      exit(EXIT_FAILURE);
   }
   ////In recv from post, numYLocal must be equal to 1. The locality is best used in x direction
   //if(numYLocal != 1){
   //   std::cout << "Y local size must equal to 1 in recv from post\n";
   //   exit(EXIT_FAILURE);
   //}

   if(postLoc->nf % numFLocal != 0){
      std::cout << "F local size of " << numFLocal << " is not divisible by post nf of " << postLoc->nf << "\n";
      exit(EXIT_FAILURE);
   }


   int localBufSizeX;
   int localBufSizeY;
   //See the size of buffer needed based on x and y
   //oNxp is the patch size from the post point of view
   if(preToPostScaleX >= 1){
      localBufSizeX = oNxp + (preToPostScaleX * (numXLocal - 1));
   }
   else{
      localBufSizeX = oNxp + ((preToPostScaleX * numXLocal) - 1);
   }
   if(preToPostScaleY >= 1){
      localBufSizeY = oNyp + (preToPostScaleY * (numYLocal - 1));
   }
   else{
      localBufSizeY = oNyp + ((preToPostScaleY * numYLocal) - 1);
   }

   std::cout << "preToPostScale: (" << preToPostScaleX << "," << preToPostScaleY << ")\n";
   std::cout << "patch size: (" << oNxp << "," << oNyp << ") numLocal: (" << numXLocal << "," << numYLocal << ")\n";
   std::cout << "local sizes: (" << localBufSizeX << "," << localBufSizeY << ")\n";
   
#ifdef PV_USE_OPENCL
   //Set arguments
   int argid = 0;
   int tmpArbor = 0;

   status |= krRecvPost->setKernelArg(argid++, sizeof(int), &postNx);
   status |= krRecvPost->setKernelArg(argid++, sizeof(int), &postNy);
   status |= krRecvPost->setKernelArg(argid++, sizeof(int), &postNf);
   status |= krRecvPost->setKernelArg(argid++, sizeof(int), &oNblt);
   status |= krRecvPost->setKernelArg(argid++, sizeof(int), &oNbrt);
   status |= krRecvPost->setKernelArg(argid++, sizeof(int), &oNbdn);
   status |= krRecvPost->setKernelArg(argid++, sizeof(int), &oNbup);
   status |= krRecvPost->setKernelArg(argid++, sizeof(int), &oNxp);
   status |= krRecvPost->setKernelArg(argid++, sizeof(int), &oNyp);
   status |= krRecvPost->setKernelArg(argid++, sizeof(int), &oNfp);

   status |= krRecvPost->setKernelArg(argid++, sizeof(int), &localBufSizeX);
   status |= krRecvPost->setKernelArg(argid++, sizeof(int), &localBufSizeY);
   status |= krRecvPost->setKernelArg(argid++, sizeof(float), &preToPostScaleX);
   status |= krRecvPost->setKernelArg(argid++, sizeof(float), &preToPostScaleY);

   status |= krRecvPost->setKernelArg(argid++, sizeof(int), &sy);
   status |= krRecvPost->setKernelArg(argid++, sizeof(int), &syp);
   status |= krRecvPost->setKernelArg(argid++, sizeof(int), &numPerStride);
   status |= krRecvPost->setKernelArg(argid++, sizeof(float), &dt_factor);
   status |= krRecvPost->setKernelArg(argid++, sizeof(int), &(i_sharedWeights));

   status |= krRecvPost->setKernelArg(argid++, d_PostToPreActivity);
   status |= krRecvPost->setKernelArg(argid++, d_PreData);
   status |= krRecvPost->setKernelArg(argid++, d_origWData);
   status |= krRecvPost->setKernelArg(argid++, d_PostGSyn);
   status |= krRecvPost->setKernelArg(argid++, d_Patch2DataLookupTable);
   //Buffer for pre activity 
   //status |= krRecvPost->setKernelArg(argid++, sizeof(float) * localBufSizeX * localBufSizeY * oNfp, NULL);
   //Buffer for post gsyn. One per neuron in workgroup
   status |= krRecvPost->setKernelArg(argid++, sizeof(float) * numXLocal * numYLocal * numFLocal, NULL);

#endif

#ifdef PV_USE_CUDA
   krRecvPost->setArgs(
      postNx, //num post neurons
      postNy,
      postNf,

      oNblt, //Border of orig
      oNbrt, //Border of orig
      oNbdn, //Border of orig
      oNbup, //Border of orig

      oNxp,
      oNyp,
      oNfp,

      localBufSizeX,
      localBufSizeY,
      preToPostScaleX,
      preToPostScaleY,

      sy,
      syp,
      numPerStride,
      dt_factor,
      i_sharedWeights,

      d_PostToPreActivity,
      d_PreData,
      d_origWData,
      d_PostGSyn,
      d_Patch2DataLookupTable
   );
#endif
   return status;
}

#endif

//
//int HyPerConn::initializeThreadKernels(const char * kernel_name)
//{
//   char kernelPath[PV_PATH_MAX+128];
//   char kernelFlags[PV_PATH_MAX+128];
//
//   int status = CL_SUCCESS;
//   CLDevice * device = parent->getCLDevice();
//
//   const char * pvRelPath = "../PetaVision";
//   sprintf(kernelPath, "%s/%s/src/kernels/%s.cl", parent->getSrcPath(), pvRelPath, kernel_name);
//   sprintf(kernelFlags, "-D PV_USE_OPENCL -cl-fast-relaxed-math -I %s/%s/src/kernels/", parent->getSrcPath(), pvRelPath);
//
//   // create kernels
//   //
//
//   krRecvSyn = device->createKernel(kernelPath, kernel_name, kernelFlags);
//
//   const PVLayerLoc * preLoc  = pre-> getLayerLoc();
//   const PVLayerLoc * postLoc = post->getLayerLoc();
//
//   int argid = 0;
//
//   status |= krRecvSyn->setKernelArg(argid++, preLoc->nx);
//   status |= krRecvSyn->setKernelArg(argid++, preLoc->ny);
//   status |= krRecvSyn->setKernelArg(argid++, preLoc->nf);
//   status |= krRecvSyn->setKernelArg(argid++, preLoc->nb);
//
//   status |= krRecvSyn->setKernelArg(argid++, nxp);
//   status |= krRecvSyn->setKernelArg(argid++, nyp);
//   status |= krRecvSyn->setKernelArg(argid++, nfp);
//
//   float fScale = (float)postLoc->nf/(float)preLoc->nf;
//   float xScale = (float)postLoc->nx/(float)preLoc->nx;
//   float yScale = (float)postLoc->ny/(float)preLoc->ny;
//   status |= krRecvSyn->setKernelArg(argid++, fScale);
//   status |= krRecvSyn->setKernelArg(argid++, xScale);
//   status |= krRecvSyn->setKernelArg(argid++, yScale);
//
//   clArgIdOffset = argid;  // offset into activity buffer (with delay)
//   argid++;
//   status |= krRecvSyn->setKernelArg(argid++, clPatch2DataLookUpTable);
//   // activity buffer from DataStore
//   clArgIdDataStore=argid;
//   argid++;
//   clArgIdWeights = argid; // weights
//   status |= krRecvSyn->setKernelArg(argid++, clWeights[0]);
//   // update variable, GSyn
//   status |= krRecvSyn->setKernelArg(argid++, post->getNumNeurons()*getChannel());
//   status |= krRecvSyn->setKernelArg(argid++, post->getChannelCLBuffer());
//
//   return status;
//}
//#endif // PV_USE_OPENCL

#ifdef OBSOLETE // Marked obsolete Mar 19, 2014
int HyPerConn::checkPVPFileHeader(Communicator * comm, const PVLayerLoc * loc, int params[], int numParams)
{
   // use default header checker
   //
   return pvp_check_file_header(comm, loc, params, numParams);
}

int HyPerConn::checkWeightsHeader(const char * filename, const int * wgtParams)
{
   // extra weight parameters
   //
   const int nxpFile = wgtParams[NUM_BIN_PARAMS + INDEX_WGT_NXP];
   if (nxp != nxpFile) {
      fprintf(stderr,
              "ignoring nxp = %i in HyPerConn %s, using nxp = %i in binary file %s\n",
              nxp, name, nxpFile, filename);
      nxp = nxpFile;
   }

   const int nypFile = wgtParams[NUM_BIN_PARAMS + INDEX_WGT_NYP];
   if (nyp != nypFile) {
      fprintf(stderr,
              "ignoring nyp = %i in HyPerConn %s, using nyp = %i in binary file %s\n",
              nyp, name, nypFile, filename);
      nyp = nypFile;
   }

   nfp = wgtParams[NUM_BIN_PARAMS + INDEX_WGT_NFP];
   // const int nfpFile = wgtParams[NUM_BIN_PARAMS + INDEX_WGT_NFP];
   // if (nfp != nfpFile) {
   //    fprintf(stderr,
   //            "ignoring nfp = %i in HyPerConn %s, using nfp = %i in binary file %s\n",
   //            nfp, name, nfpFile, filename);
   //    nfp = nfpFile;
   // }
   return 0;
}
#endif // OBSOLETE

int HyPerConn::writeWeights(double timed, bool last)
{
   PVPatch *** patches_arg = sharedWeights ? NULL : wPatches;
   return writeWeights(patches_arg, get_wDataStart(), getNumDataPatches(), NULL, timed, writeCompressedWeights, last);
}

int HyPerConn::writeWeights(const char * filename) {
   PVPatch *** patches_arg = sharedWeights ? NULL : wPatches;
   return writeWeights(patches_arg, get_wDataStart(), getNumDataPatches(), filename, parent->simulationTime(), writeCompressedWeights, true);
}

int HyPerConn::writeWeights(PVPatch *** patches, pvwdata_t ** dataStart, int numPatches,
      const char * filename, double timed, bool compressWeights, bool last) {
   int status = PV_SUCCESS;
   char path[PV_PATH_MAX];

   float minVal = FLT_MAX;
   float maxVal = -FLT_MAX;
   for(int arbor=0; arbor<this->numberOfAxonalArborLists(); arbor++) {
      float minVal1 = minWeight(arbor);
      if( minVal1 < minVal ) minVal = minVal1;
      float maxVal1 = maxWeight(arbor);
      if( maxVal1 > maxVal ) maxVal = maxVal1;
   }

   const PVLayerLoc * loc = pre->getLayerLoc();

   int chars_needed = 0;
   if (filename == NULL) {
      assert(parent->includeConnectionName()<=2 && parent->includeConnectionName()>=0);
      switch(parent->includeConnectionName()) {
      case 0:
         chars_needed = snprintf(path, PV_PATH_MAX, "%s/w%d.pvp", parent->getOutputPath(), getConnectionId());
         break;
      case 1:
         chars_needed = snprintf(path, PV_PATH_MAX, "%s/w%d_%s.pvp", parent->getOutputPath(), getConnectionId(), name);
         break;
      case 2:
         chars_needed = snprintf(path, PV_PATH_MAX, "%s/%s.pvp", parent->getOutputPath(), name);
         break;
      default:
         assert(0);
         break;
      }
   }
   else {
      chars_needed = snprintf(path, PV_PATH_MAX, "%s", filename);
   }
   if (chars_needed >= PV_PATH_MAX) {
      fprintf(stderr, "HyPerConn::writeWeights in connection \"%s\": path is too long (it would be cut off as \"%s\")\n", name, path);
      abort();
   }

   Communicator * comm = parent->icCommunicator();

   bool append = last ? false : ioAppend;

   status = PV::writeWeights(path, comm, (double) timed, append, loc, nxp, nyp,
			nfp, minVal, maxVal, patches, dataStart, numPatches,
			numberOfAxonalArborLists(), compressWeights, fileType);
   assert(status == 0);

   return status;
}

int HyPerConn::writeTextWeights(const char * filename, int k)
{
   if (parent->icCommunicator()->commSize()>1) {
      fprintf(stderr, "writeTextWeights error for connection \"%s\": writeTextWeights is not compatible with MPI", name);
      abort();
      // NOTE : if run under MPI when more than one process sees the same file system, the contending processes will clobber each other.
   }
   PV_Stream * pvstream = NULL;

   if (filename != NULL) {
      char outfile[PV_PATH_MAX];
      snprintf(outfile, PV_PATH_MAX-1, "%s/%s", parent->getOutputPath(), filename);
      pvstream = PV_fopen(outfile, "w");
   }
   else {
      pvstream = PV_stdout();
   }
   if (pvstream == NULL) {
     fprintf(stderr, "writeWeights: ERROR opening file \"%s\"\n", filename);
     return PV_FAILURE;
   }

   FILE * fd = pvstream->fp;
   fprintf(fd, "Weights for connection \"%s\", neuron %d\n", name, k);
   fprintf(fd, "   (kxPre,kyPre,kfPre)   = (%i,%i,%i)\n",
           kxPos(k,pre->getLayerLoc()->nx + pre->getLayerLoc()->halo.lt + pre->getLayerLoc()->halo.rt,
                 pre->getLayerLoc()->ny + pre->getLayerLoc()->halo.dn + pre->getLayerLoc()->halo.up, pre->getLayerLoc()->nf),
           kyPos(k,pre->getLayerLoc()->nx + pre->getLayerLoc()->halo.lt + pre->getLayerLoc()->halo.rt,
                 pre->getLayerLoc()->ny + pre->getLayerLoc()->halo.dn + pre->getLayerLoc()->halo.up, pre->getLayerLoc()->nf),
           featureIndex(k,pre->getLayerLoc()->nx + pre->getLayerLoc()->halo.lt + pre->getLayerLoc()->halo.rt,
                 pre->getLayerLoc()->ny + pre->getLayerLoc()->halo.dn + pre->getLayerLoc()->halo.up, pre->getLayerLoc()->nf) );
   fprintf(fd, "   (nxp,nyp,nfp)   = (%i,%i,%i)\n", (int) nxp, (int) nyp, (int) nfp);
   fprintf(fd, "   pre  (nx,ny,nf) = (%i,%i,%i)\n",
           pre->getLayerLoc()->nx, pre->getLayerLoc()->ny, pre->getLayerLoc()->nf);
   fprintf(fd, "   post (nx,ny,nf) = (%i,%i,%i)\n",
           post->getLayerLoc()->nx, post->getLayerLoc()->ny, post->getLayerLoc()->nf);
   fprintf(fd, "\n");

   for(int arbor = 0; arbor<numberOfAxonalArborLists(); arbor++) {
      fprintf(fd, "displaying arbor %1.1d\n", arbor);
      // give a chance for derived classes to add extra information
      //
      writeTextWeightsExtra(pvstream, k, arbor);
      pv_text_write_patch(pvstream, wPatches[arbor][k], get_wData(arbor,k), nfp, sxp, syp, sfp);
      fprintf(fd, "----------------------------\n");
   }

   PV_fclose(pvstream);

   return 0;
}

//Input delay is in ms
void HyPerConn::setDelay(int arborId, float delay) {
   assert(arborId>=0 && arborId<numAxonalArborLists);
   int intDelay = round(delay/parent->getDeltaTime());
   if (fmod(delay, parent->getDeltaTime()) != 0){
      float actualDelay = intDelay * parent->getDeltaTime();
      std::cerr << name << ": A delay of " << delay << " will be rounded to " << actualDelay << "\n";
   }
   delays[arborId] = (int)(round(delay / parent->getDeltaTime()));
}

//#ifdef PV_USE_OPENCL
//int HyPerConn::deliverOpenCL(Publisher * pub, const PVLayerCube * cube)
//{
//   int status = PV_SUCCESS;
//
//
//   const PVLayerLoc * preLoc = pre->getLayerLoc();
//   const size_t nxex = (preLoc->nx + 2*preLoc->nb)*preLoc->nf;
//   const size_t nyex = preLoc->ny + 2*preLoc->nb;
//   while((nxex%nxl!=0)&&(nxl>1)) {nxl--;}
//   while((nyex%nyl!=0)&&(nyl>1)) {nyl--;}
//
//   status |= krRecvSyn->setKernelArg(clArgIdDataStore, pre->getLayerDataStoreCLBuffer());
//
//   status |= pre->waitForDataStoreCopy();
//
//
//   // for all numextended in pre
//
//   post->startTimer();
//
//
//   int arborCnt=numberOfAxonalArborLists();
//   for (int arbor = 0; arbor < arborCnt; arbor++) {
//      int delay = getDelay(arbor);
//      size_t activityOffset = pre->getLayerDataStoreOffset(delay);
//      status |= krRecvSyn->setKernelArg(clArgIdOffset, activityOffset/sizeof(pvdata_t)); //need to convert offset to an array index offset
//      status |= krRecvSyn->setKernelArg(clArgIdWeights, clWeights[arbor]);
//      status |= krRecvSyn->run(nxex, nyex, nxl, nyl, 0, NULL, &evRecvSynWaitList[arbor]);
//      numWait++;
//   }
//
//   // TODO - use events properly
//   status |= clWaitForEvents(numWait, evRecvSynWaitList);
//   for (int i = 0; i < numWait; i++) {
//      clReleaseEvent(evRecvSynWaitList[i]);
//   }
//   numWait = 0;
//
//   post->stopTimer();
//
//   int arborId=0;
//   int delay = getDelay(arborId);
//   pub->readData(delay);
//   const PVLayerLoc * postLoc = post->getLayerLoc();
//   //define global location:
//   int kx=nxex/2; int ky=nyex/2;
//   //int kPre=ky*nxex+kx;
//   int gstart=0;//post->getNumNeurons()*getChannel();
//   float * gTempBuf= (float*) calloc(sizeof(float), post->getNumNeurons());
//   int * lutpointer = getLUTpointer();
//   const int numWeightPatches = getNumWeightPatches();
//   bool freelutpointer=false;
//   if(lutpointer==NULL) {
//      lutpointer = (int *) calloc(sizeof(int), numWeightPatches);
//      freelutpointer=true;
//      lutpointer[0]=-1;
//   }
//   printf("nxex %lu\n",nxex);
//   printf("nyex %lu\n",nyex);
//   printf("nxl %lu\n",nxl);
//   printf("nyl %lu\n",nyl);
//   printf("nxex/nxl %lu\n",nxex/nxl);
//   printf("nyex/nyl %lu\n",nyex/nyl);
//   for(kx=0;kx<(int)(nxex/nxl);kx++) {
//      for(ky=0;ky<(int)(nyex/nyl);ky++) {
//
//         for(int lidx=0;lidx<(int)nxl;lidx++) {
//            for(int lidy=0;lidy<(int)nyl;lidy++) {
//               HyPerLayer_recv_synaptic_input(kx*nxl+nxl/2, ky*nyl+nyl/2, lidx, lidy, nxl, nyl,
//                     preLoc->nx, preLoc->ny, preLoc->nf, preLoc->nb, nxp, nyp, nfp,
//                     (float)postLoc->nf/(float)preLoc->nf,(float)postLoc->nx/(float)preLoc->nx,(float)postLoc->ny/(float)preLoc->ny,
//                     0, lutpointer, cube->data, get_wDataStart(arborId), gstart, gTempBuf);
//               //free(tempBuf);
//            }
//         }
//
//      }
//   }
//   if(freelutpointer) {free(lutpointer);lutpointer=NULL;}
//   //copy back to G:
////   float xScale = (float)postLoc->nx/(float)preLoc->nx;
////   float yScale = (float)postLoc->ny/(float)preLoc->ny;
////   const int kPostX = (int)(xScale*kx) - (int)(xScale*preLoc->nb); // kPostX==0 is left boundary non-extended
////   const int kPostY = (int)(yScale*ky) - (int)(yScale*preLoc->nb); // kPostY==0 is top  boundary non-extended
////   const int gStride = xScale*preLoc->nx;
////   int tempBufStride=nxl+nxp*nfp;
////
////   const int gx=kPostX-nxp/2;
////   const int gy=kPostY-nyp/2;
////   for(int clidx=0;clidx<nxp;clidx++){
////      for(int clidy=0;clidy<nyp;clidy++){
////         gTempBuf[(gy+clidy)*gStride + gx+clidx]+=tempBuf[clidy*tempBufStride + clidx];
////      }
////   }
//   //free(tempBuf);
//
//   //cl_event   tmpcopybackGevList;         // event list
//   //cl_event   tmpevUpdate;
////   post->getChannelCLBuffer(getChannel())->copyFromDevice(1, &evRecvSyn, &tmpcopybackGevList);
////   status |= clWaitForEvents(1, &tmpcopybackGevList);
////   clReleaseEvent(tmpcopybackGevList);
//   post->copyGSynFromDevice();
//
//#ifdef TODO_CRAIG
////TODO 2014.5.24 - need to figure out type of getGSynPatchStart (see LCALIFLateralConn.cpp for usage)
////               - is it like a weight or activity parameter?
//   //copyChannelExcFromDevice();
//   ptrdiff_t gTempBuf2=getGSynPatchStart(0, arborId);
//
//   int errcnt=0;
//   for (int ix=0;ix<postLoc->nx; ix++) {
//      for (int iy=0;iy<postLoc->ny; iy++) {
//         if (fabs(gTempBuf[iy*postLoc->nx+ix]-gTempBuf2[iy*postLoc->nx+ix])>0.00001) {
//            printf("mismatch! C function version: %f \n",gTempBuf[iy*postLoc->nx+ix]);
//            printf("opencl function version: %f \n",gTempBuf2[iy*postLoc->nx+ix]);
//            printf("at loc x: %d y %d \n",ix, iy);
//            printf("kpre %d \n",ix+preLoc->nb+ (iy+preLoc->nb)*(preLoc->nx*preLoc->nf + 2*preLoc->nb));
//            errcnt++;
//            if (errcnt>10) exit(1);
//         }
////         if(gTempBuf[iy*postLoc->nx+ix]==4){
////            printf("value = 4! lutpointer: %f \n",gTempBuf[iy*postLoc->nx+ix]);
////            printf("opencl function version: %f \n",gTempBuf2[iy*postLoc->nx+ix]);
////            printf("at loc x: %d y %d \n",ix, iy);
////            printf("kpre %d \n",ix+preLoc->nb+ (iy+preLoc->nb)*(preLoc->nx*preLoc->nf + 2*preLoc->nb));
////            errcnt++;
////            if(errcnt>10) exit(1);
////         }
////         if((gTempBuf[iy*postLoc->nx+ix]>25)||(gTempBuf2[iy*postLoc->nx+ix]>25)){
////            printf("not equal to row! C function version: %f \n",gTempBuf[iy*postLoc->nx+ix]);
////            printf("opencl function version: %f \n",gTempBuf2[iy*postLoc->nx+ix]);
////            printf("at loc x: %d y %d \n",ix, iy);
////            errcnt++;
////            if(errcnt>500) exit(1);
////         }
////         if((gTempBuf[iy*postLoc->nx+ix]!=gTempBuf2[iy*postLoc->nx+ix])&&
////               (gTempBuf[iy*postLoc->nx+ix]!=0)){
////            printf("mismatch (2)! C function version: %d \n",gTempBuf[iy*postLoc->nx+ix]);
////            printf("opencl function version: %d \n",gTempBuf2[iy*postLoc->nx+ix]);
////            printf("at loc x: %d y %d \n",ix, iy);
////            errcnt++;
////            if(errcnt>10) exit(1);
////         }
////         if((gTempBuf[iy*postLoc->nx+ix]==gTempBuf2[iy*postLoc->nx+ix])&&
////               (gTempBuf[iy*postLoc->nx+ix]!=0)){
////            printf("nonzero match found! C function version: %d \n",gTempBuf[iy*postLoc->nx+ix]);
////            printf("opencl function version: %d \n",gTempBuf2[iy*postLoc->nx+ix]);
////            printf("at loc x: %d y %d \n",ix, iy);
////            //errcnt++;
////            //if(errcnt>10) exit(1);
////         }
//      }
//   }
//#endif // TODO_CRAIG
//
//   free(gTempBuf);
//
//   return status;
//}
//#endif // PV_USE_OPENCL

int HyPerConn::readStateFromCheckpoint(const char * cpDir, double * timeptr) {
   // If timeptr is NULL, the timestamps in the pvp files are ignored.  If non-null, they are compared to the value of *timeptr and
   // a warning is issued if there is a discrepancy.
   int status = PV_SUCCESS;
   status = readWeightsFromCheckpoint(cpDir, timeptr);
   return status;
}

int HyPerConn::readWeightsFromCheckpoint(const char * cpDir, double * timeptr) {
   clearWeights(get_wDataStart(), getNumDataPatches(), nxp, nyp, nfp);
   char * path = parent->pathInCheckpoint(cpDir, getName(), "_W.pvp");
   InitWeights * weightsInitObject = new InitWeights(this);
   PVPatch *** patches_arg = sharedWeights ? NULL : wPatches;
   double filetime=0.0;
   int status = weightsInitObject->readWeights(patches_arg, get_wDataStart(), getNumDataPatches(), path, &filetime);
   if (parent->columnId()==0 && timeptr && *timeptr != filetime) {
      fprintf(stderr, "Warning: \"%s\" checkpoint has timestamp %g instead of the expected value %g.\n", path, filetime, *timeptr);
   }
   free(path);
   delete weightsInitObject;
   return status;
}

int HyPerConn::checkpointRead(const char * cpDir, double * timeptr) {
   int status = readStateFromCheckpoint(cpDir, timeptr);

   status = parent->readScalarFromFile(cpDir, getName(), "lastUpdateTime", &lastUpdateTime, lastUpdateTime);
   assert(status == PV_SUCCESS);
   status = parent->readScalarFromFile(cpDir, getName(), "weightUpdateTime", &weightUpdateTime, weightUpdateTime);
   assert(status == PV_SUCCESS);
   if (this->plasticityFlag &&  weightUpdateTime<parent->simulationTime()) {
      // simulationTime() may have been changed by HyPerCol::checkpoint, so this repeats the sanity check on weightUpdateTime in allocateDataStructures
      while(weightUpdateTime <= parent->simulationTime()) {weightUpdateTime += weightUpdatePeriod;}
      if (parent->columnId()==0) {
         fprintf(stderr, "Warning: initialWeightUpdateTime of %s \"%s\" less than simulation start time.  Adjusting weightUpdateTime to %f\n",
               parent->parameters()->groupKeywordFromName(name), name, weightUpdateTime);
      }
   }

   status = parent->readScalarFromFile(cpDir, getName(), "nextWrite", &writeTime, writeTime);
   assert(status == PV_SUCCESS);

   return status;
}

int HyPerConn::checkpointWrite(const char * cpDir) {
   char filename[PV_PATH_MAX];
   int status = checkpointFilename(filename, PV_PATH_MAX, cpDir);
   assert(status==PV_SUCCESS);
#ifdef PV_USE_MPI
   if (sharedWeights && plasticityFlag && !keepKernelsSynchronized_flag) {
      for (int arbor_id = 0; arbor_id < this->numberOfAxonalArborLists(); arbor_id++) {
         reduceKernels(arbor_id);
      }
   }
#endif // PV_USE_MPI
   PVPatch *** patches_arg = sharedWeights ? NULL : wPatches;
   status = writeWeights(patches_arg, wDataStart, getNumDataPatches(), filename, parent->simulationTime(), writeCompressedCheckpoints, /*last*/true);
   assert(status==PV_SUCCESS);
   status = parent->writeScalarToFile(cpDir, getName(), "nextWrite", writeTime);
   assert(status==PV_SUCCESS);
   status = parent->writeScalarToFile(cpDir, getName(), "lastUpdateTime", lastUpdateTime);
   assert(status==PV_SUCCESS);
   status = parent->writeScalarToFile(cpDir, getName(), "weightUpdateTime", weightUpdateTime);
   assert(status==PV_SUCCESS);
   return status;
}

int HyPerConn::checkpointFilename(char * cpFilename, int size, const char * cpDir) {
   int chars_needed = snprintf(cpFilename, size, "%s/%s_W.pvp", cpDir, name);
   if(chars_needed >= PV_PATH_MAX) {
      if ( parent->icCommunicator()->commRank()==0 ) {
         fprintf(stderr, "HyPerConn::checkpointFilename error: path \"%s/%s_W.pvp\" is too long.\n", cpDir, name);
      }
      abort();
   }
   return PV_SUCCESS;
}

int HyPerConn::writeTimers(FILE* stream){
   if (parent->icCommunicator()->commRank()==0) {
      io_timer->fprint_time(stream);
      update_timer->fprint_time(stream);
      for (int p=0; p<numProbes; p++){
         probes[p]->writeTimer(stream);
      }
   }
   return PV_SUCCESS;
}

float HyPerConn::minWeight(int arborId)
{
   const int num_data_patches = getNumDataPatches();
   float min_weight = FLT_MAX;
   if (sharedWeights) {
      const int numWeights = nxp * nyp * nfp;
      for (int iKernel = 0; iKernel < num_data_patches; iKernel++) {
         pvwdata_t * kernelWeights = this->get_wDataHead(arborId, iKernel);
         for (int iWeight = 0; iWeight < numWeights; iWeight++) {
            min_weight = (min_weight < kernelWeights[iWeight]) ? min_weight
                  : kernelWeights[iWeight];
         }
      }
   }
   else {
      for (int i_patch = 0; i_patch < num_data_patches; i_patch++) {
         pvwdata_t * w_data = this->get_wData(arborId, i_patch);
         PVPatch * w_patch = this->getWeights(i_patch, arborId);
         int num_weights = this->fPatchSize() * w_patch->nx * w_patch->ny;
         for (int iWeight = 0; iWeight < num_weights; iWeight++) {
            min_weight = (min_weight < w_data[iWeight]) ? min_weight
                  : w_data[iWeight];
         }
      }
   }
   return min_weight;
}

float HyPerConn::maxWeight(int arborId)
{
   const int num_data_patches = getNumDataPatches();
   float max_weight = -FLT_MAX;
   if (sharedWeights) {
      const int numWeights = nxp * nyp * nfp;
      for (int iKernel = 0; iKernel < num_data_patches; iKernel++) {
         pvwdata_t * kernelWeights = this->get_wDataHead(arborId, iKernel);
         for (int iWeight = 0; iWeight < numWeights; iWeight++) {
            max_weight = (max_weight > kernelWeights[iWeight]) ? max_weight
                  : kernelWeights[iWeight];
         }
      }
   }
   else {
      for (int i_weight = 0; i_weight < num_data_patches; i_weight++) {
         pvwdata_t * w_data = this->get_wData(arborId, i_weight);
         PVPatch * w_patch = this->getWeights(i_weight, arborId);
         int num_weights = this->fPatchSize() * w_patch->nx * w_patch->ny;
         for (int iWeight = 0; iWeight < num_weights; iWeight++) {
            max_weight = (max_weight > w_data[iWeight]) ? max_weight
                  : w_data[iWeight];
         }
      }
   }
   return max_weight;
}

int HyPerConn::insertProbe(BaseConnectionProbe * p)
{
   if(p->getTargetConn() != this) {
      fprintf(stderr, "HyPerConn \"%s\": insertProbe called with probe %p, whose targetConn is not this connection.  Probe was not inserted.\n", name, p);
      return numProbes;
   }
   for( int i=0; i<numProbes; i++ ) {
      if( p == probes[i] ) {
         fprintf(stderr, "HyPerConn \"%s\": insertProbe called with probe %p, which has already been inserted as probe %d.\n", name, p, i);
         return numProbes;
      }
   }

   BaseConnectionProbe ** tmp;
   tmp = (BaseConnectionProbe **) malloc((numProbes + 1) * sizeof(BaseConnectionProbe *));
   assert(tmp != NULL);

   for (int i = 0; i < numProbes; i++) {
      tmp[i] = probes[i];
   }
   delete probes;

   probes = tmp;
   probes[numProbes] = p;

   return ++numProbes;
}

int HyPerConn::initializeState() {
   int status = PV_SUCCESS;
   assert(parent->getInitializeFromCheckpointDir()); // should never be null; it should be the empty string if not initializing from a checkpoint
   if (parent->getInitializeFromCheckpointDir()[0] && initializeFromCheckpointFlag) {
      assert(parent->getInitializeFromCheckpointDir() && parent->getInitializeFromCheckpointDir()[0]);
      status = readStateFromCheckpoint(parent->getInitializeFromCheckpointDir(), NULL);
   }
   else {
      //initialize weights for patches:
      status = setInitialValues();
   }
   return status;
}

int HyPerConn::setInitialValues() {
   initializeWeights(wPatches, wDataStart);
   return PV_SUCCESS;
}

int HyPerConn::outputProbeParams() {
   int status = PV_SUCCESS;
   for (int p=0; p<numProbes; p++) {
      int status1 = probes[p]->ioParams(PARAMS_IO_WRITE);
      if (status1 != PV_SUCCESS) { status = PV_FAILURE; }
   }
   return status;
}

int HyPerConn::outputState(double timef, bool last)
{

#ifdef PV_USE_OPENCL
   clFinishW();
#endif

   int status = 0;
   io_timer->start();

   if( !last ) {
      for (int i = 0; i < numProbes; i++) {
         probes[i]->outputStateWrapper(timef, parent->getDeltaTime());
      }
   }

   if (last) {
      status = writeWeights(timef, last);
      assert(status == 0);
   }
   else if ( (writeStep >= 0) && (timef >= writeTime) ) {
      writeTime += writeStep;

      status = writeWeights(timef, last);
      assert(status == 0);

      // append to output file after original open
      ioAppend = true;
   }
   else if (writeStep < 0) { // If writeStep is negative, we never call writeWeights, but someone might restart from a checkpoint with a different writeStep, so we should still maintain writeTime
      writeTime = timef;
   }

   io_timer->stop();
   return status;
}

bool HyPerConn::needUpdate(double time, double dt){
   if( !plasticityFlag ) {
      return false;
   }
   if(triggerFlag){
      assert(triggerLayer);
      double nextUpdateTime = triggerLayer->getNextUpdateTime();
      //never update flag
      if(nextUpdateTime == -1){
         return false;
      }
      //Check for equality
      if(fabs(time - (nextUpdateTime - triggerOffset)) < (dt/2)){
         return true;
      }
      //If it gets to this point, don't update
      return false;
   }
   //If no trigger, use weightUpdateTime
   else{
      if( time >= weightUpdateTime) {
         return true;
      }
      return false;
   }
}


int HyPerConn::updateStateWrapper(double time, double dt){
   int status = PV_SUCCESS;
   if(needUpdate(time, dt)){
      //Need to finish command queue of pre and post activity
      //Doing both in case of multiple gpus running
#ifdef PV_USE_OPENCL
      pre->clFinishActivity();
      post->clFinishActivity();
#endif

      double preTimeScale = pre->getTimeScale(); 
      double postTimeScale = post->getTimeScale();
      double colTimeScale = parent->getTimeScale();
      double timeScaleMin = parent->getTimeScaleMin();
      //If timeScale is less than the value for dtScaleMin specified in the params but not -1, don't updateState.
      //This is implemented as an optimization so weights don't change dramatically as ANNNormalizedErrorLayer values get large.
      if (preTimeScale > 0 && preTimeScale < timeScaleMin) { 
         if (parent->icCommunicator()->commRank()==0) {
            fprintf(stdout, "TimeScale = %f for layer %s, which is less than your specified dtScaleMin, %f. updateState won't be called for connection \"%s\" this timestep.\n", preTimeScale, pre->getName(), timeScaleMin, getName());
         }
      }
      else if (postTimeScale > 0 && postTimeScale < timeScaleMin) { 
         if (parent->icCommunicator()->commRank()==0) {
            fprintf(stdout, "TimeScale = %f for layer %s, which is less than your specified dtScaleMin, %f. updateState won't be called for connection \"%s\" this timestep.\n", postTimeScale, post->getName(),  timeScaleMin, getName());
         }
      }
      else if (colTimeScale > 0 && colTimeScale < timeScaleMin) { 
         if (parent->icCommunicator()->commRank()==0) {
            fprintf(stdout, "TimeScale = %f for column %s, which is less than your specified dtScaleMin, %f. updateState won't be called for connection \"%s\" this timestep.\n", colTimeScale, parent->getName(),  timeScaleMin, getName());
         }
      }
     else {
#if defined(PV_USE_OPENCL) || defined(PV_USE_CUDA)
         updatedDeviceWeights = true;
#endif
         status = updateState(time, dt);
         //Update lastUpdateTime
         lastUpdateTime = time;
     }
      computeNewWeightUpdateTime(time, weightUpdateTime);

      //Sanity check, take this out once convinced layer's nextUpdateTime is the same as weightUpdateTime
      //No way to make this assertion, cause nextUpdateTime/weightUpdateTime updates happen at different times
      //if(triggerFlag){
      //   if(weightUpdateTime != triggerLayer->getNextUpdateTime()){
      //      std::cout << "Layer " << name << ": weightUpdateTime (" << weightUpdateTime << ") and layer's getNextUpdateTime (" << triggerLayer->getNextUpdateTime() << ") mismatch\n";
      //   }
      //   //assert(weightUpdateTime == triggerLayer->getNextUpdateTime());
      //}
   }
   return status;
}

int HyPerConn::updateState(double time, double dt)
{
   int status = PV_SUCCESS;
   if( !plasticityFlag ) {
      return status;
   }
   update_timer->start();

   clear_dW();
   for(int arborId=0;arborId<numberOfAxonalArborLists();arborId++) {
      status = calc_dW(arborId);        // Calculate changes in weights
      if (status==PV_BREAK) { break; }
      assert(status == PV_SUCCESS);
   }

#ifdef PV_USE_MPI
   bool needSynchronizing = keepKernelsSynchronized_flag;
   needSynchronizing |= sharedWeights && (parent->simulationTime() >= parent->getStopTime()-parent->getDeltaTime());
   if (needSynchronizing) {
      for (int arborID = 0; arborID < numberOfAxonalArborLists(); arborID++) {
         status = reduceKernels(arborID); // combine partial changes in each column
         if (status == PV_BREAK) {
            break;
         }
         assert(status == PV_SUCCESS);
      }
   }
#endif // PV_USE_MPI

   for(int arborId=0;arborId<numberOfAxonalArborLists();arborId++){
      status = updateWeights(arborId);  // Apply changes in weights
      if (status==PV_BREAK) { break; }
      assert(status==PV_SUCCESS);
   }
   normalizeWeights();

   update_timer->stop();
   return status;
}

int HyPerConn::calc_dW(int arborId) {
   assert(plasticityFlag);
   return update_dW(arborId);
}

int HyPerConn::clear_dW() {
   // zero out all dW.
   // This also zeroes out the unused parts of shrunken patches
   for(int kArbor = 0; kArbor < numberOfAxonalArborLists(); kArbor++){
      for(int kKernel = 0; kKernel < getNumDataPatches(); kKernel++){
         int syPatch = syp;
         int nkPatch = nfp * nxp;
         pvwdata_t * dWeights = get_dwDataHead(kArbor,kKernel);
         for(int kyPatch = 0; kyPatch < nyp; kyPatch++){
            for(int kPatch = 0; kPatch < nkPatch; kPatch++){
               dWeights[kPatch] = 0.0f;
            }
            dWeights += syPatch;
         }
      }
   }
   return PV_BREAK;
}

int HyPerConn::update_dW(int arbor_ID) {
   // Typically override this method with a call to defaultUpdate_dW(arbor_ID)
   int status = defaultUpdate_dW(arbor_ID);  // calculate new weights from changes
   return status;
}

int HyPerConn::defaultUpdate_dW(int arbor_ID) {
   // compute dW but don't add them to the weights yet.
   // That takes place in reduceKernels, so that the output is
   // independent of the number of processors.
   int nExt = preSynapticLayer()->getNumExtended();

   if (sharedWeights) {
      //Reset numKernelActivations
      int numKernelIndices = getNumDataPatches();
      for(int ki = 0; ki < numKernelIndices; ki++){
         numKernelActivations[ki] = 0;
      }
   }
   for(int kExt=0; kExt<nExt;kExt++) {
      defaultUpdateInd_dW(arbor_ID, kExt);
   }
   //If update from clones, update dw here as well
   //Updates on all PlasticClones
   for(int clonei = 0; clonei < clones.size(); clonei++){
      assert(clones[clonei]->preSynapticLayer()->getNumExtended() == nExt);
      for(int kExt=0; kExt<nExt;kExt++) {
         clones[clonei]->defaultUpdateInd_dW(arbor_ID, kExt);
      }
   }

   normalize_dW(arbor_ID);

   return PV_SUCCESS;
}

int HyPerConn::defaultUpdateInd_dW(int arbor_ID, int kExt){
   const pvdata_t * preactbuf = preSynapticLayer()->getLayerData(getDelay(arbor_ID));
   const pvdata_t * postactbuf = postSynapticLayer()->getLayerData();
   const PVLayerLoc * preLoc = pre->getLayerLoc();
   const PVLayerLoc * postLoc = post->getLayerLoc();
   int sya = (post->getLayerLoc()->nf * (post->getLayerLoc()->nx + post->getLayerLoc()->halo.lt + post->getLayerLoc()->halo.rt));

   pvdata_t preact = preactbuf[kExt];
   if (skipPre(preact)) return PV_CONTINUE;

   if (sharedWeights) {
      //update numKernelActivations
      int kernelIndex = patchIndexToDataIndex(kExt);
      //Only increment if kernelIndex is restricted
      int nxExt = preLoc->nx + preLoc->halo.lt + preLoc->halo.rt;
      int nyExt = preLoc->ny + preLoc->halo.dn + preLoc->halo.up;
      int nf = preLoc->nf;
      int extX = kxPos(kExt, nxExt, nyExt, nf);
      int extY = kyPos(kExt, nxExt, nyExt, nf);
      if(extX >= preLoc->halo.lt && extX < preLoc->nx + preLoc->halo.lt &&
            extY >= preLoc->halo.up && extY < preLoc->ny + preLoc->halo.up){
         numKernelActivations[kernelIndex]++;
      }
   }

   bool inWindow = true;
   // only check inWindow if number of arbors > 1
   if (this->numberOfAxonalArborLists()>1){
      if(useWindowPost){
         int kPost = layerIndexExt(kExt, preLoc, postLoc);
         inWindow = post->inWindowExt(arbor_ID, kPost);
      }
      else{
         inWindow = pre->inWindowExt(arbor_ID, kExt);
      }
      if(!inWindow) return PV_CONTINUE;
   }
   PVPatch * weights = getWeights(kExt,arbor_ID);
   size_t offset = getAPostOffset(kExt, arbor_ID);
   int ny = weights->ny;
   int nk = weights->nx * nfp;
   const pvdata_t * postactRef = &postactbuf[offset];
   pvwdata_t * dwdata = get_dwData(arbor_ID, kExt);
   int lineoffsetw = 0;
   int lineoffseta = 0;
   for( int y=0; y<ny; y++ ) {
      for( int k=0; k<nk; k++ ) {
         pvdata_t aPost = postactRef[lineoffseta+k];
         dwdata[lineoffsetw + k] += updateRule_dW(preact, aPost);
      }
      lineoffsetw += syp;
      lineoffseta += sya;
   }


   return PV_SUCCESS;
}

void HyPerConn::addClone(PlasticCloneConn* conn){
   //Make sure that the origional conn is indeed this
   assert(conn->getOriginalConn() == this);
   clones.push_back(conn);
}

void HyPerConn::reduceNumKernelActivations(){
   if(sharedWeights){
      //Do mpi to update numKernelActivationss
#ifdef PV_USE_MPI
      int numKernelIndices = getNumDataPatches();
      int ierr = MPI_Allreduce(MPI_IN_PLACE, numKernelActivations , numKernelIndices, MPI_INT, MPI_SUM, parent->icCommunicator()->communicator());
      for( int kernelindex=0; kernelindex<numKernelIndices; kernelindex++ ) {
         numKernelActivations[kernelindex] /= parent->icCommunicator()->commSize();
      }
#endif
   }
}

int HyPerConn::normalize_dW(int arbor_ID){
   if (sharedWeights) {
      reduceNumKernelActivations();
      int numKernelIndices = getNumDataPatches();
      // Divide by numKernelActivations in this timestep
      for( int kernelindex=0; kernelindex<numKernelIndices; kernelindex++ ) {
         //Calculate pre feature index from patch index
         //TODO right now it's dividing the divisor by nprocs. This is a hack. Proper fix is to update all connections overwriting
         //update_dW to do dwNormalization in this way and take out the divide by nproc in reduceKernels.
         const int nProcs = parent->icCommunicator()->numCommColumns() * parent->icCommunicator()->numCommRows();
         double divisor = numKernelActivations[kernelindex];
         for(int i = 0; i < clones.size(); i++){
            clones[i]->reduceNumKernelActivations();
            divisor += clones[i]->getNumKernelActivations(kernelindex);
         }

         if(divisor != 0){
            int numpatchitems = nxp*nyp*nfp;
            pvwdata_t * dwpatchdata = get_dwDataHead(arbor_ID,kernelindex);
            for( int n=0; n<numpatchitems; n++ ) {
               dwpatchdata[n] /= divisor;
            }
         }
      }
   }
   return PV_SUCCESS;
}

pvdata_t HyPerConn::updateRule_dW(pvdata_t pre, pvdata_t post) {
   return dWMax * pre * post;
}

#ifdef PV_USE_MPI
int HyPerConn::reduceKernels(const int arborID) {
   assert(sharedWeights && plasticityFlag && mpiReductionBuffer);
   Communicator * comm = parent->icCommunicator();
   const MPI_Comm mpi_comm = comm->communicator();
   int ierr;
   const int nxProcs = comm->numCommColumns();
   const int nyProcs = comm->numCommRows();
   const int nProcs = nxProcs * nyProcs;
   if (nProcs == 1){
      return PV_BREAK;
   }
   const int numPatches = getNumDataPatches();
   const size_t patchSize = nxp*nyp*nfp;
   const size_t localSize = numPatches * patchSize;
   const size_t arborSize = localSize * this->numberOfAxonalArborLists();

#ifdef PV_USE_MPI
   ierr = MPI_Allreduce(MPI_IN_PLACE, this->get_dwDataStart(0), arborSize, MPI_FLOAT, MPI_SUM, mpi_comm);
#endif
   pvwdata_t * dW_data = this->get_dwDataStart(0);
   for (int i_dW = 0; i_dW < arborSize; i_dW++){
       dW_data[i_dW] /= nProcs;
   }

   return PV_BREAK;
}
#endif // PV_USE_MPI

int HyPerConn::updateWeights(int arborId)
{
   lastUpdateTime = parent->simulationTime();
   // add dw to w
   // never use shmget if plasticity flag == true
   for(int kArbor = 0; kArbor < this->numberOfAxonalArborLists(); kArbor++){
      pvwdata_t * w_data_start = get_wDataStart(kArbor);
      for( int k=0; k<nxp*nyp*nfp*getNumDataPatches(); k++ ) {
         w_data_start[k] += get_dwDataStart(kArbor)[k];
      }
   }
   return PV_BREAK;
}

double HyPerConn::computeNewWeightUpdateTime(double time, double currentUpdateTime) {
   //Only called if placisity flag is set
   while(time >= weightUpdateTime){
      weightUpdateTime += weightUpdatePeriod;
   }
   return weightUpdateTime;
}

PVPatch * HyPerConn::getWeights(int k, int arbor)
{
   // a separate arbor/patch of weights for every neuron
   return wPatches[arbor][k];
}

int HyPerConn::createWeights(PVPatch *** patches, int nWeightPatches, int nDataPatches, int nxPatch,
      int nyPatch, int nfPatch, int arborId)
{
   // could create only a single patch with following call
   //   return createPatches(numAxonalArborLists, nxp, nyp, nfp);

   //assert(numAxonalArborLists == 1);
   assert(patches[arborId] == NULL);

   //patches = (PVPatch**) calloc(sizeof(PVPatch*), nPatches);
   //patches[arborId] = (PVPatch**) calloc(nPatches, sizeof(PVPatch*));
   if (shrinkPatches_flag || arborId == 0){
      patches[arborId] = createPatches(nWeightPatches, nxPatch, nyPatch);
      assert(patches[arborId] != NULL);
   }
   else{
      patches[arborId] = patches[0];
   }

   // allocate space for all weights at once (inplace), return pointer to beginning of weight array
   //pvdata_t * data_patches = allocWeights(patches, nDataPatches, nxPatch, nyPatch, nfPatch, arborId);
   return PV_SUCCESS;
}

/**
 * Create a separate patch of weights for every neuron
 */
int HyPerConn::createWeights(PVPatch *** patches, int arborId)
{
   int nWeightPatches = getNumWeightPatches();
   int nDataPatches = getNumDataPatches();
   int nxPatch = nxp;
   int nyPatch = nyp;
   int nfPatch = nfp;

   int status = createWeights(patches, nWeightPatches, nDataPatches, nxPatch, nyPatch, nfPatch, arborId);
   return status;
}

int HyPerConn::clearWeights(pvwdata_t ** dataStart, int numPatches, int nxp, int nyp, int nfp) {
   int status = PV_SUCCESS;
   for( int arborID = 0; arborID<numAxonalArborLists; arborID++ ) {
      if( clearWeights(dataStart[arborID], numPatches, nxp, nyp, nfp)!=PV_SUCCESS ) status = PV_FAILURE;
   }
   return status;
}

int HyPerConn::clearWeights(pvwdata_t * arborDataStart, int numPatches, int nxp, int nyp, int nfp) {
   for( int w=0; w<numPatches*nxp*nyp*nfp; w++ ) {
      arborDataStart[w] = 0.0f;
   }
   return PV_SUCCESS;
}

int HyPerConn::deleteWeights() {
	// to be used if createPatches is used above
	// HyPerConn::deletePatches(numAxonalArborLists, wPatches);
	if (wPatches != NULL) {
		for (int arbor = 0; arbor < numAxonalArborLists; arbor++) {
			if (wPatches[arbor] != NULL) {
				if (shrinkPatches_flag || arbor == 0) {
					deletePatches(wPatches[arbor]);
				}
				wPatches[arbor] = NULL;
			}
		}  // arbor
		free(wPatches);
		wPatches = NULL;
	} // wPatches != NULL

	if (wDataStart != NULL) {
		for (int arbor = 0; arbor < numAxonalArborLists; arbor++) {
			// entire arbor allocated as single block
			if (arbor == 0) {
#ifdef USE_SHMGET
				if (!shmget_flag) {
					if (wDataStart[arbor] != NULL) {
						free(this->wDataStart[arbor]);
					}
				} else {
					if (wDataStart[arbor] != NULL) {
						int shmget_status = shmdt(this->get_wDataStart(arbor));
						assert(shmget_status==0);
						if (shmget_owner[arbor]) {
							shmid_ds * shmget_ds = NULL;
							shmget_status = shmctl(shmget_id[arbor], IPC_RMID,
									shmget_ds);
							assert(shmget_status==0);
						}
					}
				}
#else
				if (wDataStart[arbor] != NULL) {
					free(this->wDataStart[arbor]);
				}
#endif // USE_SHMGET
			} // arbor == 0
			this->wDataStart[arbor] = NULL;
			if (!this->combine_dW_with_W_flag) {
				if (dwDataStart != NULL && dwDataStart[arbor] != NULL) {
					free(dwDataStart[arbor]);
					dwDataStart[arbor] = NULL;
				}
			}
		}  // arbor
		free(wDataStart);
		wDataStart = NULL;
		if (!this->combine_dW_with_W_flag) {
			free(dwDataStart);
		}
		dwDataStart = NULL;
	} // wDataStart != NULL

#ifdef USE_SHMGET
	if (shmget_flag) {
		free(shmget_id);
		free(shmget_owner);
	}
#endif

	if (wPostPatches != NULL) {
		for (int arborID = 0; arborID < numberOfAxonalArborLists(); arborID++) {
			if (wPostPatches[arborID] != NULL) {
				if (shrinkPatches_flag || arborID == 0) {
					deletePatches(wPostPatches[arborID]);
				}
				wPostPatches[arborID] = NULL;
			}

			if (wPostDataStart != NULL) {
				free(this->wPostDataStart[arborID]);
				this->wPostDataStart[arborID] = NULL;
			}
		}
		free(wPostPatches);
		wPostPatches = NULL;
		free(wPostDataStart);
		wPostDataStart = NULL;
	}  // wPostPatches != NULL

	if (gSynPatchStart != NULL) {
		free(gSynPatchStart[0]); // All gSynPatchStart[k]'s were allocated together in a single malloc call.
		free(gSynPatchStart);
	}
	if (aPostOffset != NULL) {
		free(aPostOffset[0]); // All aPostOffset[k]'s were allocated together in a single malloc call.
		free(aPostOffset);
	}
	free(patch2datalookuptable); patch2datalookuptable = NULL;

	return PV_SUCCESS;
}


//This function is doing what adjust axonal arbors was doing before, but generalized to not use the pre/post layer's pre/post for use with gpu post groups
int HyPerConn::adjustAllPatches(
      int nxPre, int nyPre, int nfPre, const PVHalo * haloPre,
      int nxPost, int nyPost, int nfPost, const PVHalo * haloPost,
      PVPatch*** inWPatches,
      size_t** inGSynPatchStart,
      size_t** inAPostOffset,
      int arborId){

   const int layerNxPre = pre->getLayerLoc()->nx;
   const int layerNyPre = pre->getLayerLoc()->ny;
   const int layerNxPost = post->getLayerLoc()->nx;
   const int layerNyPost = post->getLayerLoc()->ny;

   const int xPostNeuronsPerPreNeuron = layerNxPre < layerNxPost ? layerNxPost/layerNxPre : 1;
   assert(layerNxPre>=layerNxPost || layerNxPre*xPostNeuronsPerPreNeuron==layerNxPost);
   const int xPreNeuronsPerPostNeuron = layerNxPre > layerNxPost ? layerNxPre/layerNxPost : 1;
   assert(layerNxPre<=layerNxPost || layerNxPost*xPreNeuronsPerPostNeuron==layerNxPre);
   const int yPostNeuronsPerPreNeuron = layerNyPre < layerNyPost ? layerNyPost/layerNyPre : 1;
   assert(layerNyPre>=layerNyPost || layerNyPre*yPostNeuronsPerPreNeuron==layerNyPost);
   const int yPreNeuronsPerPostNeuron = layerNyPre > layerNyPost ? layerNyPre/layerNyPost : 1;
   assert(layerNyPre<=layerNyPost || layerNyPost*yPreNeuronsPerPostNeuron==layerNyPre);

   int xPatchHead = (nxp-nxpShrunken)/2;
   assert(2*xPatchHead == nxp-nxpShrunken);
   int yPatchHead = (nyp-nypShrunken)/2;
   assert(2*yPatchHead == nyp-nypShrunken);
   offsetShrunken = kIndex(xPatchHead, yPatchHead, 0, nxp, nyp, nfp);

   int numWeightPatches = (nxPre + haloPre->lt + haloPre->rt) * (nyPre+haloPre->up + haloPre->dn) * nfPre; 
   for (int kex=0; kex<numWeightPatches; kex++) {
   //for (int kex=0; kex<getNumWeightPatches(); kex++) {
      // calculate xPostStart, xPostStop, xPatchStart, xPatchStop
      int xHalfLength = (nxpShrunken-xPostNeuronsPerPreNeuron)/2;
      assert(2*xHalfLength+xPostNeuronsPerPreNeuron==nxpShrunken);
      int xPre = kxPos(kex, nxPre+haloPre->lt+haloPre->rt, nyPre+haloPre->dn+haloPre->up, nfPre)-haloPre->lt; // x-coordinate of presynaptic neuron tied to patch kex, in restricted coordinates.
      // xCellStartInPostCoords will be the x-coordinate of the first neuron in the unit cell pre-synaptic site xPre,
      // in postsynaptic restricted coordinates (i.e. leftmost restricted neuron is at x=0; rightmost is at x=post->getLayerLoc()->nx - 1.
      // For a 1-1 connection, this is the same as xPre, but for 1-many or many-1 connections, we have to multiply or divide by "many".
      int xCellStartInPostCoords = xPre;
      if (xPostNeuronsPerPreNeuron>1) {
         xCellStartInPostCoords *= xPostNeuronsPerPreNeuron;
      }
      else if (xPreNeuronsPerPostNeuron>1) {
         // For a many-to-one connection, need to divide by "many", and discard the remainder,
         // but in the left boundary region xPre is negative, so xPre/xPreNeuronsPerPostNeuron is not what we want.
         if (xCellStartInPostCoords>=0) {
            xCellStartInPostCoords /= xPreNeuronsPerPostNeuron;
         }
         else {
            xCellStartInPostCoords = -(-xCellStartInPostCoords-1)/xPreNeuronsPerPostNeuron - 1;
         }
      }
      int xPostStart = xCellStartInPostCoords - xHalfLength;
      int xPostStop = xPostStart + nxpShrunken;
      int xPatchStart = xPatchHead;
      int xPatchStop = xPatchStart + nxpShrunken;

      if (xPostStart < 0) {
         int shrinkamount = -xPostStart;
         xPatchStart += shrinkamount;
         xPostStart = 0;
      }
      if (xPostStart > nxPost) { // This can happen if the pre-layer's boundary region is big and the patch size is small
         int shrinkamount = xPostStart - nxPost;
         xPatchStart -= shrinkamount;
         xPostStart = nxPost;
      }
      if (xPostStop > nxPost) {
         int shrinkamount = xPostStop - nxPost;
         xPatchStop -= shrinkamount;
         xPostStop = nxPost;
      }
      if (xPostStop < 0) {
         int shrinkamount = -xPostStop;
         xPatchStop += shrinkamount;
         xPostStop = 0;
      }
      if (xPatchStart < 0) {
         assert(xPatchStart==xPatchStop);
         xPatchStart = 0;
         xPatchStop = 0;
      }
      if (xPatchStop > (nxp+nxpShrunken)/2) {
         assert(xPatchStart==xPatchStop);
         xPatchStop = (nxp+nxpShrunken)/2;
         xPatchStart = xPatchStop;
      }
      assert(xPostStop-xPostStart==xPatchStop-xPatchStart);

      int nx = xPatchStop - xPatchStart;
      assert(nx>=0 && nx<=nxpShrunken);
      assert(xPatchStart>=0 && (xPatchStart<nxp || (nx==0 && xPatchStart==nxp)));

      // calculate yPostStart, yPostStop, yPatchStart, yPatchStop
      int yHalfLength = (nypShrunken-yPostNeuronsPerPreNeuron)/2;
      assert(2*yHalfLength+yPostNeuronsPerPreNeuron==nypShrunken);
      int yPre = kyPos(kex, nxPre+haloPre->lt+haloPre->rt, nyPre+haloPre->dn+haloPre->up, nfPre)-haloPre->up;
      int yCellStartInPostCoords = yPre;
      if (yPostNeuronsPerPreNeuron>1) {
         yCellStartInPostCoords *= yPostNeuronsPerPreNeuron;
      }
      else if (yPreNeuronsPerPostNeuron>1) {
         // For a many-to-one connection, need to divide by "many", and discard the remainder,
         // but in the top boundary region yPre is negative, so yPre/yPreNeuronsPerPostNeuron is not what we want.
         if (yCellStartInPostCoords>=0) {
            yCellStartInPostCoords /= yPreNeuronsPerPostNeuron;
         }
         else {
            yCellStartInPostCoords = -(-yCellStartInPostCoords-1)/yPreNeuronsPerPostNeuron - 1;
         }
      }
      int yPostStart = yCellStartInPostCoords - yHalfLength;
      int yPostStop = yPostStart + nypShrunken;
      int yPatchStart = yPatchHead;
      int yPatchStop = yPatchStart + nypShrunken;

      if (yPostStart < 0) {
         int shrinkamount = -yPostStart;
         yPatchStart += shrinkamount;
         yPostStart = 0;
      }
      if (yPostStart > nyPost) { // This can happen if the pre-layer's boundary region is big and the patch size is small
         int shrinkamount = yPostStart - nyPost;
         yPatchStart -= shrinkamount;
         yPostStart = nyPost;
      }
      if (yPostStop > nyPost) {
         int shrinkamount = yPostStop - nyPost;
         yPatchStop -= shrinkamount;
         yPostStop = nyPost;
      }
      if (yPostStop < 0) {
         int shrinkamount = -yPostStop;
         yPatchStop += shrinkamount;
         yPostStop = 0;
      }
      if (yPatchStart < 0) {
         assert(yPatchStart==yPatchStop);
         yPatchStart = 0;
         yPatchStop = 0;
      }
      if (yPatchStop > (nyp+nypShrunken)/2) {
         assert(yPatchStart==yPatchStop);
         yPatchStop = (nyp+nypShrunken)/2;
         yPatchStart = yPatchStop;
      }
      assert(yPostStop-yPostStart==yPatchStop-yPatchStart);

      int ny = yPatchStop - yPatchStart;
      assert(ny>=0 && ny<=nypShrunken);
      assert(yPatchStart>=0 && (yPatchStart<nyp || (ny==0 && yPatchStart==nyp)));

      if(inAPostOffset){
         inAPostOffset[arborId][kex] = (size_t) kIndex(xPostStart+haloPost->lt,yPostStart+haloPost->up,0,nxPost+haloPost->lt+haloPost->rt,nyPost+haloPost->dn+haloPost->up,nfPost);
      }

      inGSynPatchStart[arborId][kex] = (size_t) kIndex(xPostStart,yPostStart,0,nxPost,nyPost,nfPost);

      PVPatch * w = inWPatches[arborId][kex];
      assert(w->offset==0);
      pvpatch_adjust(w, sxp, syp, nx, ny, xPatchStart, yPatchStart);
   } // loop over patches

   return PV_SUCCESS;
}

//!
/*!
 *
 *      - Each neuron in the pre-synaptic layer projects a number of axonal
 *      arbors to the post-synaptic layer (Can they be projected accross columns too?).
 *      - numAxons is the number of axonal arbors projected by each neuron.
 *      - Each axonal arbor (PVAxonalArbor) connects to a patch of neurons in the post-synaptic layer.
 *      - The PVAxonalArbor structure contains STDP P variable.
 *      -
 *      .
 *
 * REMARKS:
 *      - numArbors = (nxPre + 2*prePad)*(nyPre+2*prePad) = nxexPre * nyexPre
 *      This is the total number of weight patches for a given arbor.
 *      Is the number of pre-synaptic neurons including margins.
 *      - activity and STDP M variable are extended into margins
 *      .
 *
 */
int HyPerConn::adjustAxonalArbors(int arborId)
{

   const int nxPre = pre->getLayerLoc()->nx;
   const int nyPre = pre->getLayerLoc()->ny;
   const int nfPre = pre->getLayerLoc()->nf;
   const PVHalo * haloPre = &pre->getLayerLoc()->halo;
   const int nxPost = post->getLayerLoc()->nx;
   const int nyPost = post->getLayerLoc()->ny;
   const int nfPost = post->getLayerLoc()->nf;
   const PVHalo * haloPost = &post->getLayerLoc()->halo;

   return adjustAllPatches(nxPre, nyPre, nfPre, haloPre, nxPost, nyPost, nfPost, haloPost, wPatches, gSynPatchStart, aPostOffset, arborId);
   
   //This function is moved to adjustAllPatches
   // activity is extended into margins
   //
   // Sets patches' offsets, ny, nx based on shrinking due to edge of the layer, and n{x,y}pShrunken
   // Doesn't do the shrinking turned on or off by the shrinkPatches parameter; that's handled by shrinkPatches().

   //const int nxPre = pre->getLayerLoc()->nx;
   //const int nyPre = pre->getLayerLoc()->ny;
   //const int nfPre = pre->getLayerLoc()->nf;
   //const int nbPre = pre->getLayerLoc()->nb;
   //const int nxPost = post->getLayerLoc()->nx;
   //const int nyPost = post->getLayerLoc()->ny;
   //const int nfPost = post->getLayerLoc()->nf;
   //const int nbPost = post->getLayerLoc()->nb;

   //const int xPostNeuronsPerPreNeuron = nxPre < nxPost ? nxPost/nxPre : 1;
   //assert(nxPre>=nxPost || nxPre*xPostNeuronsPerPreNeuron==nxPost);
   //const int xPreNeuronsPerPostNeuron = nxPre > nxPost ? nxPre/nxPost : 1;
   //assert(nxPre<=nxPost || nxPost*xPreNeuronsPerPostNeuron==nxPre);
   //const int yPostNeuronsPerPreNeuron = nyPre < nyPost ? nyPost/nyPre : 1;
   //assert(nyPre>=nyPost || nyPre*yPostNeuronsPerPreNeuron==nyPost);
   //const int yPreNeuronsPerPostNeuron = nyPre > nyPost ? nyPre/nyPost : 1;
   //assert(nyPre<=nyPost || nyPost*yPreNeuronsPerPostNeuron==nyPre);

   //int xPatchHead = (nxp-nxpShrunken)/2;
   //assert(2*xPatchHead == nxp-nxpShrunken);
   //int yPatchHead = (nyp-nypShrunken)/2;
   //assert(2*yPatchHead == nyp-nypShrunken);
   //offsetShrunken = kIndex(xPatchHead, yPatchHead, 0, nxp, nyp, nfp);

   //for (int kex=0; kex<getNumWeightPatches(); kex++) {
   //   // calculate xPostStart, xPostStop, xPatchStart, xPatchStop
   //   int xHalfLength = (nxpShrunken-xPostNeuronsPerPreNeuron)/2;
   //   assert(2*xHalfLength+xPostNeuronsPerPreNeuron==nxpShrunken);
   //   int xPre = kxPos(kex, nxPre+2*nbPre, nyPre+2*nbPre, nfPre)-nbPre; // x-coordinate of presynaptic neuron tied to patch kex, in restricted coordinates.
   //   // xCellStartInPostCoords will be the x-coordinate of the first neuron in the unit cell pre-synaptic site xPre,
   //   // in postsynaptic restricted coordinates (i.e. leftmost restricted neuron is at x=0; rightmost is at x=post->getLayerLoc()->nx - 1.
   //   // For a 1-1 connection, this is the same as xPre, but for 1-many or many-1 connections, we have to multiply or divide by "many".
   //   int xCellStartInPostCoords = xPre;
   //   if (xPostNeuronsPerPreNeuron>1) {
   //      xCellStartInPostCoords *= xPostNeuronsPerPreNeuron;
   //   }
   //   else if (xPreNeuronsPerPostNeuron>1) {
   //      // For a many-to-one connection, need to divide by "many", and discard the remainder,
   //      // but in the left boundary region xPre is negative, so xPre/xPreNeuronsPerPostNeuron is not what we want.
   //      if (xCellStartInPostCoords>=0) {
   //         xCellStartInPostCoords /= xPreNeuronsPerPostNeuron;
   //      }
   //      else {
   //         xCellStartInPostCoords = -(-xCellStartInPostCoords-1)/xPreNeuronsPerPostNeuron - 1;
   //      }
   //   }
   //   int xPostStart = xCellStartInPostCoords - xHalfLength;
   //   int xPostStop = xPostStart + nxpShrunken;
   //   int xPatchStart = xPatchHead;
   //   int xPatchStop = xPatchStart + nxpShrunken;

   //   if (xPostStart < 0) {
   //      int shrinkamount = -xPostStart;
   //      xPatchStart += shrinkamount;
   //      xPostStart = 0;
   //   }
   //   if (xPostStart > nxPost) { // This can happen if the pre-layer's boundary region is big and the patch size is small
   //      int shrinkamount = xPostStart - nxPost;
   //      xPatchStart -= shrinkamount;
   //      xPostStart = nxPost;
   //   }
   //   if (xPostStop > nxPost) {
   //      int shrinkamount = xPostStop - nxPost;
   //      xPatchStop -= shrinkamount;
   //      xPostStop = nxPost;
   //   }
   //   if (xPostStop < 0) {
   //      int shrinkamount = -xPostStop;
   //      xPatchStop += shrinkamount;
   //      xPostStop = 0;
   //   }
   //   if (xPatchStart < 0) {
   //      assert(xPatchStart==xPatchStop);
   //      xPatchStart = 0;
   //      xPatchStop = 0;
   //   }
   //   if (xPatchStop > (nxp+nxpShrunken)/2) {
   //      assert(xPatchStart==xPatchStop);
   //      xPatchStop = (nxp+nxpShrunken)/2;
   //      xPatchStart = xPatchStop;
   //   }
   //   assert(xPostStop-xPostStart==xPatchStop-xPatchStart);

   //   int nx = xPatchStop - xPatchStart;
   //   assert(nx>=0 && nx<=nxpShrunken);
   //   assert(xPatchStart>=0 && (xPatchStart<nxp || (nx==0 && xPatchStart==nxp)));

   //   // calculate yPostStart, yPostStop, yPatchStart, yPatchStop
   //   int yHalfLength = (nypShrunken-yPostNeuronsPerPreNeuron)/2;
   //   assert(2*yHalfLength+yPostNeuronsPerPreNeuron==nypShrunken);
   //   int yPre = kyPos(kex, nxPre+2*nbPre, nyPre+2*nbPre, nfPre)-nbPre;
   //   int yCellStartInPostCoords = yPre;
   //   if (yPostNeuronsPerPreNeuron>1) {
   //      yCellStartInPostCoords *= yPostNeuronsPerPreNeuron;
   //   }
   //   else if (yPreNeuronsPerPostNeuron>1) {
   //      // For a many-to-one connection, need to divide by "many", and discard the remainder,
   //      // but in the top boundary region yPre is negative, so yPre/yPreNeuronsPerPostNeuron is not what we want.
   //      if (yCellStartInPostCoords>=0) {
   //         yCellStartInPostCoords /= yPreNeuronsPerPostNeuron;
   //      }
   //      else {
   //         yCellStartInPostCoords = -(-yCellStartInPostCoords-1)/yPreNeuronsPerPostNeuron - 1;
   //      }
   //   }
   //   int yPostStart = yCellStartInPostCoords - yHalfLength;
   //   int yPostStop = yPostStart + nypShrunken;
   //   int yPatchStart = yPatchHead;
   //   int yPatchStop = yPatchStart + nypShrunken;

   //   if (yPostStart < 0) {
   //      int shrinkamount = -yPostStart;
   //      yPatchStart += shrinkamount;
   //      yPostStart = 0;
   //   }
   //   if (yPostStart > nyPost) { // This can happen if the pre-layer's boundary region is big and the patch size is small
   //      int shrinkamount = yPostStart - nyPost;
   //      yPatchStart -= shrinkamount;
   //      yPostStart = nyPost;
   //   }
   //   if (yPostStop > nyPost) {
   //      int shrinkamount = yPostStop - nyPost;
   //      yPatchStop -= shrinkamount;
   //      yPostStop = nyPost;
   //   }
   //   if (yPostStop < 0) {
   //      int shrinkamount = -yPostStop;
   //      yPatchStop += shrinkamount;
   //      yPostStop = 0;
   //   }
   //   if (yPatchStart < 0) {
   //      assert(yPatchStart==yPatchStop);
   //      yPatchStart = 0;
   //      yPatchStop = 0;
   //   }
   //   if (yPatchStop > (nyp+nypShrunken)/2) {
   //      assert(yPatchStart==yPatchStop);
   //      yPatchStop = (nyp+nypShrunken)/2;
   //      yPatchStart = yPatchStop;
   //   }
   //   assert(yPostStop-yPostStart==yPatchStop-yPatchStart);

   //   int ny = yPatchStop - yPatchStart;
   //   assert(ny>=0 && ny<=nypShrunken);
   //   assert(yPatchStart>=0 && (yPatchStart<nxp || (ny==0 && yPatchStart==nyp)));

   //   gSynPatchStart[arborId][kex] = (size_t) kIndex(xPostStart,yPostStart,0,nxPost,nyPost,nfPost);
   //   aPostOffset[arborId][kex] = (size_t) kIndex(xPostStart+nbPost,yPostStart+nbPost,0,nxPost+2*nbPost,nyPost+2*nbPost,nfPost);

   //   PVPatch * w = getWeights(kex, arborId);
   //   assert(w->offset==0);
   //   pvpatch_adjust(w, sxp, syp, nx, ny, xPatchStart, yPatchStart);

   //} // loop over patches

   //return PV_SUCCESS;
}

PVPatch *** HyPerConn::convertPreSynapticWeights(double time)
{
   if (time <= wPostTime) {
      return wPostPatches;
   }
   wPostTime = time;

   const PVLayerLoc * preLoc = pre->getLayerLoc();
   const PVLayerLoc * postLoc = post->getLayerLoc();

   const int xScale = post->getXScale() - pre->getXScale();
   const int yScale = post->getYScale() - pre->getYScale();
   const double powXScale = pow(2.0f, (double) xScale);
   const double powYScale = pow(2.0f, (double) yScale);

// fixed?
// TODO - fix this
//   assert(xScale <= 0);
//   assert(yScale <= 0);

   // pre-synaptic weights are in extended layer reference frame
   const int nxPre = preLoc->nx + preLoc->halo.lt + preLoc->halo.rt;
   const int nyPre = preLoc->ny + preLoc->halo.dn + preLoc->halo.up;
   const int nfPre = preLoc->nf;

   const int nxPost  = postLoc->nx;
   const int nyPost  = postLoc->ny;
   const int nfPost  = postLoc->nf;
   const int numPost = post->getNumNeurons();

   nxpPost = (int) (nxp * powXScale);
   nypPost = (int) (nyp * powYScale);
   nfpPost = preLoc->nf;

   int sxPost = nfpPost;
   int syPost = sxPost * nxpPost;
   int spPost = syPost * nypPost;

   // the number of features is the end-point value (normally post-synaptic)
   const int numPostPatch = nxpPost * nypPost * nfpPost; // Post-synaptic weights are never shrunken

   if (wPostPatches == NULL) {
      wPostPatches = (PVPatch***) calloc(numAxonalArborLists, sizeof(PVPatch**));
      assert(wPostPatches!=NULL);
      assert(wPostDataStart == NULL);
      //TODO-CER-2014.4.3 - is the sizeof part correct??????????????????
      //PFS-2014.6.4 - This looks correct; it's of the form "foo * x = (foo *) calloc(numfoos, sizeof(foo))"
      wPostDataStart = (pvwdata_t **) calloc(numAxonalArborLists, sizeof(pvwdata_t *));
      assert(wPostDataStart!=NULL);
      wPostDataStart[0] = allocWeights(numPost, nxpPost, nypPost, nfpPost);
      assert(wPostDataStart[0] != NULL);
      for(int arborID=0;arborID<numberOfAxonalArborLists();arborID++) {
         int status = createWeights(wPostPatches, numPost, numPost, nxpPost, nypPost, nfpPost, arborID);
         assert(status == PV_SUCCESS);
         if (arborID > 0){  // wDataStart already allocated
            wPostDataStart[arborID] = wPostDataStart[0] + spPost * numPost * arborID;
            assert(wPostDataStart[arborID] != NULL);
         }
      }
   }

   //loop through all axons:
   for(int arborID=0;arborID<numberOfAxonalArborLists();arborID++) {

      // loop through post-synaptic neurons (non-extended indices)

      for (int kPost = 0; kPost < numPost; kPost++) {
         int kxPost = kxPos(kPost, nxPost, nyPost, nfPost);
         int kyPost = kyPos(kPost, nxPost, nyPost, nfPost);
         int kfPost = featureIndex(kPost, nxPost, nyPost, nfPost);

         int kxPreHead = zPatchHead(kxPost, nxpPost, post->getXScale(), pre->getXScale());
         int kyPreHead = zPatchHead(kyPost, nypPost, post->getYScale(), pre->getYScale());

         // convert kxPreHead and kyPreHead to extended indices
         kxPreHead += preLoc->halo.lt;
         kyPreHead += preLoc->halo.up;

         // TODO - FIXME for powXScale > 1
   //      int ax = (int) (1.0f / powXScale);
   //      int ay = (int) (1.0f / powYScale);
   //      int xShift = (ax - 1) - (kxPost + (int) (0.5f * ax)) % ax;
   //      int yShift = (ay - 1) - (kyPost + (int) (0.5f * ay)) % ay;

         pvwdata_t * postData = wPostDataStart[arborID] + nxpPost*nypPost*nfpPost*kPost + wPostPatches[arborID][kPost]->offset;
         for (int kp = 0; kp < numPostPatch; kp++) {

            // calculate extended indices of presynaptic neuron {kPre, kzPre}
            int kxPostPatch = (int) kxPos(kp, nxpPost, nypPost, nfPre);
            int kyPostPatch = (int) kyPos(kp, nxpPost, nypPost, nfPre);
            int kfPostPatch = (int) featureIndex(kp, nxpPost, nypPost, nfPre);

            int kxPre = kxPreHead + kxPostPatch;
            int kyPre = kyPreHead + kyPostPatch;
            int kfPre = kfPostPatch;
            int kPre = kIndex(kxPre, kyPre, kfPre, nxPre, nyPre, nfPre);

            // if {kPre, kzPre} out of bounds, set post weight to zero
            if (kxPre < 0 || kyPre < 0 || kxPre >= nxPre || kyPre >= nyPre) {
               assert(kxPre < 0 || kyPre < 0 || kxPre >= nxPre || kyPre >= nyPre);
               postData[kp] = 0.0; // wPostPatches[arborID][kPost]->data[kp] = 0.0;
            }
            else {
               //int arbor = 0;
               //PVPatch * p = wPatches[arborID][kPre];
               //PVPatch * p = c->getWeights(kPre, arbor);

               //const int nfp = p->nf;

               // get strides for possibly shrunken patch
               //const int sxp = p->sx;
               //const int syp = p->sy;
               //const int sfp = p->sf;

               // *** Old Method (fails test_post_weights) *** //
               // The patch from the pre-synaptic layer could be smaller at borders.
               // At top and left borders, calculate the offset back to the original
               // data pointer for the patch.  This make indexing uniform.
               //
   //            int dx = (kxPre < nxPre / 2) ? nxPrePatch - p->nx : 0;
   //            int dy = (kyPre < nyPre / 2) ? nyPrePatch - p->ny : 0;
   //            int prePatchOffset = - p->sx * dx - p->sy * dy;

   //            int kxPrePatch = (nxPrePatch - 1) - ax * kxPostPatch - xShift;
   //            int kyPrePatch = (nyPrePatch - 1) - ay * kyPostPatch - yShift;
   //            int kPrePatch = kIndex(kxPrePatch, kyPrePatch, kfPost, nxPrePatch, nyPrePatch, p->nf);
   //            wPostPatches[kPost]->data[kp] = p->data[kPrePatch + prePatchOffset];
               // ** //

               // *** New Method *** //
               // {kPre, kzPre} store the extended indices of the presynaptic cell
               // {kPost, kzPost} store the restricted indices of the postsynaptic cell

               // {kzPostHead} store the restricted indices of the postsynaptic patch head
               int kxPostHead, kyPostHead, kfPostHead;
               int nxp_post, nyp_post;  // shrunken patch dimensions
               int dx_nxp, dy_nyp;  // shrinkage

               postSynapticPatchHead(kPre, &kxPostHead, &kyPostHead, &kfPostHead, &dx_nxp,
                                        &dy_nyp,  &nxp_post,   &nyp_post);

               //assert(nxp_post == p->nx);
               //assert(nyp_post == p->ny);
               //assert(nfp == postLoc->nf);

               int kxPrePatch, kyPrePatch; // relative index in shrunken patch
               kxPrePatch = kxPost - kxPostHead;
               kyPrePatch = kyPost - kyPostHead;
               int kPrePatch = kfPost * sfp + kxPrePatch * sxp + kyPrePatch * syp;
               pvwdata_t * preData = get_wDataStart(arborID) + nxp*nyp*nfp*kPre + getWeights(kPre,arborID)->offset;
               postData[kp] = preData[kPrePatch];
               // wPostPatches[arborID][kPost]->data[kp] = preData[kPrePatch];

            }
         }
      }
   }
   return wPostPatches;
}

//PVPatch **** HyPerConn::point2PreSynapticWeights2(){
//
//   const PVLayer * lPre  = pre->getCLayer();
//   const PVLayer * lPost = post->getCLayer();
//
//   //xScale is in log format, powScale is post/pre
//   const int xScale = post->getXScale() - pre->getXScale();
//   const int yScale = post->getYScale() - pre->getYScale();
//   const double powXScale = pow(2.0f, (double) xScale);
//   const double powYScale = pow(2.0f, (double) yScale);
//
//// fixed?
//// TODO - fix this
////   assert(xScale <= 0);
////   assert(yScale <= 0);
//
//   const int prePad = lPre->loc.nb;
//
//   // pre-synaptic weights are in extended layer reference frame
//   const int nxPre = lPre->loc.nx + 2 * prePad;
//   const int nyPre = lPre->loc.ny + 2 * prePad;
//   const int nfPre = lPre->loc.nf;
//
//   // post-synaptic weights are in restricted layer
//   const int nxPost  = lPost->loc.nx;
//   const int nyPost  = lPost->loc.ny;
//   const int nfPost  = lPost->loc.nf;
//   const int numPost = lPost->numNeurons;
//
//   nxpPost = (int) (nxp * powXScale);
//   nypPost = (int) (nyp * powYScale);
//   nfpPost = lPre->loc.nf;
//   pvwdata_t z = 0;
//
//   // the number of features is the end-point value (normally post-synaptic)
//   const int numPostPatch = nxpPost * nypPost * nfpPost; // Post-synaptic weights are never shrunken
//
//   if (wPostPatchesp == NULL) {
//
//      //Return data structure
//      wPostPatchesp = (PVPatch****) calloc(numAxonalArborLists, sizeof(PVPatch***));
//      assert(wPostPatchesp!=NULL);
//      assert(wPostDataStartp == NULL);
//      wPostDataStartp = (pvwdata_t ***) calloc(numAxonalArborLists, sizeof(pvdata_t **));
//      assert(wPostDataStartp!=NULL);
//
//      for(int arborID=0;arborID<numberOfAxonalArborLists();arborID++) {
//
//         wPostPatchesp[arborID] = (PVPatch***) calloc(numPost, sizeof(PVPatch**));
//
//         int sx = nfpPost;
//         int sy = sx * nxpPost;
//         int sp = sy * nypPost;
//
//         size_t patchSize = sp * sizeof(pvwdata_t);
//         size_t dataSize = numPost * patchSize;
//
//         wPostDataStartp[arborID] = (pvdata_t **) calloc(dataSize, sizeof(char*));
//
//
//         PVPatch** patcharray = (PVPatch**) (calloc(numPost, sizeof(PVPatch*)));
//         PVPatch ** curpatch = patcharray;
//         for (int i = 0; i < numPost; i++) {
//            wPostPatchesp[arborID][i] = curpatch;
//            curpatch++;
//         }
//        //createWeights(wPostPatches, numPost, nxpPost, nypPost, nfpPost, arborID);
//      }
//   }
//
//}


PVPatch **** HyPerConn::point2PreSynapticWeights()
{

   const PVLayerLoc * preLoc = pre->getLayerLoc();
   const PVLayerLoc * postLoc = post->getLayerLoc();

   const int xScale = post->getXScale() - pre->getXScale();
   const int yScale = post->getYScale() - pre->getYScale();
   const double powXScale = pow(2.0f, (double) xScale);
   const double powYScale = pow(2.0f, (double) yScale);

// fixed?
// TODO - fix this
//   assert(xScale <= 0);
//   assert(yScale <= 0);

   // pre-synaptic weights are in extended layer reference frame
   const int nxPre = preLoc->nx + preLoc->halo.lt + preLoc->halo.rt;
   const int nyPre = preLoc->ny + preLoc->halo.dn + preLoc->halo.up;
   const int nfPre = preLoc->nf;

   const int nxPost  = postLoc->nx;
   const int nyPost  = postLoc->ny;
   const int nfPost  = postLoc->nf;
   const int numPost = post->getNumNeurons();

   nxpPost = (int) (nxp * powXScale);
   nypPost = (int) (nyp * powYScale);
   nfpPost = preLoc->nf;
   pvwdata_t z = 0;

   // the number of features is the end-point value (normally post-synaptic)
   const int numPostPatch = nxpPost * nypPost * nfpPost; // Post-synaptic weights are never shrunken

   if (wPostPatchesp == NULL) {

      //Return data structure
      wPostPatchesp = (PVPatch****) calloc(numAxonalArborLists, sizeof(PVPatch***));
      assert(wPostPatchesp!=NULL);
      assert(wPostDataStartp == NULL);
      wPostDataStartp = (pvwdata_t ***) calloc(numAxonalArborLists, sizeof(pvwdata_t **));
      assert(wPostDataStartp!=NULL);

      for(int arborID=0;arborID<numberOfAxonalArborLists();arborID++) {

         wPostPatchesp[arborID] = (PVPatch***) calloc(numPost, sizeof(PVPatch**));

         int sx = nfpPost;
         int sy = sx * nxpPost;
         int sp = sy * nypPost;

         size_t patchSize = sp * sizeof(pvwdata_t);
         size_t dataSize = numPost * patchSize;

         wPostDataStartp[arborID] = (pvwdata_t **) calloc(dataSize, sizeof(char*));


         PVPatch** patcharray = (PVPatch**) (calloc(numPost, sizeof(PVPatch*)));
         PVPatch ** curpatch = patcharray;
         for (int i = 0; i < numPost; i++) {
            wPostPatchesp[arborID][i] = curpatch;
            curpatch++;
         }
        //createWeights(wPostPatches, numPost, nxpPost, nypPost, nfpPost, arborID);
      }
   }

   //loop through all arbors:
   for(int arborID=0;arborID<numberOfAxonalArborLists();arborID++) {

      // loop through post-synaptic neurons (non-extended indices)
      for (int kPost = 0; kPost < numPost; kPost++) {
         int kxPost = kxPos(kPost, nxPost, nyPost, nfPost);
         int kyPost = kyPos(kPost, nxPost, nyPost, nfPost);
         int kfPost = featureIndex(kPost, nxPost, nyPost, nfPost);

         int kxPreHead = zPatchHead(kxPost, nxpPost, post->getXScale(), pre->getXScale());
         int kyPreHead = zPatchHead(kyPost, nypPost, post->getYScale(), pre->getYScale());

         // convert kxPreHead and kyPreHead to extended indices
         kxPreHead += preLoc->halo.lt;
         kyPreHead += preLoc->halo.up;

         // TODO - FIXME for powXScale > 1
   //      int ax = (int) (1.0f / powXScale);
   //      int ay = (int) (1.0f / powYScale);
   //      int xShift = (ax - 1) - (kxPost + (int) (0.5f * ax)) % ax;
   //      int yShift = (ay - 1) - (kyPost + (int) (0.5f * ay)) % ay;

         //Accessing by patch offset through wPostDataStart by x,y,and feature of a patch
         pvwdata_t ** postData = wPostDataStartp[arborID] + nxpPost*nypPost*nfpPost*kPost + 0;
         for (int kp = 0; kp < numPostPatch; kp++) {

            // calculate extended indices of presynaptic neuron {kPre, kzPre}
            int kxPostPatch = (int) kxPos(kp, nxpPost, nypPost, nfPre);
            int kyPostPatch = (int) kyPos(kp, nxpPost, nypPost, nfPre);
            int kfPostPatch = (int) featureIndex(kp, nxpPost, nypPost, nfPre);

            int kxPre = kxPreHead + kxPostPatch;
            int kyPre = kyPreHead + kyPostPatch;
            int kfPre = kfPostPatch;
            int kPre = kIndex(kxPre, kyPre, kfPre, nxPre, nyPre, nfPre);

            // if {kPre, kzPre} out of bounds, set post weight to zero
            if (kxPre < 0 || kyPre < 0 || kxPre >= nxPre || kyPre >= nyPre) {
               assert(kxPre < 0 || kyPre < 0 || kxPre >= nxPre || kyPre >= nyPre);
               postData[kp] = &z; // wPostPatches[arborID][kPost]->data[kp] = 0.0;
            }
            else {
               //int arbor = 0;
               //PVPatch * p = wPatches[arborID][kPre];
               //PVPatch * p = c->getWeights(kPre, arbor);

               //const int nfp = p->nf;

               // get strides for possibly shrunken patch
               //const int sxp = p->sx;
               //const int syp = p->sy;
               //const int sfp = p->sf;

               // *** Old Method (fails test_post_weights) *** //
               // The patch from the pre-synaptic layer could be smaller at borders.
               // At top and left borders, calculate the offset back to the original
               // data pointer for the patch.  This make indexing uniform.
               //
   //            int dx = (kxPre < nxPre / 2) ? nxPrePatch - p->nx : 0;
   //            int dy = (kyPre < nyPre / 2) ? nyPrePatch - p->ny : 0;
   //            int prePatchOffset = - p->sx * dx - p->sy * dy;

   //            int kxPrePatch = (nxPrePatch - 1) - ax * kxPostPatch - xShift;
   //            int kyPrePatch = (nyPrePatch - 1) - ay * kyPostPatch - yShift;
   //            int kPrePatch = kIndex(kxPrePatch, kyPrePatch, kfPost, nxPrePatch, nyPrePatch, p->nf);
   //            wPostPatches[kPost]->data[kp] = p->data[kPrePatch + prePatchOffset];
               // ** //

               // *** New Method *** //
               // {kPre, kzPre} store the extended indices of the presynaptic cell
               // {kPost, kzPost} store the restricted indices of the postsynaptic cell

               // {kzPostHead} store the restricted indices of the postsynaptic patch head
               int kxPostHead, kyPostHead, kfPostHead;
               int nxp_post, nyp_post;  // shrunken patch dimensions
               int dx_nxp, dy_nyp;  // shrinkage

               postSynapticPatchHead(kPre, &kxPostHead, &kyPostHead, &kfPostHead, &dx_nxp,
                                        &dy_nyp,  &nxp_post,   &nyp_post);

               //assert(nxp_post == p->nx);
               //assert(nyp_post == p->ny);
               //assert(nfp == lPost->loc.nf);

               int kxPrePatch, kyPrePatch; // relative index in shrunken patch
               kxPrePatch = kxPost - kxPostHead;
               kyPrePatch = kyPost - kyPostHead;
               int kPrePatch = kfPost * sfp + kxPrePatch * sxp + kyPrePatch * syp;
               pvwdata_t * preData = get_wDataStart(arborID) + nxp*nyp*nfp*kPre + getWeights(kPre,arborID)->offset;
               postData[kp] = &(preData[kPrePatch]);
               // wPostPatches[arborID][kPost]->data[kp] = preData[kPrePatch];

            }
         }
      }
   }
   return wPostPatchesp;
}


/**
 * Returns the head (kxPre, kyPre) of a pre-synaptic patch given post-synaptic layer indices.
 * @kxPost the post-synaptic kx index (non-extended units)
 * @kyPost the post-synaptic ky index (non-extended units)
 * @kfPost the post-synaptic kf index
 * @kxPre address of the kx index in the pre-synaptic layer (non-extended units) on output
 * @kyPre address of the ky index in the pre-synaptic layer (non-extended units) on output
 *
 * NOTE: kxPre and kyPre may be in the border region
 */
int HyPerConn::preSynapticPatchHead(int kxPost, int kyPost, int kfPost, int * kxPre, int * kyPre)
{
   int status = 0;

   const int xScale = post->getXScale() - pre->getXScale();
   const int yScale = post->getYScale() - pre->getYScale();
   const double powXScale = pow(2, (double) xScale);
   const double powYScale = pow(2, (double) yScale);

   const int nxPostPatch = (int) (nxp * powXScale);
   const int nyPostPatch = (int) (nyp * powYScale);

   int kxPreHead = zPatchHead(kxPost, nxPostPatch, post->getXScale(), pre->getXScale());
   int kyPreHead = zPatchHead(kyPost, nyPostPatch, post->getYScale(), pre->getYScale());

   *kxPre = kxPreHead;
   *kyPre = kyPreHead;

   return status;
}

/**
 * Returns the head (kxPostOut, kyPostOut) of the post-synaptic patch plus other
 * patch information.
 * @kPreEx the pre-synaptic k index (extended units)
 * @kxPostOut address of the kx index in post layer (non-extended units) on output
 * @kyPostOut address of the ky index in post layer (non-extended units) on output
 * @kfPostOut address of the kf index in post layer (non-extended units) on output
 * @dxOut address of the change in x dimension size of patch (to fit border) on output
 * @dyOut address of the change in y dimension size of patch (to fit border) on output
 * @nxpOut address of x dimension patch size (includes border reduction) on output
 * @nypOut address of y dimension patch size (includes border reduction) on output
 *
 * NOTE: kxPostOut and kyPostOut are always within the post-synaptic
 * non-extended layer because the patch size is reduced at borders
 */
int HyPerConn::postSynapticPatchHead(int kPreEx,
                                     int * kxPostOut, int * kyPostOut, int * kfPostOut,
                                     int * dxOut, int * dyOut, int * nxpOut, int * nypOut)
{
   int status = 0;

   const PVLayerLoc * preLoc = pre->getLayerLoc();
   const PVLayerLoc * postLoc = post->getLayerLoc();

   const int nxPre  = preLoc->nx;
   const int nyPre  = preLoc->ny;
   const int kx0Pre = preLoc->kx0;
   const int ky0Pre = preLoc->ky0;
   const int nfPre  = preLoc->nf;

   const int nxexPre = preLoc->nx + preLoc->halo.lt + preLoc->halo.rt;
   const int nyexPre = preLoc->ny + preLoc->halo.dn + preLoc->halo.up;

   const int nxPost  = postLoc->nx;
   const int nyPost  = postLoc->ny;
   const int kx0Post = postLoc->kx0;
   const int ky0Post = postLoc->ky0;

   // kPreEx is in extended frame, this makes transformations more difficult
   //

   // local indices in extended frame
   //
   int kxPre = kxPos(kPreEx, nxexPre, nyexPre, nfPre);
   int kyPre = kyPos(kPreEx, nxexPre, nyexPre, nfPre);

   // convert to global non-extended frame
   //
   kxPre += kx0Pre - preLoc->halo.lt;
   kyPre += ky0Pre - preLoc->halo.up;

   // global non-extended post-synaptic frame
   //
   int kxPost = zPatchHead(kxPre, nxp, pre->getXScale(), post->getXScale());
   int kyPost = zPatchHead(kyPre, nyp, pre->getYScale(), post->getYScale());

   // TODO - can get nf from weight patch but what about kf0?
   // weight patch is actually a pencil and so kfPost is always 0?
   int kfPost = 0;

   // convert to local non-extended post-synaptic frame
   kxPost = kxPost - kx0Post;
   kyPost = kyPost - ky0Post;

   // adjust location so patch is in bounds
   int dx = 0;
   int dy = 0;
   int nxPatch = nxp;
   int nyPatch = nyp;

   if (kxPost < 0) {
      nxPatch -= -kxPost;
      kxPost = 0;
      if (nxPatch < 0) nxPatch = 0;
      dx = nxp - nxPatch;
   }
   else if (kxPost + nxp > nxPost) {
      nxPatch -= kxPost + nxp - nxPost;
      if (nxPatch <= 0) {
         nxPatch = 0;
         kxPost  = nxPost - 1;
      }
   }

   if (kyPost < 0) {
      nyPatch -= -kyPost;
      kyPost = 0;
      if (nyPatch < 0) nyPatch = 0;
      dy = nyp - nyPatch;
   }
   else if (kyPost + nyp > nyPost) {
      nyPatch -= kyPost + nyp - nyPost;
      if (nyPatch <= 0) {
         nyPatch = 0;
         kyPost  = nyPost - 1;
      }
   }

   // if out of bounds in x (y), also out in y (x)
   //
   if (nxPatch == 0 || nyPatch == 0) {
      dx = 0;
      dy = 0;
      nxPatch = 0;
      nyPatch = 0;
      fprintf(stderr, "HyPerConn::postSynapticPatchHead: WARNING patch size is zero\n");
   }

   *kxPostOut = kxPost;
   *kyPostOut = kyPost;
   *kfPostOut = kfPost;

   *dxOut = dx;
   *dyOut = dy;
   *nxpOut = nxPatch;
   *nypOut = nyPatch;

   return status;
}

int HyPerConn::writePostSynapticWeights(double timef, bool last) {
   int status = PV_SUCCESS;
   char path[PV_PATH_MAX];

   const PVLayerLoc * preLoc = pre->getLayerLoc();

   float minVal = FLT_MAX;
   float maxVal = -FLT_MAX;
   for(int arbor=0; arbor<this->numberOfAxonalArborLists(); arbor++) {
      float minVal1 = minWeight(arbor);
      if( minVal1 < minVal ) minVal = minVal1;
      float maxVal1 = maxWeight(arbor);
      if( maxVal1 > maxVal ) maxVal = maxVal1;
   }

   const int numPostPatches = post->getNumNeurons();

   const int xScale = post->getXScale() - pre->getXScale();
   const int yScale = post->getYScale() - pre->getYScale();
   const double powXScale = pow(2, (double) xScale);
   const double powYScale = pow(2, (double) yScale);

   const int nxPostPatch = (int) (nxp * powXScale);
   const int nyPostPatch = (int) (nyp * powYScale);
   const int nfPostPatch = preLoc->nf;

   const char * last_str = (last) ? "_last" : "";

   int chars_needed = 0;
   assert(parent->includeConnectionName()<=2 && parent->includeConnectionName()>=0);
   switch(parent->includeConnectionName()) {
   case 0:
	  chars_needed = snprintf(path, PV_PATH_MAX-1, "%s/w%d_post%s.pvp", parent->getOutputPath(), getConnectionId(), last_str);
	  break;
   case 1:
	  chars_needed = snprintf(path, PV_PATH_MAX-1, "%s/w%d_%s_post%s.pvp", parent->getOutputPath(), getConnectionId(), name, last_str);
	  break;
   case 2:
	  chars_needed = snprintf(path, PV_PATH_MAX-1, "%s/%s_post%s.pvp", parent->getOutputPath(), name, last_str);
	  break;
   default:
	  assert(0);
	  break;
   }

   const PVLayerLoc * postLoc = post->getLayerLoc();
   Communicator * comm = parent->icCommunicator();

   bool append = (last) ? false : ioAppend;

	status = PV::writeWeights(path, comm, (double) timef, append, postLoc,
			nxPostPatch, nyPostPatch, nfPostPatch, minVal, maxVal, wPostPatches,
			wPostDataStart, numPostPatches, numberOfAxonalArborLists(),
			writeCompressedWeights, fileType);

   if(status != PV_SUCCESS) {
      fflush(stdout);
      fprintf(stderr, "Connection \"%s\": writePostSynapticWeights failed at time %f.  Exiting.\n", name, timef);
      abort();
   }

   return PV_SUCCESS;
}

int HyPerConn::sumWeights(int nx, int ny, int offset, pvwdata_t * dataStart, double * sum, double * sum2, pvdata_t * maxVal)
{
   // assert(wp != NULL);
   // TODO CER - should make volatile conditional on GPU usage (this could be slow otherwise)?
   volatile pvwdata_t * w = dataStart + offset;
   // assert(w != NULL);
   // const int nx = wp->nx;
   // const int ny = wp->ny;
   //const int nfp = wp->nf;
   //const int syp = wp->sy;
   double sum_tmp = 0;
   double sum2_tmp = 0;
   pvdata_t max_tmp = -FLT_MAX;
   for (int ky = 0; ky < ny; ky++) {
      for(int iWeight = 0; iWeight < syp; iWeight++ ){
         sum_tmp += w[iWeight];
         sum2_tmp += w[iWeight] * w[iWeight];
         max_tmp = ( max_tmp > w[iWeight] ) ? max_tmp : w[iWeight];
      }
      w += syp;
   }
   *sum = sum_tmp;
   *sum2 = sum2_tmp;
   *maxVal = max_tmp;
   return PV_SUCCESS;
} // sumWeights

int HyPerConn::scaleWeights(int nx, int ny, int offset, pvwdata_t * dataStart, pvdata_t sum, pvdata_t sum2, pvdata_t maxVal)
{
   // assert(wp != NULL);
   int num_weights = nx * ny * nfp; //wp->nf;
   if (!this->normalizeArborsIndividually){
      num_weights *= numberOfAxonalArborLists(); // assumes all arbors shrunken equally at this point (shrink patches should occur after normalize)
   }
   float sigma2 = ( sum2 / num_weights ) - ( sum / num_weights ) * ( sum / num_weights );
   double zero_offset = 0.0f;
   if (normalize_zero_offset){
      // set sum to zero and normalize std of weights to sigma
      zero_offset = sum / num_weights;
      sum = 0.0f;
      maxVal -= zero_offset;
   }
   float scale_factor = 1.0f;
   if (normalize_max) {
      // set maximum weight to normalize_strength
      scale_factor = normalize_strength / ( fabs(maxVal) + (maxVal == 0.0f) );
   }
   else if (sum != 0.0f && !normalize_RMS_amp) {
      scale_factor = normalize_strength / sum;
   }
   else if (!normalize_RMS_amp && (sum == 0.0f) && (sigma2 > 0.0f)) {
      scale_factor = normalize_strength / sqrt(sigma2) ;
   }
   else if (normalize_RMS_amp && (sum2 > 0.0f)) {
      scale_factor = normalize_strength / sqrt(sum2) ;
   }
   else{
	  std::cout << "can't normalize HyPerConn:" << this->name << std::endl;
	  return PV_FAILURE;
   }

   pvwdata_t * w = dataStart + offset;
   assert(w != NULL);
   for (int ky = 0; ky < ny; ky++) {
      for(int iWeight = 0; iWeight < syp; iWeight++ ){
         w[iWeight] = ( w[iWeight] - zero_offset ) * scale_factor;
      }
      w += syp;
   }
   maxVal = ( maxVal - zero_offset ) * scale_factor;
   w = dataStart + offset;
   if (fabs(normalize_cutoff) > 0.0f){
      for (int ky = 0; ky < ny; ky++) {
         for(int iWeight = 0; iWeight < syp; iWeight++ ){
            w[iWeight] = ( fabs(w[iWeight]) > fabs(normalize_cutoff) * maxVal) ? w[iWeight] : 0.0f;
         }
         w += syp;
      }
   }
   //maxVal = ( fabs(maxVal) > fabs(normalize_cutoff) ) ? maxVal : 0.0f;
   this->wMax = maxVal > this->wMax ? maxVal : this->wMax;
   return PV_SUCCESS;
} // scaleWeights

// only checks for certain combinations of normalize parameter settings
int HyPerConn::checkNormalizeWeights(float sum, float sum2, float sigma2, float maxVal)
{
   assert( sum != 0 || sigma2 != 0 ); // Calling routine should make sure this condition is met.
   float tol = 0.01f;
   if (normalize_zero_offset && (normalize_cutoff == 0.0f)){  // condition may be violated is normalize_cutoff != 0.0f
      // set sum to zero and normalize std of weights to sigma
      assert((sum > -tol) && (sum < tol));
      assert((sqrt(sigma2) > (1-tol)*normalize_strength) && (sqrt(sigma2) < (1+tol)*normalize_strength));
   }
   else if (normalize_max) {
      // set maximum weight to normalize_strength
      assert((maxVal > (1-tol)*normalize_strength) && ((maxVal < (1+tol)*normalize_strength)));
    }
   else if (normalize_cutoff == 0.0f && !normalize_RMS_amp){  // condition may be violated if normalize_cutoff != 0.0f
      assert((sum > (1-sign(normalize_strength)*tol)*normalize_strength) && ((sum < (1+sign(normalize_strength)*tol)*normalize_strength)));
   }
   else if (normalize_RMS_amp){
	      assert((sqrt(sum2) > (1-tol)*normalize_strength) && (sqrt(sum2) < (1+tol)*normalize_strength));
   }
   return PV_SUCCESS;

} // checkNormalizeWeights

int HyPerConn::checkNormalizeArbor(PVPatch ** patches, pvwdata_t ** dataStart, int numPatches, int arborId)
{
   int status = PV_SUCCESS;
   int nx = nxp;
   int ny = nyp;
   int offset = 0;
   if (this->normalizeArborsIndividually) {
      for (int k = 0; k < numPatches; k++) {
         if (patches != NULL) {
            PVPatch * wp = patches[k];
            nx = wp->nx;
            ny = wp->ny;
            offset = wp->offset;
         }
//      PVPatch * wp = patches[k];
//      if( wp->nx < nxp || wp->ny < nyp ) {
//         continue;  // Normalization of shrunken patches used unshrunken part, which is no longer available
//      }
         double sum = 0;
         double sum2 = 0;
         float maxVal = -FLT_MAX;
         status = sumWeights(nx, ny, offset, dataStart[arborId] + k * nxp * nyp * nfp,
               &sum, &sum2, &maxVal);
         int num_weights = nx * ny * nfp; //wp->nf;
         float sigma2 = (sum2 / num_weights) - (sum / num_weights) * (sum / num_weights);
         if (sum != 0 || sigma2 != 0) {
            status = checkNormalizeWeights(sum, sum2, sigma2, maxVal);
            assert( status == PV_SUCCESS);
         }
         else {
            fprintf(stderr,
                  "checkNormalizeArbor: connection \"%s\", arbor %d, patch %d has all zero weights.\n",
                  name, arborId, k);
         }
      }
      return PV_SUCCESS;
   }
   else{
      for (int kPatch = 0; kPatch < numPatches; kPatch++) {
         double sumAll = 0.0f;
         double sum2All = 0.0f;
         float maxAll = 0.0f;
         for(int kArbor = 0; kArbor < this->numberOfAxonalArborLists(); kArbor++){
            if (patches != NULL) {
               PVPatch * wp = patches[kArbor];
               nx = wp->nx;
               ny = wp->ny;
               offset = wp->offset;
            }
            double sum, sum2;
            float maxVal;
            // PVPatch * p = patches[kPatch];
            status = sumWeights(nx, ny, offset, dataStart[kArbor] + kPatch*nxp*nyp*nfp, &sum, &sum2, &maxVal);
            assert( (status == PV_SUCCESS) || (status == PV_BREAK) );
            sumAll += sum;
            sum2All += sum2;
            maxAll = maxVal > maxAll ? maxVal : maxAll;
         } // kArbor
         int num_weights = nx * ny * nfp * numberOfAxonalArborLists();
         float sigma2 = ( sum2All / num_weights ) - ( sumAll / num_weights ) * ( sumAll / num_weights );
         for(int kArbor = 0; kArbor < this->numberOfAxonalArborLists(); kArbor++){
            if( sumAll != 0 || sigma2 != 0 ) {
               status = checkNormalizeWeights(sumAll, sum2All, sigma2, maxAll);
               assert(status == PV_SUCCESS );
            }
            else {
					fprintf(stderr,
							"checkNormalizeArbor: connection \"%s\", arbor %d, kernel %d has all zero weights.\n",
							name, kArbor, kPatch);
            }
         }
      }
      return PV_BREAK;
   } // normalizeArborsIndividually
} // checkNormalizeArbor

int HyPerConn::normalizeWeights() {
   int status = PV_SUCCESS;
   if (normalizer) {
      status = normalizer->normalizeWeights(this);
   }
   return status;
}

#ifdef OBSOLETE // Marked obsolete Oct 10, 2013
int HyPerConn::calcPatchSize(int arbor_index, int kex,
                             int * kl_out, int * offset_out,
                             int * nxPatch_out, int * nyPatch_out,
                             int * dx_out, int * dy_out)
{
   int status = PV_SUCCESS;

   const PVLayerLoc * preLoc = pre->getLayerLoc();
   const PVLayerLoc * postLoc = post->getLayerLoc();

   const int prePad  = preLoc->nb;
   const int postPad = postLoc->nb;

   const int nxPre  = preLoc->nx;
   const int nyPre  = preLoc->ny;
   const int kx0Pre = preLoc->kx0;
   const int ky0Pre = preLoc->ky0;
   const int nfPre  = preLoc->nf;

   const int nxexPre = nxPre + 2 * prePad;
   const int nyexPre = nyPre + 2 * prePad;

   const int nxPost  = postLoc->nx;
   const int nyPost  = postLoc->ny;
   const int kx0Post = postLoc->kx0;
   const int ky0Post = postLoc->ky0;
   const int nfPost  = postLoc->nf;

   const int nxexPost = nxPost + 2 * postPad;
   const int nyexPost = nyPost + 2 * postPad;

   // local indices in extended frame
   int kxPre = kxPos(kex, nxexPre, nyexPre, nfPre);
   int kyPre = kyPos(kex, nxexPre, nyexPre, nfPre);

   // convert to global non-extended frame
   kxPre += kx0Pre - prePad;
   kyPre += ky0Pre - prePad;

   // global non-extended post-synaptic frame
   int kxPost = zPatchHead( kxPre, nxp, pre->getXScale(), post->getXScale() );
   int kyPost = zPatchHead( kyPre, nyp, pre->getYScale(), post->getYScale() );

   // TODO - can get nf from weight patch but what about kf0?
   // weight patch is actually a pencil and so kfPost is always 0?
   int kfPost = 0;

   // convert to local non-extended post-synaptic frame
   kxPost = kxPost - kx0Post;
   kyPost = kyPost - ky0Post;

   // adjust location so patch is in bounds
   int dx = 0;
   int dy = 0;
   int nxPatch = nxp;
   int nyPatch = nyp;

   if (kxPost < 0) {
      nxPatch -= -kxPost;
      kxPost = 0;
      if (nxPatch < 0) nxPatch = 0;
      dx = nxp - nxPatch;
   }
   else if (kxPost + nxp > nxPost) {
      nxPatch -= kxPost + nxp - nxPost;
      if (nxPatch <= 0) {
         nxPatch = 0;
         kxPost = nxPost - 1;
      }
   }

   if (kyPost < 0) {
      nyPatch -= -kyPost;
      kyPost = 0;
      if (nyPatch < 0) nyPatch = 0;
      dy = nyp - nyPatch;
   }
   else if (kyPost + nyp > nyPost) {
      nyPatch -= kyPost + nyp - nyPost;
      if (nyPatch <= 0) {
         nyPatch = 0;
         kyPost  = nyPost - 1;
      }
   }

   // if out of bounds in x (y), also out in y (x)
   if (nxPatch == 0 || nyPatch == 0) {
      dx = 0;
      dy = 0;
      nxPatch = 0;
      nyPatch = 0;
   }

   //If higher than post, set equal to npost
   if (nxPatch > nxPost) {
      nxPatch = nxPost;
   }
   if (nyPatch > nyPost) {
         nyPatch = nyPost;
   }

   // local non-extended index but shifted to be in bounds
   int kl = kIndex(kxPost, kyPost, kfPost, nxPost, nyPost, nfPost);
   assert(kl >= 0);
   assert(kl < post->getNumNeurons());

   // get offset in extended frame
   kxPost += postPad;
   kyPost += postPad;

   int offset = kIndex(kxPost, kyPost, kfPost, nxexPost, nyexPost, nfPost);
   assert(offset >= 0);
   assert(offset < post->getNumExtended());

   // set return variables
   *kl_out = kl;
   *offset_out = offset;
   *nxPatch_out = nxPatch;
   *nyPatch_out = nyPatch;
   *dx_out = dx;
   *dy_out = dy;

   return status;
}
#endif // OBSOLETE

//int HyPerConn::patchSizeFromFile(const char * filename) {
//   // use patch dimensions from file if (filename != NULL)
//   //
//   int status = PV_SUCCESS;
//   int filetype, datatype;
//   double timed = 0.0;
//
//   int numWgtParams = NUM_WGT_PARAMS;
//
//   Communicator * comm = parent->icCommunicator();
//
//   char nametmp[PV_PATH_MAX];
//   for (int arborId = 0; arborId < this->numberOfAxonalArborLists(); arborId++){
//      snprintf(nametmp, PV_PATH_MAX-1, "%s", filename);
//
//      status = pvp_read_header(nametmp, comm, &timed, &filetype, &datatype, fileparams, &numWgtParams);
//      if (status < 0) return status;
//      assert(numWgtParams==NUM_WGT_PARAMS);
//
//      // const PVLayerLoc loc = pre->getCLayer()->loc; // checkPVPFileHeader moved to communicate since pre needs to be defined.
//      // status = checkPVPFileHeader(comm, &loc, wgtParams, numWgtParams);
//      // if (status < 0) return status;
//
//      // reconcile differences with inputParams
//      status = checkWeightsHeader(nametmp, fileparams);
//   }
//   return status;
//}

int HyPerConn::checkPatchDimensions() {
   int statusx = checkPatchSize(nxp, pre->getXScale(), post->getXScale(), 'x');
   int statusy = checkPatchSize(nyp, pre->getYScale(), post->getYScale(), 'y');
   int status = statusx==PV_SUCCESS && statusy==PV_SUCCESS ? PV_SUCCESS : PV_FAILURE;
   return status;
}

int HyPerConn::checkPatchSize(int patchSize, int scalePre, int scalePost, char dim) {
   int scaleDiff = scalePre - scalePost;
   bool goodsize;

   if( scaleDiff > 0) {
      // complain if patchSize is not an odd number times 2^xScaleDiff
      int scaleFactor = (int) pow(2, (double) scaleDiff);
      int shouldbeodd = patchSize/scaleFactor;
      goodsize = shouldbeodd > 0 && shouldbeodd % 2 == 1 && patchSize == shouldbeodd*scaleFactor;
   }
   else {
      // complain if patchSize is not an odd number
      goodsize = patchSize > 0 && patchSize % 2 == 1;
   }
   if( !goodsize ) {
      fprintf(stderr, "Error:  Connection: %s\n",name);
      fprintf(stderr, "Presynaptic layer:  %s\n", pre->getName());
      fprintf(stderr, "Postsynaptic layer: %s\n", post->getName());
      fprintf(stderr, "Patch size n%cp=%d is not compatible with presynaptic n%cScale %f\n",
              dim,patchSize,dim,pow(2,-scalePre));
      fprintf(stderr, "and postsynaptic n%cScale %f.\n",dim,pow(2,-scalePost));
      if( scaleDiff > 0) {
         int scaleFactor = (int) pow(2, (float) scaleDiff);
         fprintf(stderr, "(postsynaptic scale) = %d * (presynaptic scale);\n", scaleFactor);
         fprintf(stderr, "therefore compatible sizes are %d times an odd number.\n", scaleFactor);
      }
      else {
         fprintf(stderr, "(presynaptic scale) >= (postsynaptic scale);\n");
         fprintf(stderr, "therefore patch size must be odd\n");
      }
      fprintf(stderr, "Exiting.\n");
      exit(1);
   }
   return PV_SUCCESS;
}

int HyPerConn::setPatchStrides() {
   // these strides are for the weight patches
   sfp = 1;
   sxp = nfp;
   syp = nfp * nxp;

   // these strides are for a post-synaptic non-extended layer variable.
   // HyPerLayer::recvSynapticInput uses these strides for GSyn, which is nonextended
   postNonextStrides.sf = 1;
   postNonextStrides.sx = nfp;
   postNonextStrides.sy = nfp * post->getLayerLoc()->nx;

   // these strides are for a post-synaptic extended layer variable.
   postExtStrides.sf = 1;
   postExtStrides.sx = nfp;
   postExtStrides.sy = nfp * (post->getLayerLoc()->nx+post->getLayerLoc()->halo.lt+post->getLayerLoc()->halo.rt);

   return PV_SUCCESS;
}

//pvwdata_t * HyPerConn::allocWeights(PVPatch *** patches, int nPatches, int nxPatch,
//      int nyPatch, int nfPatch, int arborId)
pvwdata_t * HyPerConn::allocWeights(int nPatches, int nxPatch, int nyPatch, int nfPatch)
{
   int sx = nfPatch;
   int sy = sx * nxPatch;
   int sp = sy * nyPatch;

   size_t patchSize = sp * sizeof(pvwdata_t);
   size_t dataSize = nPatches * patchSize;
   //if (arborId > 0){  // wDataStart already allocated
	//   assert(this->get_wDataStart(0) != NULL);
	//   return (this->get_wDataStart(0) + sp * nPatches * arborId);
	//}
   // arborID == 0
   size_t arborSize = dataSize * this->numberOfAxonalArborLists();
   pvwdata_t * dataPatches = NULL;
#ifdef USE_SHMGET
   int arbor_ID = 0;
   if (!shmget_flag) {
      dataPatches = (pvwdata_t *) calloc(arborSize, sizeof(char));
   } else {
      assert(sharedWeights);
      shmget_owner[arbor_ID] = true;
      // shmget diagnostics
#define SHMGET_DEBUG
#ifdef SHMGET_DEBUG
      if (arbor_ID == 0 || arbor_ID == (this->numberOfAxonalArborLists()-1)) {
         std::cout << "rank = " << parent->icCommunicator()->commRank();
         std::cout << ", arbor_ID = " << arbor_ID;
      }
#endif // SHMGET_DEBUG
      // dataSize must be a multiple of PAGE_SIZE
      size_t shmget_dataSize = (floor(arborSize / PAGE_SIZE) + 1) * PAGE_SIZE;
      key_t key = IPC_PRIVATE;
      const int max_arbors = 8712;
      key = 11 + (this->getConnectionId() + 1) * max_arbors + arbor_ID; //hopefully unique key identifier for all shared memory associated with this connection arbor
      int shmflg = (IPC_CREAT | IPC_EXCL | 0666);
      char *segptr;

      // check for existing segment associated with this key, delete existing segment if present, then insert barrier to ensure
      // all processes have completed this check before attempting to create new shared memory segment
      int shmget_existing_ID = shmget(key, shmget_dataSize, 0666);
      if (shmget_existing_ID != -1){
         shmid_ds * shmget_ds = NULL;
         int shmctl_status = shmctl(shmget_existing_ID, IPC_RMID,
               shmget_ds);
         std::cout << "shmctl_status = " << shmctl_status << std::endl;
         //          assert(shmget_status==0);
      }
#ifdef PV_USE_MPI
      MPI_Barrier(getParent()->icCommunicator()->communicator());
#endif // PV_USE_MPI


      /* Open the shared memory segment - create if necessary */
      if ((shmget_id[arbor_ID] = shmget(key, shmget_dataSize, shmflg))
            == -1) {
         if (errno != EEXIST) {
            std::cout << std::endl;
            std::cout << "key = " << key << ", shmget_dataSize = "
                  << shmget_dataSize << ", shmflg = "
                  << shmflg << std::endl;
            perror("shmget: unable to create shared memory segment");
            exit(1);
         }
         /* Segment already exists - try as a client */
         shmget_owner[arbor_ID] = false;
         int shmget_flag2 = (IPC_CREAT | 0666);
         if ((shmget_id[arbor_ID] = shmget(key, shmget_dataSize,
               shmget_flag2)) == -1) {
            perror(
                  "shmget: unable to obtain id of existing shared memory segment");
            exit(1);
         }
      }
#ifdef SHMGET_DEBUG
      if (arbor_ID == 0 || arbor_ID == (this->numberOfAxonalArborLists()-1)) {
         std::cout << ", shmget_owner = " << shmget_owner[arbor_ID]
                                                          << std::endl;
      }
#endif // SHMGET_DEBUG
      /* Attach (map) the shared memory segment into the current process */
      if ((segptr = (char *) shmat(shmget_id[arbor_ID], 0, 0))
            == (char *) -1) {
         perror("shmat: unable to map shared memory segment");
         exit(1);
      }
      dataPatches = (pvwdata_t *) segptr;
   }
#else
   dataPatches = (pvwdata_t *) calloc(arborSize, sizeof(char));
#endif // USE_SHMGET
   if(dataPatches == NULL) {
      fprintf(stderr, "Error allocating weights for connection \"%s\": %s\n", getName(), strerror(errno));
      exit(EXIT_FAILURE);
   }
   return dataPatches;
}

int HyPerConn::patchToDataLUT(int patchIndex) {
   return sharedWeights ? patch2datalookuptable[patchIndex] : patchIndex;
}

int HyPerConn::patchIndexToDataIndex(int patchIndex, int * kx/*default=NULL*/, int * ky/*default=NULL*/, int * kf/*default=NULL*/) {
   int dataIndex;
   if (sharedWeights) {
      dataIndex = calcUnitCellIndex(patchIndex, kx, ky, kf);
   }
   else {
      const PVLayerLoc * preLoc = pre->getLayerLoc();
      if(kx) *kx = kxPos(patchIndex, preLoc->nx + preLoc->halo.lt + preLoc->halo.rt, preLoc->ny + preLoc->halo.dn + preLoc->halo.up, preLoc->nf);
      if(ky) *ky = kyPos(patchIndex, preLoc->nx + preLoc->halo.lt + preLoc->halo.rt, preLoc->ny + preLoc->halo.dn + preLoc->halo.up, preLoc->nf);
      if(kf) *kf = featureIndex(patchIndex, preLoc->nx + preLoc->halo.lt + preLoc->halo.rt, preLoc->ny + preLoc->halo.dn + preLoc->halo.up, preLoc->nf);
      dataIndex = patchIndex;
   }
   return dataIndex;
}

int HyPerConn::dataIndexToUnitCellIndex(int dataIndex, int * kx/*default=NULL*/, int * ky/*default=NULL*/, int * kf/*default=NULL*/) {
   int unitCellIndex;
   if (sharedWeights) {
      int nfUnitCell = pre->getLayerLoc()->nf;
      int nxUnitCell = zUnitCellSize(pre->getXScale(), post->getXScale());
      int nyUnitCell = zUnitCellSize(pre->getYScale(), post->getYScale());
      assert( dataIndex >= 0 && dataIndex < nxUnitCell*nyUnitCell*nfUnitCell );
      if(kx) *kx = kxPos(dataIndex, nxUnitCell, nyUnitCell, nfUnitCell);
      if(ky) *ky = kyPos(dataIndex, nxUnitCell, nyUnitCell, nfUnitCell);
      if(kf) *kf = featureIndex(dataIndex, nxUnitCell, nyUnitCell, nfUnitCell);
      unitCellIndex = dataIndex;
   }
   else {
      unitCellIndex = calcUnitCellIndex(dataIndex, kx, ky, kf);
   }
   return unitCellIndex;
}

int HyPerConn::calcUnitCellIndex(int patchIndex, int * kxUnitCellIndex/*default=NULL*/, int * kyUnitCellIndex/*default=NULL*/, int * kfUnitCellIndex/*default=NULL*/) {
   const PVLayerLoc * preLoc = pre->getLayerLoc();
   int nxUnitCell = zUnitCellSize(pre->getXScale(), post->getXScale());
   int nyUnitCell = zUnitCellSize(pre->getYScale(), post->getYScale());
   int unitCellIndex = layerIndexToUnitCellIndex(patchIndex, preLoc, nxUnitCell, nyUnitCell,
         kxUnitCellIndex, kyUnitCellIndex, kfUnitCellIndex);
   return unitCellIndex;
}

void HyPerConn::connOutOfMemory(const char * funcname) {
   fprintf(stderr, "Out of memory error in %s for connection \"%s\"\n", funcname, name);
   exit(EXIT_FAILURE);
}

} // namespace PV

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#ifndef PV_USE_OPENCL
#  include "../kernels/HyPerLayer_recv_synaptic_input.cl"
#else
#  undef PV_USE_OPENCL
#  include "../kernels/HyPerLayer_recv_synaptic_input.cl"
#  define PV_USE_OPENCL
#endif

#ifdef __cplusplus
}
#endif // __cplusplus
