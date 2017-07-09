/**
 * This file tests weight initialization to a 2D Gaussian with sigma = 1.0 and normalized to 1.0
 * Test compares behavior of sharedWeights=false to that of sharedWeights=true.
 * assumes sharedWeights=true (the old kernelconn class) produces correct 2D Gaussian weights
 */

#undef DEBUG_PRINT

#include "connections/HyPerConn.hpp"
#include "io/io.hpp"
#include "layers/HyPerLayer.hpp"
#include <utils/PVLog.hpp>

using namespace PV;

int check_kernel_vs_hyper(HyPerConn *cHyPer, HyPerConn *cKernel, int kPre, int axonID);

int main(int argc, char *argv[]) {
   PV_Init *initObj = new PV_Init(&argc, &argv, false /*allowUnrecognizedArguments*/);
   if (initObj->getParams() == nullptr) {
      initObj->setParams("input/test_gauss2d.params");
   }

   char const *pre_layer_name   = "test_gauss2d_pre";
   char const *post_layer_name  = "test_gauss2d_post";
   char const *pre2_layer_name  = "test_gauss2d_pre2";
   char const *post2_layer_name = "test_gauss2d_post2";
   char const *hyper1to1_name   = "test_gauss2d_hyperconn";
   char const *kernel1to1_name  = "test_gauss2d_kernelconn";
   char const *hyper1to2_name   = "test_gauss2d_hyperconn";
   char const *kernel1to2_name  = "test_gauss2d_kernelconn";
   char const *hyper2to1_name   = "test_gauss2d_hyperconn";
   char const *kernel2to1_name  = "test_gauss2d_kernelconn";

   PV::HyPerCol *hc    = new PV::HyPerCol(initObj);
   PV::HyPerLayer *pre = dynamic_cast<PV::HyPerLayer *>(hc->getLayerFromName(pre_layer_name));
   FatalIf(!(pre), "Test failed.\n");
   PV::HyPerLayer *post = dynamic_cast<PV::HyPerLayer *>(hc->getLayerFromName(post_layer_name));
   FatalIf(!(post), "Test failed.\n");

   PV::HyPerConn *cHyPer = dynamic_cast<HyPerConn *>(hc->getConnFromName(hyper1to1_name));

   PV::HyPerConn *cKernel = dynamic_cast<HyPerConn *>(hc->getConnFromName(kernel1to1_name));

   PV::HyPerLayer *pre2 = dynamic_cast<PV::HyPerLayer *>(hc->getLayerFromName(pre2_layer_name));
   FatalIf(!(pre2), "Test failed.\n");
   PV::HyPerLayer *post2 = dynamic_cast<PV::HyPerLayer *>(hc->getLayerFromName(post2_layer_name));
   FatalIf(!(post2), "Test failed.\n");

   PV::HyPerConn *cHyPer1to2 = dynamic_cast<HyPerConn *>(hc->getConnFromName(hyper1to2_name));
   FatalIf(!(cHyPer1to2), "Test failed.\n");

   PV::HyPerConn *cKernel1to2 = dynamic_cast<HyPerConn *>(hc->getConnFromName(kernel1to2_name));
   FatalIf(!(cKernel1to2), "Test failed.\n");

   PV::HyPerConn *cHyPer2to1 = dynamic_cast<HyPerConn *>(hc->getConnFromName(hyper2to1_name));
   FatalIf(!(cHyPer2to1), "Test failed.\n");

   PV::HyPerConn *cKernel2to1 = dynamic_cast<HyPerConn *>(hc->getConnFromName(kernel2to1_name));
   FatalIf(!(cKernel2to1), "Test failed.\n");

   int status = 0;

   ensureDirExists(hc->getCommunicator()->getLocalMPIBlock(), hc->getOutputPath());

   auto objectMap      = hc->copyObjectMap();
   auto commMessagePtr = std::make_shared<CommunicateInitInfoMessage>(*objectMap);
   for (int l = 0; l < hc->numberOfLayers(); l++) {
      HyPerLayer *layer = hc->getLayer(l);
      int status        = layer->respond(commMessagePtr);
      FatalIf(!(status == PV_SUCCESS), "Test failed.\n");
   }
   for (int c = 0; c < hc->numberOfConnections(); c++) {
      BaseConnection *conn = hc->getConnection(c);
      int status           = conn->respond(commMessagePtr);
      FatalIf(!(status == PV_SUCCESS), "Test failed.\n");
   }
   delete objectMap;

   auto allocateMessagePtr = std::make_shared<AllocateDataMessage>();
   for (int l = 0; l < hc->numberOfLayers(); l++) {
      HyPerLayer *layer = hc->getLayer(l);
      int status        = layer->respond(allocateMessagePtr);
      FatalIf(!(status == PV_SUCCESS), "Test failed.\n");
   }

   for (int c = 0; c < hc->numberOfConnections(); c++) {
      BaseConnection *conn = hc->getConnection(c);
      int status           = conn->respond(allocateMessagePtr);
      FatalIf(!(status == PV_SUCCESS), "Test failed.\n");
   }

   const int axonID     = 0;
   int num_pre_extended = pre->clayer->numExtended;
   FatalIf(!(num_pre_extended == cHyPer->getNumWeightPatches()), "Test failed.\n");

   for (int kPre = 0; kPre < num_pre_extended; kPre++) {
      status = check_kernel_vs_hyper(cHyPer, cKernel, kPre, axonID);
      FatalIf(!(status == 0), "Test failed.\n");
      status = check_kernel_vs_hyper(cHyPer1to2, cKernel1to2, kPre, axonID);
      FatalIf(!(status == 0), "Test failed.\n");
      status = check_kernel_vs_hyper(cHyPer2to1, cKernel2to1, kPre, axonID);
      FatalIf(!(status == 0), "Test failed.\n");
   }

   delete hc;
   delete initObj;
   return 0;
}

