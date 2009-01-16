/* -*-C++-*-
   (c) Copyright 2008, Hewlett-Packard Development Company, LP

   See the file named COPYING for license details
*/

#include <Lintel/ProgramOptions.hpp>

using namespace std;
namespace po = boost::program_options;

static lintel::ProgramOption<bool> po_help("help,h", "get help on this program");

namespace {
    using namespace boost::program_options;
    // TODO: check if we have an old version of boost and only use this
    // if we do; 1.34 fixes the bug in 1.33 that causes an exception for
    // unrecognized options.1
    vector< basic_option<char> > 
    collectRecognized(const vector< basic_option<char> > &from) {
	vector< basic_option<char> > ret;
	for(unsigned i = 0; i < from.size(); ++i) {
	    if (!from[i].unregistered) {
		ret.push_back(from[i]);
	    }
	}
	return ret;
    }

    void po_vm_store(po::parsed_options &parsed, po::variables_map &var_map) {
	vector< basic_option<char> > recognized 
	    = collectRecognized(parsed.options);

	parsed_options tmp(parsed.description);
	tmp.options = recognized;
	po::store(tmp, var_map);
    }
}

namespace {


    std::string extra_help;
    using namespace lintel::detail;

    // separated out for the future when we may have multiple parsing
    // interfaces.
    void basicParseCommandLine(po::command_line_parser &parser,
			       po::options_description &desc,
			       po::variables_map &var_map,
			       vector<string> &further) {
	po::parsed_options parsed = 
	    parser.options(desc).allow_unregistered().run();
	further = po::collect_unrecognized(parsed.options, 
					   po::include_positional);
	po_vm_store(parsed, var_map);
	po::notify(var_map);
    }

    void dumpVariables(po::variables_map &var_map) {
	for(map<string, po::variable_value>::iterator i = var_map.begin(); 
	    i != var_map.end(); ++i) {
	    cout << boost::format("%s\n") % i->first;
	}
    }

    void processOptions(vector<ProgramOptionPair> &actions, po::variables_map &var_map) {
	if (false) {
	    dumpVariables(var_map);
	}
	vector<ProgramOptionPair>::iterator iter = actions.begin();
	for (; iter != actions.end(); ++iter) {
	    SINVARIANT(!!iter->second);
	    size_t pos = iter->first.find(',');
	    if (pos != string::npos) {
		SINVARIANT(pos > 0);
		iter->second(var_map[iter->first.substr(0,pos)]);
	    } else {
		iter->second(var_map[iter->first]);
	    }
	}
    }

};

namespace lintel {
    namespace detail {
	po::options_description &programOptionsDesc() {
	    static po::options_description desc("Allowed options");
	    return desc;
	}

	vector<ProgramOptionPair> &programOptionsActions() {
	    static vector<ProgramOptionPair> po_pairs;
	    return po_pairs;
	}
    }

    void programOptionsHelp(const string &to_add) {
	extra_help.append(to_add);
    }

    void programOptionsUsage(const char *argv0) {
	cout << boost::format("Usage: %s [options] %s\n") % argv0 % extra_help
	     << programOptionsDesc() << "\n";
    }
	
    vector<string> parseCommandLine(int argc, char *argv[], bool allow_unrecognized) {
	po::variables_map var_map;
	vector<string> unrecognized;

	po::command_line_parser parser(argc, argv);
	basicParseCommandLine(parser, detail::programOptionsDesc(), var_map, unrecognized);
	INVARIANT(allow_unrecognized || unrecognized.empty(), boost::format
		  ("Unexpected option '%s'; try %s -h for help") 
		  % unrecognized[0] % argv[0]);
	processOptions(programOptionsActions(), var_map);
	if (po_help.get()) {
	    programOptionsUsage(argv[0]);
	    exit(0);
	}
	return unrecognized;
    }

}