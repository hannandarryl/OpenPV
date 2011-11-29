/*
 * HyPerLayer.hpp
 *
 *  Created on: Aug 3, 2008
 *      Author: dcoates
 */

#ifndef HYPERLAYER_HPP_
#define HYPERLAYER_HPP_

#include "../layers/PVLayer.h"
#include "../layers/LayerDataInterface.hpp"
#include "../columns/DataStore.hpp"
#include "../columns/HyPerCol.hpp"
#include "../columns/InterColComm.hpp"
#include "../io/LayerProbe.hpp"
#include "../include/pv_common.h"
#include "../include/pv_types.h"
#include "../utils/Timer.hpp"

#ifdef PV_USE_OPENCL
#define PV_CL_COPY_BUFFERS 0
#define PV_CL_EVENTS 1
#include "../arch/opencl/CLKernel.hpp"
#endif

namespace PV {

class HyPerLayer : public LayerDataInterface {

protected:

   // only subclasses can be constructed directly
   HyPerLayer(const char * name, HyPerCol * hc, int numChannels);

   virtual int initializeLayerId(int layerId);
   virtual int initializeV(bool restart_flag);
   int readBufferFile(const char * filename, InterColComm * comm, double * timed, pvdata_t * buffer, int numbands, bool extended, bool contiguous);
   int readDataStoreFromFile(const char * filename, InterColComm * comm, double * timed);
   int readHeader(const char * filename, InterColComm * comm, double * timed, int * params);
   int writeBufferFile(const char * filename, InterColComm * comm, double dtime, pvdata_t * buffer, int numbands, bool extended, bool contiguous);
   int writeBuffer(FILE * fp, InterColComm * comm, double dtime, pvdata_t * buffer, int numbands, bool extended, bool contiguous);
   int incrementNBands(int * numCalls);
   int writeDataStoreToFile(const char * filename, InterColComm * comm, double dtime);

#ifdef PV_USE_OPENCL
   virtual int initializeThreadBuffers(const char * kernelName);
   virtual int initializeThreadKernels(const char * kernelName);
#endif

private:
   int initialize_base(const char * name, HyPerCol * hc, int numChannels);

public:

   virtual ~HyPerLayer() = 0;

   static int copyToBuffer(pvdata_t * buf, const pvdata_t * data,
                           const PVLayerLoc * loc, bool extended, float scale);
   static int copyToBuffer(unsigned char * buf, const pvdata_t * data,
                           const PVLayerLoc * loc, bool extended, float scale);

   template <typename T>
   static int copyFromBuffer(const T * buf, T * data,
                             const PVLayerLoc * loc, bool extended, T scale);

   static int copyFromBuffer(const unsigned char * buf, pvdata_t * data,
                             const PVLayerLoc * loc, bool extended, float scale);

   // TODO - make protected
   PVLayer  * clayer;
   HyPerCol * parent;

   virtual int triggerReceive(InterColComm * comm);
   virtual int recvSynapticInput(HyPerConn * conn, const PVLayerCube * cube, int arborID);
   virtual int updateState (float time, float dt);
   virtual int updateBorder(float time, float dt);
   virtual int publish(InterColComm * comm, float time);
   virtual int waitOnPublish(InterColComm * comm);

   virtual int updateV();
   virtual int setActivity();
   virtual int resetGSynBuffers();
   virtual int updateActiveIndices();
   int resetBuffer(pvdata_t * buf, int numItems);

   virtual int reconstruct(HyPerConn * conn, PVLayerCube * cube);

   int initialize(PVLayerType type);
   int initFinish();

   int mirrorInteriorToBorder(int whichBorder, PVLayerCube * cube, PVLayerCube * borderCube);

   virtual int columnWillAddLayer(InterColComm * comm, int id);

   virtual int checkpointRead(float * timef);
   virtual int checkpointWrite();

   virtual int readState (float * time);
   virtual int writeState(float time, bool last=false);
   virtual int outputState(float time, bool last=false);
#ifdef OBSOLETE // (marked obsolete Jan 24, 2011)
   virtual int writeActivity(const char * filename, float time);
#endif // OBSOLETE
   virtual int writeActivity(float time);
   virtual int writeActivitySparse(float time);

   virtual int insertProbe(LayerProbe * probe);

   /** returns the number of neurons in layer (for borderId=0) or a border region **/
   virtual int numberOfNeurons(int borderId);

   virtual int mirrorToNorthWest(PVLayerCube * dest, PVLayerCube * src);
   virtual int mirrorToNorth    (PVLayerCube * dest, PVLayerCube* src);
   virtual int mirrorToNorthEast(PVLayerCube * dest, PVLayerCube * src);
   virtual int mirrorToWest     (PVLayerCube * dest, PVLayerCube * src);
   virtual int mirrorToEast     (PVLayerCube * dest, PVLayerCube * src);
   virtual int mirrorToSouthWest(PVLayerCube * dest, PVLayerCube * src);
   virtual int mirrorToSouth    (PVLayerCube * dest, PVLayerCube * src);
   virtual int mirrorToSouthEast(PVLayerCube * dest, PVLayerCube * src);

