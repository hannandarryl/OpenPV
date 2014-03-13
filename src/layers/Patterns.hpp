/*
 * Patterns.hpp
 *
 *  Created on: April 21, 2010
 *      Author: Craig Rasmussen
 */

#ifndef PATTERNS_HPP_
#define PATTERNS_HPP_

#include "Image.hpp"
#include <vector>
namespace PV {

enum PatternType {
  BARS  = 0,
  RECTANGLES  = 1,
  SINEWAVE  = 2,
  COSWAVE  = 3,
  IMPULSE  = 4,
  SINEV  = 5,
  COSV  = 6,
  DROP = 7,
};

enum OrientationMode {
   horizontal = 0,
   vertical = 1,
   mixed = 2,
};

enum MovementType {
   RANDOMWALK = 0,
   MOVEFORWARD = 1,
   MOVEBACKWARD = 2,
   RANDOMJUMP = 3,
};

#define DROPPADDINGSIZE (sizeof(int)-sizeof(bool))

typedef struct _Drop{
   int centerX;
   int centerY;
   float speed;
   float radius;
   bool on;
   char padding[DROPPADDINGSIZE];
} Drop;

class Patterns : public PV::Image {
public:
   Patterns(const char * name, HyPerCol * hc, PatternType type);
   virtual ~Patterns();
   virtual int communicateInitInfo();
   virtual int allocateDataStructures();
   //virtual bool needUpdate(double timef, double dt);
   virtual int updateState(double timef, double dt);

   void setProbMove(float p)     {pMove = p;}
   void setProbSwitch(float p)   {pSwitch = p;}

   void setMinWidth(int w)  {minWidth  = w;}
   void setMaxWidth(int w)  {maxWidth  = w;}
   void setMinHeight(int h) {minHeight = h;}
   void setMaxHeight(int h) {maxHeight = h;}

   virtual int tag();

   int checkpointRead(const char * cpDir, double * timef);
   int checkpointWrite(const char * cpDir);

protected:

   Patterns();
   int initialize(const char * name, HyPerCol * hc, PatternType type);
   OrientationMode readOrientation();
   int setOrientation(OrientationMode ormode);
   MovementType readMovementType();
   int drawPattern(float val);
   int drawBars(OrientationMode ormode, pvdata_t * buf, int nx, int ny, float val);
   int drawRectangles(float val);
   int drawWaves(float val);
   int drawImpulse();
   int drawDrops();
   int updatePattern(double timef);
   float calcPosition(float pos, int step);
   virtual bool constrainBiases() {return false;}
   virtual bool constrainOffsets() {return false;}
   virtual double getDeltaUpdateTime();

   PatternType type;
   OrientationMode orientation;
   OrientationMode lastOrientation;
   MovementType movementType; //save the type of movement
                              //(random walk, horizontal or vertical drift
                              //or random jumping)

//   int writeImages;  // Base class Image already has member variable writeImages
   int writePosition;     // write positions to input/image-pos.txt
   float position;
   float pSwitch;
   float pMove;
   float movementSpeed; //save a movement speed in pixels/time step
   float positionBound; // The supremum of possible values of position
   double framenumber;

   std::vector <Drop>vDrops;
   float dropSpeed;
   float dropSpeedRandomMax;
   float dropSpeedRandomMin;
   int dropPeriod;
   int dropPeriodRandomMax;
   int dropPeriodRandomMin;
   double nextDropFrame;
   double nextPosChangeFrame;
   int xPos;
   int yPos;
   int onOffFlag;
   float inOut; //0 for random, -1 for all in drops, 1 for all out drops, and everything in between
   int dropPosition;
   int dropPositionRandomMax;
   int dropPositionRandomMin;
   double startFrame;
   double endFrame;

   int minWidth, maxWidth;
   int minHeight, maxHeight;
   int wavelengthVert;
   int wavelengthHoriz;
   float maxVal;
   float minVal;
   char * patternsOutputPath;  // path to output file directory for patterns

   double displayPeriod;   // length of time a frame is displayed
   //double nextDisplayTime; // time of next frame
   PV_Stream * patternsFile;

   Random * patternRandState; // RNG state for Patterns class.  Everything is done sequentially, so a Random(parent, 1) should be reproducible

private:
   float rotation;

   int initPatternCntr;
   int initialize_base();
};

}

#endif /* PATTERNS_HPP_ */
