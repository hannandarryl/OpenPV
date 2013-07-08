#!/bin/bash
MPIHOME=/usr/lib64/openmpi
MPIRUN=${MPIHOME}/bin/mpirun
BASEDIR=/home/gkenyon/workspace/HyPerHLCA2
COMMAND="${BASEDIR}/Debug/HyPerHLCA2 -p ${BASEDIR}/input/HyPerHLCA_vine_color_deep.params -rows 12 -columns 10"
LOGDIR=/nh/compneuro/Data/vine/LCA/2013_01_31/output_12x12x512_lambda_05X2_color_deep
LOGFILE=${LOGDIR}/LCA_vine_12x12x512_lambda_05X2_color_deep.log
mkdir -p ${LOGDIR}
touch ${LOGFILE}
echo ${LOGFILE}
time ${MPIRUN} --bynode --hostfile ~/.mpi_hosts -np 120 ${COMMAND} &> ${LOGFILE}
