/*
 * KernelConn.hpp
 *
 *  Created on: Aug 6, 2009
 *      Author: gkenyon
 */

#ifndef KERNELCONN_HPP_
#define KERNELCONN_HPP_

#include "HyPerConn.hpp"

namespace PV {

class KernelConn: public HyPerConn {

public:

   KernelConn();

   KernelConn(const char * name, HyPerCol * hc, HyPerLayer * pre, HyPerLayer * post,
              int channel, const char * filename);
   KernelConn(const char * name, HyPerCol * hc, HyPerLayer * pre, HyPerLayer * post,
              int channel);
   KernelConn(const char * name, HyPerCol * hc, HyPerLayer * pre, HyPerLayer * post);
   virtual int numDataPatches(int arbor);

protected:
   PVPatch ** kernelPatches;   // list of weight patches
   virtual int deleteWeights();
   virtual int initialize_base();
   virtual PVPatch ** createWeights(PVPatch ** patches, int nPatches, int nxPatch,
         int nyPatch, int nfPatch);
   virtual PVPatch ** allocWeights(PVPatch ** patches, int nPatches, int nxPatch,
         int nyPatch, int nfPatch);
   virtual PVPatch ** initializeWeights(PVPatch ** patches, int numPatches,
         const char * filename);
   virtual PVPatch ** readWeights(PVPatch ** patches, int numPatches,
                                     const char * filename);
   virtual int writeWeights(float time, bool last=false);

};

}

#endif /* KERNELCONN_HPP_ */