int check_kernel_vs_hyper(HyPerConn *cHyPer, HyPerConn *cKernel, int kPre, int axonID) {
   FatalIf(!(cKernel->usingSharedWeights() == true), "Test failed.\n");
   FatalIf(!(cHyPer->usingSharedWeights() == false), "Test failed.\n");
   int status           = 0;
   PVPatch *hyperPatch  = cHyPer->getWeights(kPre, axonID);
   PVPatch *kernelPatch = cKernel->getWeights(kPre, axonID);
   int hyPerDataIndex   = cHyPer->patchIndexToDataIndex(kPre);
   int kernelDataIndex  = cKernel->patchIndexToDataIndex(kPre);

   int nk = cHyPer->fPatchSize() * (int)hyperPatch->nx;
   FatalIf(!(nk == (cKernel->fPatchSize() * (int)kernelPatch->nx)), "Test failed.\n");
   int ny = hyperPatch->ny;
   FatalIf(!(ny == kernelPatch->ny), "Test failed.\n");
   int sy = cHyPer->yPatchStride();
   FatalIf(!(sy == cKernel->yPatchStride()), "Test failed.\n");
   float *hyperWeights  = cHyPer->get_wData(axonID, hyPerDataIndex);
   float *kernelWeights = cKernel->get_wDataHead(axonID, kernelDataIndex) + hyperPatch->offset;
   float test_cond      = 0.0f;
   for (int y = 0; y < ny; y++) {
      for (int k = 0; k < nk; k++) {
         test_cond = kernelWeights[k] - hyperWeights[k];
         if (fabsf(test_cond) > 0.001f) {
            Fatal(errorMessage);
            errorMessage.printf("y %d\n", y);
            errorMessage.printf("k %d\n", k);
            errorMessage.printf("kernelweight %f\n", (double)kernelWeights[k]);
            errorMessage.printf("hyperWeights %f\n", (double)hyperWeights[k]);
            const char *cHyper_filename = "gauss2d_hyper.txt";
            cHyPer->writeTextWeights(cHyper_filename, false /*verifyWrites*/, kPre);
            const char *cKernel_filename = "gauss2d_kernel.txt";
            cKernel->writeTextWeights(cKernel_filename, false /*verifyWrites*/, kPre);
            status = 1;
         }
      }
      // advance pointers in y
      hyperWeights += sy;
      kernelWeights += sy;
   }
   return status;
}
