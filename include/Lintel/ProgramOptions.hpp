/* -*-C++-*-
   (c) Copyright 2008, Hewlett-Packard Development Company, LP

   See the file named COPYING for license details
*/

#ifndef LINTEL_PROGRAM_OPTIONS_HPP
#define LINTEL_PROGRAM_OPTIONS_HPP

#include <inttypes.h>

#include <iostream>
#include <utility>

#include <boost/bind.hpp>
#include <boost/program_options.hpp>

#include <Lintel/AssertBoost.hpp>

/** @file
 
    \brief Program options handling, wrapper around boost::program_options

    Program options handling; simplest usage is:
 
    @verbatim

    using namespace std;
    // specify default seed as time() ^ getpid()
    lintel::ProgramOption<int> seed("seed", "specify the random seed", time() ^ getpid());
    lintel::ProgramOption<bool> help("help", "get help");
    lintel::ProgramOption< vector<string> > params("param", "specify string parameters");

    int main(int argc, char *argv[]) {
        vector<string> files = lintel::parseCommandLine(argc, argv, true);
	if (files.empty() || help.get()) {
	    cout << boost::format("Usage: %s [--help] [--seed=#] [--param=parameter] file...")
	        % argv[0];
            exit(0);
        }
	random_number_generator.seed(seed.get());
	if (params.used()) {
	    BOOST_FOREACH(const string &param, params.get()) {
	        handleParamter(param);
            }
        }

	...;
    }

    Slightly more advanced usage would be to specify:

    void skewHandler(const boost::program_options::variable_value &opt) {
       if (opt.empty()) {
           ... do default handling ...
       } else {
           ... do handling with opt.as<T>() ...
       }
    }

    lintel::ProgramOption<double> skew("skew", "time skew to apply", 
                                       boost::bind(&skewHandler, _1));

    Which will result in skewHandler being called when a call to
    lintel::parseCommandLine is made.

    @endverbatim
*/

namespace lintel {
    namespace detail {
	typedef boost::function<void (const boost::program_options::variable_value &)> 
	  ProgramOptionFnT;
	typedef std::pair<std::string, ProgramOptionFnT> ProgramOptionPair;

	// TODO: Use the boost support for multiple option groups rather than what we do here.
	boost::program_options::options_description &programOptionsDesc(bool is_hidden = false);
	
	std::vector<ProgramOptionPair> &programOptionsActions();

        // Helper to "pretty" print the default value in description message.
        template<typename T> std::string defaultValueString(const T &def_val) {
            return str(boost::format("[%1%]") % def_val);
        }
        // overload general helper for char's since they default to value 0
        // but cast to string. This leads to ugly line breaks/missing text in
        // default value message.
        template<> inline std::string defaultValueString<char>(const char &def_val) {
            return str(boost::format("[\'\\%03d\']") % static_cast<int>(def_val));
        }
        // Helper to pretty print default values of vector program options
        template<typename T> std::string defaultValueString(const std::vector<T> &def_vector) {
            std::string vdesc = "[";
            for(uint32_t i = 0; i < def_vector.size(); ++i) {
                vdesc += defaultValueString(def_vector[i]);
            }
            vdesc += "]";
            return vdesc;
        }

        typedef boost::function<void ()> ProgramOptionInitFn;

        void addProgramOptionInitFn(const ProgramOptionInitFn &initfn);
        void runProgramOptionInitFns();
    }

    /// add string to help message, should be called before a call to
    /// parseCommandLine.  Will be printed after the 'Usage: argv[0]' bit.
    void programOptionsHelp(const std::string &to_add);
    
    /// Adds the string to the help message; since it is in the constructor it can be done as a
    /// global variable and hence happen before main().
    class OptionsHelpAdder {
    public:
        OptionsHelpAdder(const std::string &to_add) {
            programOptionsHelp(to_add);
        }
    };

    /// print out the usage information. Uses value set by setArgvZero or by
    /// parseCommandLine(argc,argv) for program name.
    void programOptionsUsage();

    /// print out the usage information. Uses argv0 passed in as program name.
    void programOptionsUsage(const std::string &argv0);

    /// sets program name for progamOptionsUsage
    void setArgvZero(const std::string &argv0);

    /// parse command line options as a standard argc, argv pair; sets the
    /// program name (argv[0]) for ProgramOptionsUsage via setArgvZero.
    ///
    /// @return any un-parsed arguments if allow_unrecognized is true
    std::vector<std::string> parseCommandLine(int argc, char *argv[], 
					      bool allow_unrecognized = false);

    /// parse command line options as vector of strings
    ///
    /// @return any un-parsed arguments if allow_unrecognized is true
    std::vector<std::string> parseCommandLine(const std::vector<std::string> &args,
					      bool allow_unrecognized = false);

    /// \brief Class for constructing program arguments more easily.
    class ProgramArguments {
    public:
	ProgramArguments() { }

	ProgramArguments(const std::string &arg) {
	    args.push_back(arg);
	}

	ProgramArguments(const boost::format &f) {
	    args.push_back(f.str());
	}

	ProgramArguments &operator <<(const std::string &arg) {
	    args.push_back(arg);
	    return *this;
	}

	ProgramArguments &operator <<(const boost::format &f) {
	    args.push_back(f.str());
	    return *this;
	}

	std::vector<std::string> args;
    };

    inline void parseCommandLine(const ProgramArguments &pa) {
	parseCommandLine(pa.args);
    }
    
