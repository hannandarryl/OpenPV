/*
 * HyPerConnCheckpointerProbe.hpp
 *
 *  Created on: Jan 5, 2017
 *      Author: pschultz
 */

#ifndef POOLINGCONNCHECKPOINTERTESTPROBE_HPP_
#define POOLINGCONNCHECKPOINTERTESTPROBE_HPP_

#include "probes/ColProbe.hpp"

#include "CorrectState.hpp"
#include "connections/HyPerConn.hpp"
#include "layers/HyPerLayer.hpp"
#include "layers/InputLayer.hpp"

class PoolingConnCheckpointerTestProbe : public PV::ColProbe {
  public:
   /**
    * Public constructor for the PoolingConnCheckpointerTestProbe class.
    */
   PoolingConnCheckpointerTestProbe(const char *probeName, PV::HyPerCol *hc);

   /**
    * Destructor for the PoolingConnCheckpointerTestProbe class.
    */
   virtual ~PoolingConnCheckpointerTestProbe();

   /**
    * Implementation of the calcValues method. This probe does not compute
    * any values that are available to other objects through getValues().
    */
   virtual int calcValues(double timevalue) { return PV_SUCCESS; }

   bool getTestFailed() const { return mTestFailed; }

  protected:
   int initialize(const char *probeName, PV::HyPerCol *hc);
   virtual void ioParam_textOutputFlag(enum PV::ParamsIOFlag ioFlag) override;
   virtual int communicateInitInfo(PV::CommunicateInitInfoMessage const *message) override;
   virtual int readStateFromCheckpoint(PV::Checkpointer *checkpointer) override;
   virtual bool needRecalc(double timevalue) override { return true; }
   virtual double referenceUpdateTime() const override { return parent->simulationTime(); }
   virtual int outputState(double timevalue);

  private:
   PoolingConnCheckpointerTestProbe();
   int initialize_base();

   /**
    * Sets the input layer data member, and checks that the input layer's parameters are
    * consistent with those expected by the test. Returns either PV_SUCCESS or PV_POSTPONE.
    */
   int initInputLayer(PV::CommunicateInitInfoMessage const *message);

   /**
    * Sets the output layer data member, and checks that the output layer's parameters are
    * consistent with those expected by the test. Returns either PV_SUCCESS or PV_POSTPONE.
    */
   int initOutputLayer(PV::CommunicateInitInfoMessage const *message);

   /**
    * Sets the connection data member, and checks that the connection's parameters are
    * consistent with those expected by the test. Returns either PV_SUCCESS or PV_POSTPONE.
    */
   int initConnection(PV::CommunicateInitInfoMessage const *message);

   /**
    * Checks whether the given object has finished its communicateInitInfo stage, and
    * returns PV_SUCCESS if it has, or PV_POSTPONE if it has not.
    */
   int checkCommunicatedFlag(PV::BaseObject *dependencyObject);

   int calcUpdateNumber(double timevalue);
   void initializeCorrectValues(double timevalue);

   bool verifyLayer(
         PV::HyPerLayer *layer,
         PV::Buffer<float> const &correctValueBuffer,
         double timevalue);

   // Data members
  protected:
   int mStartingUpdateNumber    = 0;
   bool mValuesSet              = false;
   PV::InputLayer *mInputLayer  = nullptr;
   PV::HyPerLayer *mOutputLayer = nullptr;
   PV::HyPerConn *mConnection   = nullptr;
   CorrectState *mCorrectState  = nullptr;
   bool mTestFailed             = false;
};

#endif // POOLINGCONNCHECKPOINTERTESTPROBE_HPP_
