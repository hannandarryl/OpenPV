/*
 * CheckpointEntryDataStore.hpp
 *
 *  Created on Oct 13, 2016
 *      Author: Pete Schultz
 */

#ifndef CHECKPOINTENTRYDATASTORE_HPP_
#define CHECKPOINTENTRYDATASTORE_HPP_

#include "checkpointing/CheckpointEntry.hpp"
#include "columns/DataStore.hpp"
#include <string>

namespace PV {

class CheckpointEntryDataStore : public CheckpointEntry {
  public:
   CheckpointEntryDataStore(
         std::string const &name,
         MPIBlock const *mpiBlock,
         DataStore *dataStore,
         PVLayerLoc const *layerLoc)
         : CheckpointEntry(name, mpiBlock) {
      initialize(dataStore, layerLoc);
   }
   CheckpointEntryDataStore(
         std::string const &objName,
         std::string const &dataName,
         MPIBlock const *mpiBlock,
         DataStore *dataStore,
         PVLayerLoc const *layerLoc)
         : CheckpointEntry(objName, dataName, mpiBlock) {
      initialize(dataStore, layerLoc);
   }
   virtual void write(std::string const &checkpointDirectory, double simTime, bool verifyWritesFlag)
         const override;
   virtual void read(std::string const &checkpointDirectory, double *simTimePtr) const override;
   virtual void remove(std::string const &checkpointDirectory) const override;

  protected:
   void initialize(DataStore *dataStore, PVLayerLoc const *layerLoc);

  private:
   DataStore *mDataStore;
   PVLayerLoc const *mLayerLoc = nullptr;
};

} // end namespace PV

#endif // CHECKPOINTENTRYDATASTORE_HPP_