    inline void parseCommandLine(const std::string &arg) {
	parseCommandLine(ProgramArguments(arg));
    }

    /// \brief A single program option.
    ///
    /// Generic template program option, you can use ProgramOption<
    /// vector<int> > to allow for multiple values to be specified, or
    /// just ProgramOption<int> for one value.
    template<typename T> class ProgramOption {
    public:
	/// Program option without an action function; expected to be
	/// accessed via the used() and get() functions.  If you specify
	/// a default value, or the default initialization of T is a
	/// sane default, you can skip used().  
	/// 
	/// @param name option name, will match with --name; should be a static string
	/// @param desc option description, will be printed when --help/-h is used; 
	///             should be a static string
	/// @param in_default_val default value for the option
	ProgramOption(const std::string &name, const std::string &desc, 
		      // TODO: This should be removed, and we should
		      // force people to specify a default Make a
		      // 2-arg and a 3 arg, and put
		      // FUNC_DEPRECATED_PREFIX before the 2 valued
		      // and FUNC_DEPRICATED after it, the fix a bunch of them.
		      // 
		      // May want to check downstream packages if a
		      // similar todo is in there.
		      const T &in_default_val = T())
	    : default_val(in_default_val)
	{
	    init(name, desc);
	}
	
	bool used() const {
	    return !saved.empty();
	}
	
	T get() const {
	    if (used()) {
		return saved.as<T>();
	    } else {
		return default_val;
	    }
	}

	void set(const T &val) {
            saved = boost::program_options::variable_value(val, false);
	}

    protected:
	ProgramOption(const std::string &name, const std::string &desc,
		      const T &in_default_val, bool do_description)
	    : default_val(in_default_val)
	{
	    init(name, desc, do_description);	    
	}

    private:
	void init(const std::string &name, const std::string &desc, 
		  bool do_help=true) {
            detail::addProgramOptionInitFn(boost::bind(&ProgramOption<T>::doInit, this, 
                                                       name, desc, do_help));
	}

        void doInit(const std::string &name, const std::string &desc, bool do_help) {
	    std::string def_desc = str(boost::format("%s (Default value %s.)")
		    % desc % detail::defaultValueString(default_val));
	    detail::programOptionsDesc(!do_help).add_options()
		(name.c_str(), boost::program_options::value<T>(), def_desc.c_str());
	    detail::ProgramOptionFnT f = boost::bind(&ProgramOption<T>::save, this, _1);
	    detail::programOptionsActions().push_back(std::make_pair(name, f));
        }
            
	void save(const boost::program_options::variable_value &opt) {
	    if (!opt.empty()) { // Do not overwrite saved opts with empty values.
		saved = opt;
	    }
	}
	
	boost::program_options::variable_value saved;
	const T default_val;
    };


    /// \brief Program option which won't appear in the usage unless in debug mode
    template<typename T> class TestingOption : public ProgramOption<T> {
    public:	
#if LINTEL_DEBUG
	static const bool show_description = true;
#else
	static const bool show_description = false;
#endif
	
	TestingOption(const std::string &name, const std::string &desc, 
		      const T &in_default_val = T())
	    : ProgramOption<T>(name, desc, in_default_val, show_description) { }
    };

    /// \brief Special case boolean program option
    ///
    /// Special case for program options without values, e.g.,
    /// "--help", which doesn't carry a value like "--seed=100"
    template<> class ProgramOption<bool> {
    public:
	ProgramOption(const std::string &name, const std::string &desc) 
	    : val(false)
	{
	    init(name, desc);
	}

	bool used() const {
	    return val;
	}
	
	bool get() const {
	    return val;
	}
	
	void set(const bool &_val) {
            val = _val;
	}

    protected:
	ProgramOption(const std::string &name, const std::string &desc, bool do_description)
	    : val(false)
	{
	    init(name, desc, do_description);
	}
	
    private:
	void init(const std::string &name, const std::string &desc, 
		  bool do_help=true) {
            detail::addProgramOptionInitFn(boost::bind(&ProgramOption<bool>::doInit, this, 
                                                       name, desc, do_help));
        }

        void doInit(const std::string &name, const std::string &desc, bool do_help) {
	    detail::programOptionsDesc(!do_help).add_options() (name.c_str(), desc.c_str());
	    detail::ProgramOptionFnT f = boost::bind(&ProgramOption<bool>::save, this, _1);
	    detail::programOptionsActions().push_back(std::make_pair(name, f));
	}

	void save(const boost::program_options::variable_value &opt) {
	    if (!opt.empty()) {
		val = true;
	    }
	}

	bool val;
    };

    /// \brief Hidden option which also doesn't take an argument
    template<> class TestingOption<bool> : public ProgramOption<bool> {
    public:	
	static const bool show_description = TestingOption<int>::show_description;

	TestingOption(const std::string &name, const std::string &desc)
	    : ProgramOption<bool>(name, desc, show_description) { }
    };

    void processOptions(std::vector<detail::ProgramOptionPair> &actions,
			boost::program_options::variables_map &var_map);

    void parseConfigFile(const std::string &name);

    template<class charT>
    void parseConfigFile(std::basic_istream<charT> &t) {
        detail::runProgramOptionInitFns();
        boost::program_options::variables_map var_map;
        boost::program_options::store(boost::program_options::parse_config_file
                                      (t, detail::programOptionsDesc()), var_map);
        boost::program_options::notify(var_map);
        processOptions(detail::programOptionsActions(), var_map);
    }
}

#endif
