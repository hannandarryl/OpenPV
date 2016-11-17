/*
 * LayerPhaseTestProbe.cpp
 *
 *  Created on: January 27, 2013
 *      Author: garkenyon
 */

#include "LayerPhaseTestProbe.hpp"

namespace PV {

LayerPhaseTestProbe::LayerPhaseTestProbe(const char *probeName, HyPerCol *hc) : StatsProbe() {
   initLayerPhaseTestProbe_base();
   initLayerPhaseTestProbe(probeName, hc);
}

int LayerPhaseTestProbe::initLayerPhaseTestProbe_base() { return PV_SUCCESS; }

int LayerPhaseTestProbe::initLayerPhaseTestProbe(const char *probeName, HyPerCol *hc) {
   return initStatsProbe(probeName, hc);
}

int LayerPhaseTestProbe::ioParamsFillGroup(enum ParamsIOFlag ioFlag) {
   int status = StatsProbe::ioParamsFillGroup(ioFlag);
   ioParam_equilibriumValue(ioFlag);
   ioParam_equilibriumTime(ioFlag);
   return status;
}

void LayerPhaseTestProbe::ioParam_buffer(enum ParamsIOFlag ioFlag) { requireType(BufV); }

void LayerPhaseTestProbe::ioParam_equilibriumValue(enum ParamsIOFlag ioFlag) {
   getParent()->parameters()->ioParamValue(
         ioFlag, getName(), "equilibriumValue", &equilibriumValue, 0.0f, true);
}

void LayerPhaseTestProbe::ioParam_equilibriumTime(enum ParamsIOFlag ioFlag) {
   getParent()->parameters()->ioParamValue(
         ioFlag, getName(), "equilibriumTime", &equilibriumTime, 0.0, true);
}

int LayerPhaseTestProbe::outputState(double timed) {
   int status           = StatsProbe::outputState(timed);
   Communicator *icComm = getTargetLayer()->getParent()->getCommunicator();
   const int rcvProc    = 0;
   if (icComm->commRank() != rcvProc) {
      return 0;
   }
   for (int b = 0; b < parent->getNBatch(); b++) {
      if (timed >= equilibriumTime) {
         double tol = 1e-6;
         FatalIf(!(fabs(fMin[b] - equilibriumValue) < tol), "Test failed.\n");
         FatalIf(!(fabs(fMax[b] - equilibriumValue) < tol), "Test failed.\n");
         FatalIf(!(fabs(avg[b] - equilibriumValue) < tol), "Test failed.\n");
      }
   }

   return status;
}

} /* namespace PV */
