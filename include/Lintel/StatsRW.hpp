/* -*-C++-*- */
/*
   (c) Copyright 2001-2005, Hewlett-Packard Development Company, LP

   See the file named COPYING for license details
*/

/** @file
    \brief StatsRW class header
*/

#ifndef LINTEL_STATS_RW_HPP
#define LINTEL_STATS_RW_HPP

#include <Lintel/Stats.hpp>
#include <Lintel/StatsSequence.hpp>

/// Stats class that separates between reads and writes.
class StatsRW {
public:
    enum modeT { Read, Write, All };
    StatsRW(const std::string &name); // make stats objects based on StatsMaker
    StatsRW(Stats *read, Stats *write, Stats *all); // can have null entries; StatsRW will delete these stats when it is deleted.
    virtual ~StatsRW();
  
    // Stats objects which can use the time-series information will get it;
    // others will ignore it
    void add(const double value, const modeT mode, 
	     const double timeSeq = 0);
    void setSequenceInfo(double interval_width, StatsSequence::mode mode); 
      // setSequenceInfo does something if a sub-object is a StatsSequence 
      // objects, otherwise is no-op for each object.
    virtual void printRome(int depth, std::ostream &out) const;
    virtual void printTabular(int depth, std::ostream& out) const;
private:
    void finish_init();
    Stats *read, *write, *all;
    bool autoAll; // will automatically add things into the all category
    // unless one is specifically added into there, in which case the
    // stats object is reset
    bool allStatsSeriesGroup;
};

#endif