   const char * getOutputFilename(char * buf, const char * dataName, const char * term);

   // Public access functions:

   const char * getName()            {return name;}

   int getNumNeurons()               {return clayer->numNeurons;}
   int getNumExtended()              {return clayer->numExtended;}
   int getNumGlobalNeurons()         {const PVLayerLoc * loc = getLayerLoc(); return loc->nxGlobal*loc->nyGlobal*loc->nf;}
   int getNumGlobalExtended()        {const PVLayerLoc * loc = getLayerLoc(); return (loc->nxGlobal+2*loc->nb)*(loc->nyGlobal+2*loc->nb)*loc->nf;}

   int  getLayerId()                 {return clayer->layerId;}
   PVLayerType getLayerType()        {return clayer->layerType;}
   void setLayerId(int id)           {clayer->layerId = id;}
   int increaseDelayLevels(int neededDelay);

   PVLayer*  getCLayer()             {return clayer;}
   pvdata_t * getV()                 {return clayer->V;}           // name query
   int getNumChannels()              {return numChannels;}
   pvdata_t * getChannel(ChannelType ch) {                         // name query
      return ch < this->numChannels ? GSyn[ch] : NULL;
   }
   int getXScale()                   {return clayer->xScale;}
   int getYScale()                   {return clayer->yScale;}

   HyPerCol* getParent()             {return parent;}
   void setParent(HyPerCol* parent)  {this->parent = parent;}

   bool useMirrorBCs()               {return this->mirrorBCflag;}
   bool getSpikingFlag()             {return this->spikingFlag;}

   // implementation of LayerDataInterface interface
   //
   const pvdata_t   * getLayerData(int delay=0);
   const PVLayerLoc * getLayerLoc()  { return &(clayer->loc); }
   bool isExtended()                 { return true; }

   virtual int gatherToInteriorBuffer(unsigned char * buf);

   virtual int label(int k);

   virtual int * getMarginIndices();
   virtual int getNumMargin();

protected:

   void freeChannels();

   char * name;                 // well known name of layer

   int numChannels;             // number of channels
   pvdata_t ** GSyn;            // of dynamic length numChannels

   int numProbes;
   LayerProbe ** probes;

   int * labels;                // label for the feature a neuron is tuned to

   bool mirrorBCflag;           // true when mirror BC are to be applied

   int ioAppend;                // controls opening of binary files
   float writeTime;             // time of next output
   float writeStep;             // output time interval

   bool spikingFlag;
   bool writeNonspikingActivity;
   int writeActivityCalls;      // Number of calls to writeActivity (written to nbands in the header of the a%d.pvp file)
   int writeActivitySparseCalls; // Number of calls to writeActivitySparse (written to nbands in the header of the a%d.pvp file)

   int * marginIndices;   // indices of neurons in margin
   int numMargin;         // number of neurons in margin

   // OpenCL variables
   //
#ifdef PV_USE_OPENCL
public:
   virtual int getNumKernelArgs() {return numKernelArgs;}
   virtual int getNumCLEvents()   {return numEvents;}

   CLBuffer * getChannelCLBuffer(ChannelType ch) {
      return ch < this->numChannels ? clGSyn[ch] : NULL;
   }

   CLBuffer * getLayerDataStoreCLBuffer();
   size_t     getLayerDataStoreOffset(int delay=0);

protected:

   CLKernel * krUpdate;        // CL kernel for update state call

   // OpenCL buffers
   //
   CLBuffer * clV;
   CLBuffer **clGSyn;         // of dynamic length numChannels
   CLBuffer * clActivity;
   CLBuffer * clPrevTime;
   CLBuffer * clParams;       // for transferring params to kernel

   int numKernelArgs;         // number of arguments in kernel call
   int numEvents;             // number of events in event list
   int numWait;               // number of events to wait for
   cl_event * evList;         // event list
   cl_event   evUpdate;

   int nxl;  // local OpenCL grid size in x
   int nyl;  // local OpenCL grid size in y
#endif

   Timer * update_timer;
   Timer * recvsyn_timer;
};

} // namespace PV

// Template functions
//
template <typename T>
int PV::HyPerLayer::copyFromBuffer(const T * buf, T * data,
                                   const PVLayerLoc * loc, bool extended, T scale)
{
   size_t sf, sx, sy;

   const int nx = loc->nx;
   const int ny = loc->ny;
   const int nf = loc->nf;

   int nxBorder = 0;
   int nyBorder = 0;

   if (extended) {
      nxBorder = loc->nb;
      nyBorder = loc->nb;
      sf = strideFExtended(loc);
      sx = strideXExtended(loc);
      sy = strideYExtended(loc);
   }
   else {
      sf = strideF(loc);
      sx = strideX(loc);
      sy = strideY(loc);
   }

   int ii = 0;
   for (int j = 0; j < ny; j++) {
      int jex = j + nyBorder;
      for (int i = 0; i < nx; i++) {
         int iex = i + nxBorder;
         for (int f = 0; f < nf; f++) {
            data[iex*sx + jex*sy + f*sf] = scale * buf[ii++];
         }
      }
   }
   return 0;
}

#endif /* HYPERLAYER_HPP_ */
