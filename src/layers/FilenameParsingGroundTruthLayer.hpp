/*
 * FilenameParsingGroundTruthLayer.hpp
 *
 *  Created on: Nov 10, 2014
 *      Author: wchavez
 */

#ifndef FILENAMEPARSINGGROUNDTRUTHLAYER_HPP_
#define FILENAMEPARSINGGROUNDTRUTHLAYER_HPP_

#include "ANNLayer.hpp"
#include <string>
#include "Movie.hpp"
namespace PV {

class FilenameParsingGroundTruthLayer: public PV::ANNLayer {
public:
   FilenameParsingGroundTruthLayer(const char * name, HyPerCol * hc);
   virtual ~FilenameParsingGroundTruthLayer();
   virtual int initialize(const char * name, HyPerCol * hc);
   virtual int updateState(double timef, double dt);
   virtual bool needUpdate(double time, double dt);
   int ioParamsFillGroup(enum ParamsIOFlag ioFlag);
   void ioParam_classes(enum ParamsIOFlag ioFlag);
   void ioParam_movieLayerName(enum ParamsIOFlag ioFlag);
private:
   std::ifstream inputfile;
   std::string * classes;
   int numClasses;
   char * movieLayerName;
   Movie * movieLayer;
};

} /* namespace PV */
#endif /* FILENAMEPARSINGGROUNDTRUTHLAYER_HPP_ */
