/* -*-C++-*- */
/*
   (c) Copyright 2007-2008, Hewlett-Packard Development Company, LP

   See the file named COPYING for license details
*/

/** @file 

    Very low overhead logging support; disable debug level logging by
    adding -DDISABLE_DEBUG_LINTELLOG
*/

#ifndef LINTEL_LOG_HPP
#define LINTEL_LOG_HPP

#include <string>

#include <boost/format.hpp>
#include <boost/function.hpp>

#include <Lintel/HashMap.hpp>
#include <Lintel/SimpleMutex.hpp>

#ifndef DISABLE_DEBUG_LINTELLOG
#define DISABLE_DEBUG_LINTELLOG 0
#endif

#if DISABLE_DEBUG_LINTELLOG
#define LintelLogDebug(category, msg) do { } while(0)
#define LintelLogDebugLevel(category, level, msg) do { } while(0)
#define LintelLogDebugLevelVariable(category, level, msg) do { } while(0)
#else

/// General version of LintelLogDebug that allows you to specify the
/// debugging level.
#define LintelLogDebugLevel(category, level, msg) \
do { \
    static LintelLog::Category ll_category(std::string(category)); \
    if (LintelLog::wouldDebug(ll_category, level)) { \
        LintelLog::debug(msg); \
    } \
} while(0)

/// Macro for doing logging; expects the category to be a string, which
/// should be constant as it will only be interpreted once. Message
/// can either be a string or a boost::format
///
/// If you try to call LintelLogDebug with a variable it won't do what
/// you expect because the first time through it will copy the
/// category you pass in, and from then on will hold the category
/// constant.  Use the last macro if you want to be able to change the
/// variable controlling debugging.
#define LintelLogDebug(category, msg) LintelLogDebugLevel(category, 1, msg)

/// Version of LintelLogDebugLevel that allows the category to be a
/// variable.
/// 
/// The macro forces people to pass in Category's because if they pass
/// in strings it's just going to be too inefficient.  If they want
/// that ineffiency, they can always say:
/// LintelLogDebugLevelVariable(LintelLog::Category(string_var), 0,
/// "slow");
#define LintelLogDebugLevelVariable(category, level, msg) \
do { \
    if (LintelLog::wouldDebug(static_cast<const LintelLog::Category &>(category), \
                              level)) { \
        LintelLog::debug(msg); \
    } \
} while(0)
#endif

/// The lintel logging class.  Right now it is pretty highly focused
/// on the debugging side of things; it has support for some of the
/// other standard levels for logging for converage, but currently
/// no support for turning those levels on or off.
///
/// LintelLog operates around the idea of named debug "categories"
/// which are specified through the use of staticly declared
/// LintelLog::Category objects.  The creation of these objects
/// provides the ability to activate any debugging statements within a
/// category through the use of an environment variable,
/// LINTEL_LOG_DEBUG, which contains a comma-separated list of
/// Category names.  This environment variable is parsed by the
/// command LintelLog::parseEnv(), which must be called to enable
/// debugging.  By default LintelLog will send debugging to the
/// console, however, this can be overridden through the use of
/// "appender" functions.  These appender functions recieve the
/// debugging string and a LogType and can act accordingly.  These are
/// activated by calling LintelLog::addAppender().  Multiple appenders
/// can be applied simultaneously.
class LintelLog {
public:
    /// Similar to the logging levels from http://www.slf4j.org/ we
    /// skip the trace level of debugging (you'd want to log directly
    /// to DataSeries if you're going to write that much out), and add
    /// the Report level, which doesn't really match up with the
    /// standard hierarchy as it's intended for a different class of
    /// output, and is expected to be multi-line whereas the other
    /// levels should stay on a single line. 
    ///
    /// Report level logging is for writing out multi-line reports.  Debug
    /// level logging is for detailed debugging information.  Info level
    /// logging is for expected, but normal operation.  Warn level logging is
    /// for unexpected problems tha the logger can automatically handle.  Error
    /// level logging is for unexpected problems that the logger can not
    /// automatically handle.
    enum LogType { Report, Debug, Info, Warn, Error, MaxLogType };

    typedef boost::function<void (const std::string &, const LogType)> 
        appender_fn;

    /// Note that constructing these can be somewhat inefficient, hence
    /// you want to do it exactly once or you destroy the efficiency of
    /// the debugging code.
    class Category {
    public:
	Category(const std::string &category) 
	    : id(categoryToId(category)) { }
	
	unsigned id;
    };

    /// should we generate debugging output for this category at the
    /// specified level?
    static bool wouldDebug(const Category &category, unsigned level = 1) {
	return instance->debug_levels[category.id] >= level;
    }

    static uint8_t getDebugLevel(const Category &category) {
	return instance->debug_levels[category.id];
    }

