/*
 * buildandrun.hpp
 *
 *  Created on: May 27, 2011
 *      Author: peteschultz
 */

#ifndef BUILDANDRUN_HPP_
#define BUILDANDRUN_HPP_

#include <time.h>
#include <string>
#include <iostream>

#include "../include/pv_common.h"

#include "../columns/HyPerCol.hpp"

#include "../layers/HyPerLayer.hpp"
#include "../layers/ANNLayer.hpp"
#include "../layers/GenerativeLayer.hpp"
#include "../layers/IncrementLayer.hpp"
#include "../layers/LeakyIntegrator.hpp"
#include "../layers/LogLatWTAGenLayer.hpp"
#ifdef OBSOLETE // Marked obsolete Dec 2, 2014.  No longer used.
#include "../layers/PursuitLayer.hpp"
#endif // OBSOLETE
#include "../layers/MatchingPursuitResidual.hpp"
#include "../layers/PoolingANNLayer.hpp"
#include "../layers/PtwiseProductLayer.hpp"
#include "../layers/TrainingLayer.hpp"
#include "../layers/CloneVLayer.hpp"
#include "../layers/BinningLayer.hpp"
#include "../layers/WTALayer.hpp"
#include "../layers/GapLayer.hpp"
#include "../layers/TextStream.hpp"
#ifdef PV_USE_SNDFILE
#include "../layers/SoundStream.hpp"
#include "../layers/NewCochlear.h"
#endif
#include "../layers/Image.hpp"
#include "../layers/CreateMovies.hpp"
#include "../layers/ImageFromMemoryBuffer.hpp"
#include "../layers/Movie.hpp"
#include "../layers/Patterns.hpp"
#include "../layers/LabelLayer.hpp"
#include "../layers/LIF.hpp"
#include "../layers/LIFGap.hpp"
#include "../layers/MatchingPursuitLayer.hpp"
#include "../layers/Retina.hpp"
#include "../layers/SigmoidLayer.hpp"
#include "../layers/MLPSigmoidLayer.hpp"
#include "../layers/RescaleLayer.hpp"
#include "../layers/ShuffleLayer.hpp"
#include "../layers/ANNSquaredLayer.hpp"
#include "../layers/ANNWhitenedLayer.hpp"
#include "../layers/BIDSLayer.hpp"
#include "../layers/BIDSCloneLayer.hpp"
#include "../layers/BIDSMovieCloneMap.hpp"
#include "../layers/BIDSSensorLayer.hpp"
#include "../layers/LCALIFLayer.hpp"
#include "../layers/HyPerLCALayer.hpp"
#include "../layers/ANNErrorLayer.hpp"
#include "../layers/ANNNormalizedErrorLayer.hpp"
#include "../layers/MLPErrorLayer.hpp"
#include "../layers/MLPForwardLayer.hpp"
#include "../layers/MLPOutputLayer.hpp"
#include "../layers/LabelErrorLayer.hpp"
#include "../layers/KmeansLayer.hpp"
#ifdef OBSOLETE // Marked obsolete Dec 29, 2014.  Removing several long-unused layers.
#include "../layers/ANNDivInh.hpp"
#include "../layers/ANNLabelLayer.hpp"
#include "../layers/ANNWeightedErrorLayer.hpp"
#include "../layers/AccumulateLayer.hpp"
#include "../layers/CliqueLayer.hpp"
#endif // OBSOLETE
#ifdef OBSOLETE // Marked obsolete April 23, 2014.
// Use ANNLayer with triggerFlag set to true and triggerLayerName for the triggering layer
#include "../layers/ANNTriggerUpdateOnNewImageLayer.hpp"
#endif // OBSOLETE
#include "../layers/ConstantLayer.hpp"
#include "../layers/FilenameParsingGroundTruthLayer.hpp"
#include "../connections/BaseConnection.hpp"
#include "../connections/HyPerConn.hpp"
#include "../connections/BIDSConn.hpp"
#include "../connections/CloneConn.hpp"
#include "../connections/PlasticCloneConn.hpp"
#include "../connections/CopyConn.hpp"
#include "../connections/KernelConn.hpp"
#include "../connections/CloneKernelConn.hpp"
#include "../connections/GapConn.hpp"
#ifdef OBSOLETE // Marked obsolete Nov 25, 2014.  Use HyPerConn instead of GenerativeConn and PoolingConn instead of PoolingGenConn
#include "../connections/GenerativeConn.hpp"
#include "../connections/PoolingGenConn.hpp"
#endif // OBSOLETE
#include "../connections/IdentConn.hpp"
#ifdef OBSOLETE // Marked obsolete Oct 20, 2014.  Normalizers are being generalized to allow for group normalization
#include "../connections/NoSelfKernelConn.hpp"
#include "../connections/SiblingConn.hpp"
#endif // OBSOLETE
#ifdef OBSOLETE // Marked obsolete Nov 25, 2014.  No longer used.
#include "../connections/ReciprocalConn.hpp"
#endif // OBSOLETE
#include "../connections/TransposeConn.hpp"
#include "../connections/FeedbackConn.hpp"
#include "../connections/LCALIFLateralConn.hpp"
#include "../connections/OjaSTDPConn.hpp"
#include "../connections/PoolingConn.hpp"
#ifdef OBSOLETE // Marked obsolete Dec 2, 2014.  Use sharedWeights=false instead of windowing.
#include "../connections/WindowConn.hpp"
#endif // OBSOLETE
#ifdef OBSOLETE // Marked obsolete Dec 29, 2014.  Removing several long-unused connections.
#include "../connections/CliqueConn.hpp"
#include "../connections/InhibSTDPConn.hpp"
#include "../connections/LCALIFLateralKernelConn.hpp"
#include "../connections/MapReduceKernelConn.hpp"
#include "../connections/OjaKernelConn.hpp"
#include "../connections/STDP3Conn.hpp"
#include "../connections/STDPConn.hpp"
#endif // OBSOLETE

