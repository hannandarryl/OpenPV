#include "layers/HyPerLayer.hpp"
#include "structures/Buffer.hpp"
#include <vector>

std::vector<float> copyOutput(PV::HyPerLayer *layer) {
   PVLayerLoc const *loc = layer->getLayerLoc();
   int const nxExt       = loc->nx + loc->halo.lt + loc->halo.rt;
   int const nyExt       = loc->ny + loc->halo.dn + loc->halo.up;
   int const nf          = loc->nf;
   float const *data     = layer->getLayerData();
   PV::Buffer<float> outputBuffer{data, nxExt, nyExt, nf};
   return outputBuffer.asVector();
}
