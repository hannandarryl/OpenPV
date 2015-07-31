/*
 * PointProbe.hpp
 *
 *  Created on: Mar 10, 2009
 *      Author: rasmussn
 */

#ifndef POINTPROBE_HPP_
#define POINTPROBE_HPP_

#include "LayerProbe.hpp"

namespace PV {

class PointProbe: public PV::LayerProbe {
public:
   PointProbe(const char * probeName, HyPerCol * hc);
   virtual ~PointProbe();

   virtual int communicateInitInfo();

   virtual int outputState(double timef);

   // void setSparseOutput(bool flag) {sparseOutput = flag;}

protected:
   int xLoc;
   int yLoc;
   int fLoc;
   int batchLoc;
   char * msg;

   // bool sparseOutput;

   PointProbe();
   int initialize(const char * probeName, HyPerCol * hc);
   virtual int ioParamsFillGroup(enum ParamsIOFlag ioFlag);
   virtual void ioParam_xLoc(enum ParamsIOFlag ioFlag);
   virtual void ioParam_yLoc(enum ParamsIOFlag ioFlag);
   virtual void ioParam_fLoc(enum ParamsIOFlag ioFlag);
   virtual void ioParam_batchLoc(enum ParamsIOFlag ioFlag);
   virtual int initOutputStream(const char * filename);
   virtual int point_writeState(double timef, float outVVal, float outAVal);

private:
   int initPointProbe_base();
};

}

#endif /* POINTPROBE_HPP_ */