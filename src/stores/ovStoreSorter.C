
/******************************************************************************
 *
 *  This file is part of canu, a software program that assembles whole-genome
 *  sequencing reads into contigs.
 *
 *  This software is based on:
 *    'Celera Assembler' (http://wgs-assembler.sourceforge.net)
 *    the 'kmer package' (http://kmer.sourceforge.net)
 *  both originally distributed by Applera Corporation under the GNU General
 *  Public License, version 2.
 *
 *  Canu branched from Celera Assembler at its revision 4587.
 *  Canu branched from the kmer project at its revision 1994.
 *
 *  This file is derived from:
 *
 *    src/AS_OVS/overlapStoreSorter.C
 *
 *  Modifications by:
 *
 *    Brian P. Walenz from 2012-APR-02 to 2013-SEP-09
 *      are Copyright 2012-2013 J. Craig Venter Institute, and
 *      are subject to the GNU General Public License version 2
 *
 *    Brian P. Walenz from 2014-DEC-15 to 2015-SEP-21
 *      are Copyright 2014-2015 Battelle National Biodefense Institute, and
 *      are subject to the BSD 3-Clause License
 *
 *    Brian P. Walenz beginning on 2016-JAN-11
 *      are a 'United States Government Work', and
 *      are released in the public domain
 *
 *  File 'README.licenses' in the root directory of this distribution contains
 *  full conditions and disclaimers for each license.
 */

#include "AS_global.H"

#include "sqStore.H"
#include "ovStore.H"
#include "ovStoreConfig.H"

#include <algorithm>
using namespace std;


void
checkSentinel(const char *ovlName, uint32 sliceNum, ovStoreConfig *config) {
  char   N[FILENAME_MAX+1];

  //  Check if the user is a moron.

  if ((sliceNum == 0) ||
      (sliceNum > config->numSlices())) {
    fprintf(stderr, "No slice " F_U32 " exists; only slices 1-" F_U32 " exist.\n", sliceNum, config->numSlices());
    exit(1);
  }

  //  Check if done.

  snprintf(N, FILENAME_MAX, "%s/%04u.started", ovlName, sliceNum);

  if (AS_UTL_fileExists(N, true, false) == true) {
    fprintf(stderr, "Job (appears to be) in progress; sentinel file '%s' exists.\n", N);
    exit(1);
  }

  //  Not done and not running, so create a sentinel to say we're running.

  AS_UTL_createEmptyFile(N);
}



void
removeSentinel(const char *ovlName, uint32 sliceNum) {
  char   N[FILENAME_MAX+1];

  snprintf(N, FILENAME_MAX, "%s/%04u.started", ovlName, sliceNum);

  AS_UTL_unlink(N);
}



void
checkMemory(const char *ovlName, uint32 sliceNum, uint64 totOvl, uint64 maxMemory) {

  if (ovOverlapSortSize * totOvl > maxMemory) {
    fprintf(stderr, "ERROR:  Overlaps need %.2f GB memory, but process limited (via -M) to " F_U64 " GB.\n",
            ovOverlapSortSize * totOvl / 1024.0 / 1024.0 / 1024.0, maxMemory >> 30);
    removeSentinel(ovlName, sliceNum);
    exit(1);
  }

  fprintf(stderr, "\n");

  double  memUsed    = totOvl * ovOverlapSortSize / 1024.0 / 1024.0 / 1024.0;
  uint64  memAllowed = maxMemory >> 30;

  if (maxMemory == UINT64_MAX)
    fprintf(stderr, "Loading %10" F_U64P " overlaps using %.2f GB memory.\n", totOvl, memUsed);
  else
    fprintf(stderr, "Loading %10" F_U64P " overlaps using %.2f GB of allowed (-M) " F_U64 " GB memory.\n", totOvl, memUsed, memAllowed);
}







