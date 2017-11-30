/*
 * WeightsPair.hpp
 *
 *  Created on: Nov 17, 2017
 *      Author: Pete Schultz
 */

#ifndef WEIGHTSPAIR_HPP_
#define WEIGHTSPAIR_HPP_

#include "columns/BaseObject.hpp"
#include "components/ConnectionData.hpp"
#include "components/PostWeights.hpp"
#include "components/Weights.hpp"

namespace PV {

class WeightsPair : public BaseObject {
  protected:
   /**
    * List of parameters needed from the WeightsPair class
    * @name WeightsPair Parameters
    * @{
    */

   /**
    * @brief nxp: Specifies the x patch size
    * @details If one pre to many post, nxp restricted to many * an odd number
    * If many pre to one post or one pre to one post, nxp restricted to an odd number
    */
   virtual void ioParam_nxp(enum ParamsIOFlag ioFlag);

   /**
    * @brief nyp: Specifies the y patch size
    * @details If one pre to many post, nyp restricted to many * an odd number
    * If many pre to one post or one pre to one post, nyp restricted to an odd number
    */
   virtual void ioParam_nyp(enum ParamsIOFlag ioFlag);

   /**
    * @brief nfp: Specifies the post feature patch size
    */
   virtual void ioParam_nfp(enum ParamsIOFlag ioFlag);

   /**
    * @brief sharedWeights: Defines if the weights use shared weights
    */
   virtual void ioParam_sharedWeights(enum ParamsIOFlag ioFlag);
   /** @} */ // end of WeightsPair parameters

  public:
   WeightsPair(char const *name, HyPerCol *hc);

   virtual ~WeightsPair();

   int getPatchSizeX() const { return mPatchSizeX; }
   int getPatchSizeY() const { return mPatchSizeY; }
   int getPatchSizeF() const { return mPatchSizeF; }
   int getSharedWeights() const { return mSharedWeights; }

   Weights *getPreWeights() { return mPreWeights; }
   Weights *getPostWeights() { return mPostWeights; }

   virtual void needPre();
   virtual void needPost();

   virtual int allocateDataStructures() override; // TODO: move to protected;
   // this should be handled by observer pattern

  protected:
   WeightsPair() {}

   int initialize(char const *name, HyPerCol *hc);

   virtual int setDescription() override;

   int ioParamsFillGroup(enum ParamsIOFlag ioFlag);

   int communicateInitInfo(std::shared_ptr<CommunicateInitInfoMessage const> message) override;

  protected:
   int mPatchSizeX     = 0;
   int mPatchSizeY     = 0;
   int mPatchSizeF     = -1;
   bool mSharedWeights = false;

   ConnectionData *mConnectionData = nullptr;

   Weights *mPreWeights  = nullptr;
   Weights *mPostWeights = nullptr;
};

} // namespace PV

#endif // WEIGHTSPAIR_HPP_
