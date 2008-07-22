/* -*-C++-*- */
/*
   (c) Copyright 1993-2005, Hewlett-Packard Development Company, LP

   See the file named COPYING for license details
*/

/** @file
    Simple statistics functions for single variables
*/

#ifndef LINTEL_STATS_HPP
#define LINTEL_STATS_HPP

#include <math.h>
#include <string>
#include <iostream>
#include <fstream>

class StatsBase {
protected:
    unsigned      reset_count;	///< Number of time reset() called
    bool          is_assigned;	///< True iff structure is not deleted
    
public:
    StatsBase();
    virtual ~StatsBase();		
    virtual bool checkInvariants() const;
    virtual void reset();		///< Reset to original values

    //----Query functions
    unsigned resets()      const { return reset_count; };
    virtual double min()	 const = 0;
    virtual double max()	 const = 0;
    virtual double mean()     const = 0;
    virtual double stddev()   const;
    virtual double variance() const = 0;
    virtual double conf95()   const = 0;
    virtual double relconf95()   const;

    /// Emit data about this value.
    virtual std::string debugString() const = 0;
};


//////////////////////////////////////////////////////////////////////////////
// Stats proper
//////////////////////////////////////////////////////////////////////////////

class Stats : public StatsBase {
protected:
    unsigned long long number;	///< How many calls to add() have occurred
    double        sum;		///< Sum of values passed in
    double        sumsq;	///< Sum of the squares of the values passed in
    double        min_value;	///< Minimum seen, else undefined
    double        max_value;	///< Minimum seen, else undefined

public:
    Stats(); 
    virtual ~Stats();
    
    /// Stick in a new value
    virtual void add(const double value);
    virtual void add(const Stats &stat); // attempt to merge two stats

    /// default function just re-dispatches to add
    virtual void addTimeSeq(const double value, const double timeSeq); 

    /// Clear all samples and stats.
    virtual void reset();

    // ----Query functions (not inherited from StatsBase):
    unsigned long count()  const { return (unsigned long)number; };
    unsigned long long countll() const { return number; };
    double total()  const { return sum; };
    double total_sq()  const { return sumsq; };

    // ----Query functions (inherited from StatsBase):
    virtual double min()	 const { return min_value; };
    virtual double max()	 const { return max_value; };
    unsigned resets()      const { return reset_count; };
    virtual double mean()     const;
    virtual double variance() const;
    virtual double conf95()   const;

    //----Printing functions
    virtual std::string debugString() const;
    virtual void printRome(int depth, std::ostream &out) const;
    virtual void printTabular(int depth, std::ostream &out) const;
    // TODO: decide whether this function should print trailing newline,
    // and if so, how it should deal with formats that are naturally multi-line
    // also whether indentation, etc should be in the interface.
    virtual void printText(std::ostream &out) const;
    friend std::ostream &operator<<(std::ostream &to, const Stats &s) {
	s.printText(to);
	return to;
    }

    virtual Stats *another_new() const;
};


#endif