int
main(int argc, char **argv) {
  char           *ovlName      = NULL;
  char           *seqName      = NULL;
  char           *cfgName      = NULL;
  uint32          sliceNum     = UINT32_MAX;

  uint64          maxMemory    = UINT64_MAX;

  bool            deleteIntermediateEarly = false;
  bool            deleteIntermediateLate  = false;
  bool            forceRun = false;

  argc = AS_configure(argc, argv);

  vector<char *>  err;
  int             arg=1;
  while (arg < argc) {
    if        (strcmp(argv[arg], "-O") == 0) {
      ovlName = argv[++arg];

    } else if (strcmp(argv[arg], "-S") == 0) {
      seqName = argv[++arg];

    } else if (strcmp(argv[arg], "-C") == 0) {
      cfgName = argv[++arg];

    } else if (strcmp(argv[arg], "-s") == 0) {
      sliceNum  = atoi(argv[++arg]);

    } else if (strcmp(argv[arg], "-M") == 0) {
      maxMemory  = (uint64)ceil(atof(argv[++arg]) * 1024.0 * 1024.0 * 1024.0);

    } else if (strcmp(argv[arg], "-deleteearly") == 0) {
      deleteIntermediateEarly = true;

    } else if (strcmp(argv[arg], "-deletelate") == 0) {
      deleteIntermediateLate  = true;

    } else if (strcmp(argv[arg], "-force") == 0) {
      forceRun = true;

    } else {
      char *s = new char [1024];
      snprintf(s, 1024, "%s: unknown option '%s'.\n", argv[0], argv[arg]);
      err.push_back(s);
    }

    arg++;
  }

  if (ovlName == NULL)
    err.push_back("ERROR: No overlap store (-O) supplied.\n");

  if (sliceNum == UINT32_MAX)
    err.push_back("ERROR: no slice number (-F) supplied.\n");

  if (maxMemory < OVSTORE_MEMORY_OVERHEAD + ovOverlapSortSize)
    fprintf(stderr, "ERROR: Memory (-M) must be at least 0.25 GB to account for overhead.\n");  //  , OVSTORE_MEMORY_OVERHEAD / 1024.0 / 1024.0 / 1024.0

  if (err.size() > 0) {
    fprintf(stderr, "usage: %s -O asm.ovlStore -S asm.seqStore -C ovStoreConfig -s slice [opts]\n", argv[0]);
    fprintf(stderr, "  -O asm.ovlStore       path to overlap store to create\n");
    fprintf(stderr, "  -S asm.seqStore       path to sequence store\n");
    fprintf(stderr, "  -C config             path to ovStoreConfig configuration file\n");
    fprintf(stderr, "  -s slice              slice to process (1 ... N)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -M m             maximum memory to use, in gigabytes\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -deleteearly     remove intermediates as soon as possible (unsafe)\n");
    fprintf(stderr, "  -deletelate      remove intermediates when outputs exist (safe)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -force           force a recompute, even if the output exists\n");
    fprintf(stderr, "\n");

    for (uint32 ii=0; ii<err.size(); ii++)
      if (err[ii])
        fputs(err[ii], stderr);

    exit(1);
  }

  //  Load the config.

  ovStoreConfig  *config = new ovStoreConfig(cfgName);

  //  Check if the sentinel exists (and if the user is a moron).

  checkSentinel(ovlName, sliceNum, config);

  //  Not done.  Let's go!

  sqStore             *seq    = sqStore::sqStore_open(seqName);
  ovStoreSliceWriter  *writer = new ovStoreSliceWriter(ovlName, seq, sliceNum, config->numSlices(), config->numBuckets());

  //  Get the number of overlaps in each bucket slice.

  fprintf(stderr, "\n");
  fprintf(stderr, "Finding overlaps.\n");

  uint64 *bucketSizes = new uint64 [config->numBuckets() + 1];     //  The number of overlaps in bucket i
  uint64  totOvl      = writer->loadBucketSizes(bucketSizes);

  //  Fail if we don't have enough memory to process.

  checkMemory(ovlName, sliceNum, totOvl, maxMemory);

  //  Allocatge space for overlaps, and load them.

  ovOverlap *ovls   = ovOverlap::allocateOverlaps(seq, totOvl);
  uint64     ovlsLen = 0;

  for (uint32 bb=0; bb<=config->numBuckets(); bb++)
    writer->loadOverlapsFromBucket(bb, bucketSizes[bb], ovls, ovlsLen);

  //  Check that we found all the overlaps we were expecting.

  if (ovlsLen != totOvl) {
    fprintf(stderr, "ERROR: read " F_U64 " overlaps, expected " F_U64 "\n", ovlsLen, totOvl);
    exit(1);
  }

  //  Clean up space if told to.

  if (deleteIntermediateEarly)
    writer->removeOverlapSlice();

  //  Sort the overlaps!  Finally!  The parallel STL sort is NOT inplace, and blows up our memory.

  fprintf(stderr, "\n");
  fprintf(stderr, "Sorting.\n");

#ifdef _GLIBCXX_PARALLEL
  __gnu_sequential::sort(ovls, ovls + ovlsLen);
#else
  sort(ovls, ovls + ovlsLen);
#endif

  //  Output to the store.

  fprintf(stderr, "\n");   //  Sorting has no output, so this would generate a distracting extra newline
  fprintf(stderr, "Writing sorted overlaps.\n");

  writer->writeOverlaps(ovls, ovlsLen);

  //  Clean up.  Delete inputs, remove the sentinel, release memory, etc.

  delete [] ovls;
  delete [] bucketSizes;

  removeSentinel(ovlName, sliceNum);

  seq->sqStore_close();

  if (deleteIntermediateLate) {
    fprintf(stderr, "\n");
    fprintf(stderr, "Removing bucketized overlaps.\n");
    fprintf(stderr, "\n");

    writer->removeOverlapSlice();
  }

  //  Success!

  fprintf(stderr, "Success!\n");

  return(0);
}
