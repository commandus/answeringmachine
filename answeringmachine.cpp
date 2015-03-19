#include <algorithm>
#include <argtable2.h>
#include "processanswer.h"
#include "Deamonize.h"
#include "signalhandler.h"
#include "ConfigClient.h"
#include "ClientContext.h"
#include "Logger.h"

#define progname	"answeringmachine"

ConfigClient config;

Logger *logger;

/*
Parameters:
argcFromCommandLine	argumemt count before merge with arguments from sipws.cfg
*/
int parseCmd(int argc, char* argv[], ConfigClient &config)
{
	struct arg_str  *a_registrar_uri = arg_str1("s", "sip", "<uri>", "SIP registrar host name or IP address e.g. 192.168.1.1");
	struct arg_int	*a_port = arg_int0("p", "port", "<port>", "SIP registrar port number, default 5060");
	// struct arg_lit  *a_tcp = arg_lit0("t", "tcp", "enable TCP transport, otherwise UDP");
	
	struct arg_str  *a_id = arg_str1("i", "id", "<name>", "Identifier");
	struct arg_str  *a_domain = arg_str1("d", "domain", "<name>", "Domain name. Default is registrar host name or IP address (-h)");
	struct arg_str  *a_password = arg_str0("w", "password", "<key>", "password, default empty");
	struct arg_file *a_playfn = arg_file1("f", "play", "<file>", "WAV file to play");
	struct arg_file *a_recfn = arg_file1("r", "record", "<file>", "WAV file to record");
	struct arg_lit  *a_verbose = arg_litn("v", "verbose", 0, 5, "severity: -v: error, ... -vvvvv: debug. Default fatal error only.");
	struct arg_lit  *a_daemon = arg_lit0("d", "daemon", "start as deamon");
	struct arg_lit  *a_help = arg_lit0("h", "help", "print this help and exit");
	
	struct arg_end  *a_end = arg_end(20);
	void* argtable[] = { a_registrar_uri, a_port, /*a_tcp, */a_id, a_domain, a_password,
		a_playfn, a_recfn, a_verbose, a_daemon, a_help, a_end };
	int nerrors;
	// verify the argtable[] entries were allocated sucessfully
	if (arg_nullcheck(argtable) != 0)
	{
		std::cerr << "insufficient memory" << std::endl;
		arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
		return 1;
	}
	// Parse the command line as defined by argtable[]
	nerrors = arg_parse(argc, argv, argtable);
	
	// special case: '--help' takes precedence over error reporting
	if ((a_help->count > 0) || nerrors)
	{
		std::cout << "Usage " << progname << std::endl;
		arg_print_syntax(stdout, argtable, "\n");
		std::cout << "play wav file on call" << std::endl;
		arg_print_glossary(stdout, argtable, "  %-25s %s\n");
		arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
		return 1;
	}

	config.host = *a_registrar_uri->sval;
	if (a_port->count > 0)
	{
		config.port = *a_port->ival;
	}
	config.id = *a_id->sval;
	config.domain = *a_domain->sval;
	if (a_password->count > 0)
	{
		config.password = *a_password->sval;
	}
	config.playfilename = *a_playfn->filename;
	config.recfilename = *a_recfn->filename;
	config.severity = a_verbose->count;
	config.isTcp = false; // TODO no support yet implemented. (a_tcp->count > 0);
	config.isDaemon = (a_daemon->count > 0);
	arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
	return 0;
}

void done()
{
	if (config.severity)
		if (logger && logger->lout(VERB_DEBUG))
			*logger->lout(VERB_DEBUG) << "Done" << std::endl;
	delete logger;
}

void stopRequest()
{
	getClientContext()->isQuit = PJ_TRUE;
}

void onSignal(int signal)
{
	stopRequest();
	done();
	exit(signal);
}

void run()
{
	logger = new Logger(config.binpath, progname, config.severity, config.isDaemon);
	SignalHandler sh(*logger->lout(VERB_FATAL), onSignal);
	process(config);
}

std::string extractPath(const char *modulename)
{
	struct MatchPathSeparator
	{
		bool operator()(char ch) const
		{
			return ch == '\\' || ch == '/';
		}
	};

	std::string r(modulename);
	r = std::string(r.begin(), std::find_if(r.rbegin(), r.rend(), MatchPathSeparator()).base());
	return r;
}

int main(int argc, char* argv[])
{
	if (parseCmd(argc, argv, config))
		exit(1);
	config.binpath = extractPath(argv[0]);
	if (config.isDaemon)
		Deamonize deamonize(progname, run, stopRequest, done);
	else
	{
		run();
		done();
	}

}

