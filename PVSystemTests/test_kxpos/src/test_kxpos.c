#include <utils/conversions.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
   int kl;

   int nf = 3;
   int nx = 64;
   int ny = 128;

   float nxLocal = nx;
   float nyLocal = ny;

   int inx = nx;
   int iny = ny;

   for (kl = 0; kl < inx*iny*nf; kl++) {
#ifdef FEATURES_LAST
      int kxx = kl % inx;
#else
      int y   = kl/(nf*nx);
      int kxx = (kl - y*nx*nf) / nf;
#endif
      float kx = kxPos(kl, nxLocal, nyLocal, nf);

      if (kx != (float)kxx) {
         printf("FAILED:TEST_KXPOS: (k,kx) = (%d,%f)\n", kl, kx);
         exit(1);
      }
   }

   nx = 1009;
   ny = 5;
   nxLocal = nx;
   nyLocal = ny;

   inx = nx;
   iny = ny;

   for (kl = 0; kl < inx*iny*nf; kl++) {
#ifdef FEATURES_LAST
      int kxx = kl % inx;
#else
      int y   = kl/(nf*nx);
      int kxx = (kl - y*nx*nf) / nf;
#endif
      float kx = kxPos(kl, nxLocal, nyLocal, nf);

      if ((int)kx-kxx != 0) {
         printf("FAILED:TEST_KXPOS: (k,kx) = (%d,%f)\n", kl, kx);
         exit(1);
      }
   }

   nf = 4;
   nx = 107;
   ny = 5;
   nxLocal = nx;
   nyLocal = ny;

   inx = nx;
   iny = ny;

   for (kl = 0; kl < inx*iny*nf; kl++) {
#ifdef FEATURES_LAST
      int kxx = kl % inx;
#else
      int y   = kl/(nf*nx);
      int kxx = (kl - y*nx*nf) / nf;
#endif
      float kx = kxPos(kl, nxLocal, nyLocal, nf);

      if ((int)kx-kxx != 0) {
         printf("FAILED:TEST_KXPOS: (k,kx) = (%d,%f)\n", kl, kx);
         exit(1);
      }
   }

   nf = 1;
   nx = 16777216+1;  // this should fail
   nx = 16777216;
   ny = 1;
   nxLocal  = nx;
   nyLocal  = ny;

   inx = nx;
   iny = ny;

   for (kl = 0; kl < inx*iny*nf; kl++) {
#ifdef FEATURES_LAST
      int kxx = kl % inx;
#else
      int y   = kl/(nf*nx);
      int kxx = (kl - y*nx*nf) / nf;
#endif
      float kx = kxPos(kl, nxLocal, nyLocal, nf);

      if ((int)kx-kxx != 0) {
         printf("FAILED:TEST_KXPOS: (k,kx) = (%d,%f)\n", kl, kx);
         exit(1);
      }
   }

   return 0;
}