#include "../weightinit/InitWeights.hpp"
#include "../weightinit/InitGauss2DWeights.hpp"
#include "../weightinit/InitCocircWeights.hpp"
#include "../weightinit/InitSmartWeights.hpp"
#include "../weightinit/InitUniformRandomWeights.hpp"
#include "../weightinit/InitGaussianRandomWeights.hpp"
#include "../weightinit/InitGaborWeights.hpp"
#include "../weightinit/InitBIDSLateral.hpp"
#include "../weightinit/InitOneToOneWeights.hpp"
#include "../weightinit/InitOneToOneWeightsWithDelays.hpp"
#include "../weightinit/InitIdentWeights.hpp"
#include "../weightinit/InitUniformWeights.hpp"
#include "../weightinit/InitSpreadOverArborsWeights.hpp"
#ifdef OBSOLETE // Marked obsolete Dec. 29, 2014.  Removing several long-unused weight init methods
#include "../weightinit/Init3DGaussWeights.hpp"
#include "../weightinit/InitByArborWeights.hpp"
#include "../weightinit/InitDistributedWeights.hpp"
#include "../weightinit/InitMTWeights.hpp"
#include "../weightinit/InitPoolWeights.hpp"
#include "../weightinit/InitRuleWeights.hpp"
#include "../weightinit/InitSubUnitWeights.hpp"
#include "../weightinit/InitWindowed3DGaussWeights.hpp"
#endif // OBSOLETE

#include "../io/BaseConnectionProbe.hpp"
#include "../io/ConnStatsProbe.hpp"
#ifdef OBSOLETE // Marked obsolete Nov 25, 2014.  No longer used.
#include "../io/ReciprocalEnergyProbe.hpp"
#endif // OBSOLETE
#include "../io/KernelProbe.hpp"
#include "../io/LCALIFLateralProbe.hpp"
#include "../io/OjaConnProbe.hpp"
#include "../io/PatchProbe.hpp"
#include "../io/TextStreamProbe.hpp"
#include "../io/LayerProbe.hpp"
#include "../io/PointProbe.hpp"
#include "../io/PointLCALIFProbe.hpp"
#include "../io/PointLIFProbe.hpp"
#include "../io/StatsProbe.hpp"
#include "../io/SparsityLayerProbe.hpp"
#include "../io/L2NormProbe.hpp"
#include "../io/SparsityTermProbe.hpp"
#include "../io/LogLatWTAProbe.hpp"
#include "../io/RequireAllZeroActivityProbe.hpp"
#include "../io/GenColProbe.hpp"
#ifdef OBSOLETE // Marked obsolete Dec 29, 2014.  Removing several long-unused probes.
#include "../io/OjaKernelSpikeRateProbe.hpp"
#endif // OBSOLETE

using namespace PV;

int buildandrun(int argc, char * argv[],
                int (*custominit)(HyPerCol *, int, char **) = NULL,
                int (*customexit)(HyPerCol *, int, char **) = NULL,
                void * (*customgroups)(const char *, const char *, HyPerCol *) = NULL);
int buildandrun1paramset(int argc, char * argv[],
                         int (*custominit)(HyPerCol *, int, char **),
                         int (*customexit)(HyPerCol *, int, char **),
                         void * (*customgroups)(const char *, const char *, HyPerCol *),
                         PVParams * params);
HyPerCol * build(int argc, char * argv[], void * (*customgroups)(const char *, const char *, HyPerCol *) = NULL, PVParams * params = NULL);

HyPerCol * addHyPerColToColumn(const char * classkeyword, const char * name, HyPerCol * hc);
HyPerLayer * addLayerToColumn(const char * classkeyword, const char * name, HyPerCol * hc);
HyPerConn * addConnToColumn(const char * classkeyword, const char * name, HyPerCol * hc);
ColProbe * addColProbeToColumn(const char * classkeyword, const char * name, HyPerCol * hc);
void insertColProbe(ColProbe * colProbe, HyPerCol * hc);
BaseConnectionProbe * addBaseConnectionProbeToColumn(const char * classkeyword, const char * name, HyPerCol * hc);
const char * getStringValueFromParameterGroup(const char * groupName, PVParams * params, const char * parameterString, bool warnIfAbsent);
HyPerLayer * getLayerFromParameterGroup(const char * groupName, HyPerCol * hc, const char * parameterStringName, bool warnIfAbsent=true);
BaseConnection * getConnFromParameterGroup(const char * groupName, HyPerCol * hc, const char * parameterStringName, bool warnIfAbesnt=true);
LayerProbe * addLayerProbeToColumn(const char * classkeyword, const char * name, HyPerCol * hc);
int getLayerFunctionProbeParameters(const char * name, const char * keyword, HyPerCol * hc, HyPerLayer ** targetLayer, char ** message, const char ** filename);
int decodeChannel(int channel, ChannelType * channelType);
int checknewobject(void * object, const char * kw, const char * name, HyPerCol * hc);

#endif /* BUILDANDRUN_HPP_ */