    /// this version is much slower than statically constructing a
    /// LintelLog::Category, but is somewhat simpler to use
    static bool wouldDebug(const std::string &category, unsigned level = 1);

    /// Report string, usually printed only with trailing newline, may
    /// have multiple lines of output.
    static void report(const std::string &msg) {
	log(msg, Report);
    }

    /// Debug level logging; you probably want to be using the
    /// LintelLogDebug* series of macros instead of calling this
    /// function.
    static void debug(const std::string &msg) {
	log(msg, Debug);
    }

    /// Informational level logging; should only be one line of output
    /// as it will be prefixed with INFO:
    static void info(const std::string &msg) {
	log(msg, Info);
    }

    /// Warning level logging; should only be one line of output as it
    /// will be prefixed with WARN:
    static void warn(const std::string &msg) {
	log(msg, Warn);
    }

    /// Error level logging; should only be one line of output as it
    /// will be prefixed with ERROR:
    static void error(const std::string &msg) {
	log(msg, Error);
    }

    /// boost::format variant of report()
    static void report(const boost::format &f) { 
	report(f.str()); 
    };
    /// boost::format variant of debug()
    static void debug(const boost::format &f) {
	debug(f.str());
    }
    /// boost::format variant of info()
    static void info(const boost::format &f) {
	info(f.str());
    }
    /// boost::format variant of warn()
    static void warn(const boost::format &f) {
	warn(f.str());
    }
    /// boost::format variant of error()
    static void error(const boost::format &f) {
	error(f.str());
    }

    /// The max_categories constant specifices the maximum number of
    /// categories that can occur in total in the program using the
    /// library.  This function assumes that it is called while the
    /// program is still single threaded.  The constant is necessary
    /// to achieve thread safety without a massive performance
    /// overhead.  The library will generate a warning when 3/4 of the
    /// categories are used.  If you use
    /// setKnownDebuggingCategories(), then this function is probably
    /// unnecessary.
    static void reserveCategories(unsigned max_categories = 4000);

    /// extracting any debugging options from the LINTEL_LOG_DEBUG
    /// environment variable using parseDebugString().
    static void parseEnv();

    /// parse a debugging options string containing comma separated
    /// entries of the form category[=level]
    static void parseDebugString(const std::string &debug_options);

    /// set known debugging categories; will pre-allocate the entries
    /// so that creating the Category instances goes faster, and so
    /// that it can report on the existing categories in
    /// debugMessagesInitial()
    static void setKnownCategories(const std::vector<std::string> &categories);

    /// Set the debugging level for a particular category to the specified
    /// level. 
    static void setDebugLevel(const std::string &category, uint8_t level = 1);

    /// If we wouldDebug("help"), then print out the list of known
    /// debugging options
    static void debugMessagesInitial(); 

    /// If we wouldDebug("LintelLog::stats"), then print out logging
    /// statistics.
    static void debugMessagesFinal();

    /// This function allows you to specify appenders that will be run
    /// when a message should be logged.  Appenders are called
    /// atomically (the instance->mutex will be held).  By default the
    /// list is empty, in which case the consoleAppender function will
    /// be automatically called. (appender name from log4j)
    static void addAppender(appender_fn fn);

    /// Translate a log type into a string.
    static const std::string &logTypeToString(const LogType logtype);

    /// This function will write Report, Debug, and Info type messages
    /// to stdout, and Warn and Error type messages to stderr.  All
    /// messages will have a newline appended.  All messages except
    /// for the Report type will be prefixed with the log type.  This
    /// is the default appender if none are specified.
    static void consoleAppender(const std::string &msg, const LogType logtype);

    /// This function is the same as the consoleAppender, but for non
    /// Report type messages, it will add in \@seconds.microseconds
    /// after the level.
    static void consoleTimeAppender(const std::string &msg,
				    const LogType logtype);
    
    /// Basic logging function; you probably don't want to call this
    /// directly but through the various macros and other functions.
    static void log(const std::string &msg, const LogType log_type);
private:
    static uint32_t categoryToId(const std::string &category);
    static void maybeCategoryLimitWarning(uint32_t id);
    /// Assumes instance->mutex is held
    static uint32_t lockedCategoryToId(const std::string &category);
    static void maybeInitInstance();

    struct instance_data {
	std::vector<uint8_t> debug_levels;
	HashMap<std::string, uint32_t> category2id;
	std::vector<appender_fn> appenders;
	SimpleMutex mutex;
	// use about 4k of space to store the debugging levels; it
	// turns out there isn't any race-free way to resize this once
	// we go multi-threaded, so rely on the calls to
	// reserveCategories() to resize this if necessary.
	instance_data() {
	    debug_levels.reserve(4000); 
	}
    };

    static instance_data *instance;
};

#endif
