#ifndef MAXPOOLTESTLAYER_HPP_ 
#define MAXPOOLTESTLAYER_HPP_

#include <layers/ANNLayer.hpp>

namespace PV {

class MaskTestLayer: public PV::ANNLayer{
public:
	MaskTestLayer(const char* name, HyPerCol * hc);

protected:
   virtual int updateState(double timef, double dt);

private:
};

} /* namespace PV */
#endif
