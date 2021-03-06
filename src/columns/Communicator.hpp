/*
 * Communicator.hpp
 */

#ifndef COMMUNICATOR_HPP_
#define COMMUNICATOR_HPP_

#include "Arguments.hpp"
#include "include/pv_arch.h"
#include "include/pv_types.h"
#include "structures/MPIBlock.hpp"
#include <cstdio>
#include <vector>

#include "arch/mpi/mpi.h"

#define COMMNAME_MAXLENGTH 16

namespace PV {

class Communicator {
  public:
   // The following Communicator methods related to border exchange were moved to
   // the BorderExchange class in utils/BorderExchange.{c,h}pp Feb 6, 2017.
   //     newDatatypes
   //     freeDatatypes
   //     exchange
   //     wait
   //     recvOffset
   //     sendOffset

   Communicator(Arguments *argumentList);
   virtual ~Communicator();

   // Previous names of MPI getter functions now default to local ranks and sizes
   int commRank() { return localRank; }
   int globalCommRank() { return globalRank; }
   int commSize() { return localSize; }
   int globalCommSize() { return globalSize; }

   MPI_Comm communicator() const { return localMPIBlock->getComm(); }
   MPI_Comm batchCommunicator() const { return batchMPIBlock->getComm(); }
   MPI_Comm globalCommunicator() const { return globalMPIBlock->getComm(); }

   MPIBlock const *getLocalMPIBlock() const { return localMPIBlock; }
   MPIBlock const *getBatchMPIBlock() const { return batchMPIBlock; }
   MPIBlock const *getGlobalMPIBlock() const { return globalMPIBlock; }

   int numberOfNeighbors(); // includes interior (self) as a neighbor

   bool hasNeighbor(int neighborId);
   int neighborIndex(int commId, int index);
   int reverseDirection(int commId, int direction);

   int commRow() { return commRow(localRank); }
   int commColumn() { return commColumn(localRank); }
   int commBatch() { return commBatch(globalRank); }
   int numCommRows() { return numRows; }
   int numCommColumns() { return numCols; }
   int numCommBatches() { return batchWidth; }

   int getTag(int neighbor) { return tags[neighbor]; }
   int getReverseTag(int neighbor) { return tags[reverseDirection(localRank, neighbor)]; }

   bool isExtraProc() { return isExtra; }

   static const int LOCAL     = 0;
   static const int NORTHWEST = 1;
   static const int NORTH     = 2;
   static const int NORTHEAST = 3;
   static const int WEST      = 4;
   static const int EAST      = 5;
   static const int SOUTHWEST = 6;
   static const int SOUTH     = 7;
   static const int SOUTHEAST = 8;

  protected:
   int commRow(int commId);
   int commColumn(int commId);
   int commBatch(int commId);
   int commIdFromRowColumn(int commRow, int commColumn);

   int numNeighbors; // # of remote neighbors plus local.  NOT the size of the
   // neighbors array,
   // which uses negative values to mark directions where there is no remote
   // neighbor.

   bool isExtra; // Defines if the process is an extra process

   int neighbors[NUM_NEIGHBORHOOD]; // [0] is interior (local)
   int tags[NUM_NEIGHBORHOOD]; // diagonal communication needs a different tag
   int exchangeCounter = 1024;
   // from left/right or
   // up/down communication.

  private:
   int gcd(int a, int b);

   int localRank;
   int localSize;
   int globalRank;
   int globalSize;
   int batchRank;
   int numRows;
   int numCols;
   int batchWidth;

   MPIBlock *localMPIBlock  = nullptr;
   MPIBlock *batchMPIBlock  = nullptr;
   MPIBlock *globalMPIBlock = nullptr;

   // These methods are private for now, move to public as needed

   int neighborInit();

   bool hasNorthwesternNeighbor(int commRow, int commColumn);
   bool hasNorthernNeighbor(int commRow, int commColumn);
   bool hasNortheasternNeighbor(int commRow, int commColumn);
   bool hasWesternNeighbor(int commRow, int commColumn);
   bool hasEasternNeighbor(int commRow, int commColumn);
   bool hasSouthwesternNeighbor(int commRow, int commColumn);
   bool hasSouthernNeighbor(int commRow, int commColumn);
   bool hasSoutheasternNeighbor(int commRow, int commColumn);

   int northwest(int commRow, int commColumn);
   int north(int commRow, int commColumn);
   int northeast(int commRow, int commColumn);
   int west(int commRow, int commColumn);
   int east(int commRow, int commColumn);
   int southwest(int commRow, int commColumn);
   int south(int commRow, int commColumn);
   int southeast(int commRow, int commColumn);
};

} // namespace PV

#endif /* COMMUNICATOR_HPP_ */
