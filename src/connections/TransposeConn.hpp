/*
 * TransposeConn.hpp
 *
 *  Created on: May 16, 2011
 *      Author: peteschultz
 */

#ifndef TRANSPOSECONN_HPP_
#define TRANSPOSECONN_HPP_

#include "PeriodicUpdateConn.hpp"

namespace PV {

class TransposeConn: public PeriodicUpdateConn {
public:
    TransposeConn();
    TransposeConn(const char * name, HyPerCol * hc, HyPerLayer * preLayer, HyPerLayer * postLayer, ChannelType channelType, KernelConn * auxConn);
    virtual ~TransposeConn();
    inline KernelConn * getOriginalConn() {return originalConn;}

    virtual int updateWeights(int axonId);


protected:
    virtual int initialize_base();
    int initialize(const char * name, HyPerCol * hc, HyPerLayer * preLayer, HyPerLayer * postLayer, ChannelType channelType, KernelConn * auxConn);
    virtual PVPatch ** initializeWeights(PVPatch ** patches, int numPatches, const char * filename);
    int setPatchSize(const char * filename);
    int transposeKernels();
    KernelConn * originalConn;
};

}  // end namespace PV

#endif /* TRANSPOSECONN_HPP_ */
