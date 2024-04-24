#include <chrono>
#include <httplib.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <csignal>

#ifdef _MSC_VER

#include <direct.h>
#define PATH_MAX MAX_PATH
#define getcwd _getcwd
#define	SYSLOG(msg)
#define OPEN_SYSLOG(NAME)
#define CLOSE_SYSLOG()

#else

#include <unistd.h>
#include <syslog.h>
#include <execinfo.h>
#define	SYSLOG(level, msg) { syslog (level, "%s", msg); }
#define OPEN_SYSLOG(NAME) { setlogmask (LOG_UPTO(LOG_NOTICE)); openlog(NAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1); }
#define CLOSE_SYSLOG() closelog();\

#endif

#include "argtable3/argtable3.h"
#include "daemonize.h"

#include "event-dispatcher.h"

// i18n
// #include <libintl.h>
// #define _(String) gettext (String)
#define _(String) (String)
#define MSG_INTERRUPTED        _("Interruped")
#define MSG_LISTENER_DAEMON_RUN _("Run as daemon")
#define ERR_SEGMENTATION_FAULT  _("Segmentation fault")
#define ERR_ABRT                _("Aborted")
#define ERR_HANGUP_DETECTED     _("Hangup")
#define MSG_CHECK_SYSLOG        _("Check syslog")

#define CODE_OK                     0
#define ERR_CODE_SEGMENTATION_FAULT (-5001)
#define ERR_CODE_INT                (-5002)
#define ERR_CODE_ABRT               (-5003)
#define ERR_CODE_PARAM_INVALID      (-5004)

const char *programName = "tlns-check";

httplib::Server *svr;

static EventDispatcher ed;

const auto html = R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>SSE demo</title>
</head>
<body>
<script>
  const ev = new EventSource("event");
  ev.onmessage = function(e) {
    const d = e.data;
    console.log(new Date(), d);
    const l = document.getElementById("t");
    const n = document.createElement("div");
    const t = document.createTextNode(d);
    n.appendChild(t);
    l.appendChild(n);
  }
</script>
  <a target="_blanc" href="send?a=1&b=Ab">Send</a>  
<p id="t">
    
</p>  
</body>
</html>
)";

class TemperatureSSEConfiguration {
public:
    std::string intf;
    uint16_t port;
    bool daemonize;
    int verbosity;
    std::string pidfile;
    TemperatureSSEConfiguration()
        : port(1234), daemonize(false), verbosity(0) {
    }
};

static TemperatureSSEConfiguration config;

static void stop()
{
    svr->stop();
    delete svr;
}

static void done()
{
    svr = nullptr;
}

static void run() {
    svr = new httplib::Server;

    svr->Get("/", [&](const httplib::Request & /*req*/, httplib::Response &res) {
        res.set_content(html, "text/html");
    });

    svr->Get("/send", [&](const httplib::Request & req, httplib::Response &res) {
        std::stringstream ss;
        ss << "data: ";
        bool f = true;
        for (const auto & param : req.params) {
            if (f)
                f = false;
            else
                ss << ",";
            ss << param.first << "=" << param.second;
        }
        // std::cout << ss.str() << "\n";
        ss << "\n\n";
        auto s = ss.str();
        ed.send_event(s);
        res.set_content(s, "text/plain");
    });

    svr->Get("/event", [&](const httplib::Request & /*req*/, httplib::Response &res) {
        std::cout << "connected" << std::endl;
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_chunked_content_provider("text/event-stream",
            [&](size_t /*offset*/, httplib::DataSink &sink) {
                ed.wait_event(&sink);
                return true;
            });
    });
    svr->listen(config.intf, config.port);
}

#define TRACE_BUFFER_SIZE   256

static void printTrace() {
#ifdef _MSC_VER
#else
    void *t[TRACE_BUFFER_SIZE];
    backtrace_symbols_fd(t, backtrace(t, TRACE_BUFFER_SIZE), STDERR_FILENO);
#endif
}

void signalHandler(int signal)
{
    switch (signal)
    {
        case SIGINT:
            std::cerr << MSG_INTERRUPTED << std::endl;
            stop();
            done();
            exit(ERR_CODE_INT);
        case SIGSEGV:
            std::cerr << ERR_SEGMENTATION_FAULT << std::endl;
            printTrace();
            exit(ERR_CODE_SEGMENTATION_FAULT);
        case SIGABRT:
            std::cerr << ERR_ABRT << std::endl;
            printTrace();
            exit(ERR_CODE_ABRT);
#ifndef _MSC_VER
        case SIGHUP:
            std::cerr << ERR_HANGUP_DETECTED << std::endl;
            break;
#endif
        default:
            break;
    }
}

/**
 * Split @param address e.g. ADRESS:PORT to @param retAddress and @param retPort
 */
static bool splitAddress(
    std::string &retAddress,
    uint16_t &retPort,
    const std::string &address
)
{
    size_t pos = address.find_last_of(':');
    if (pos == std::string::npos)
        return false;
    retAddress = address.substr(0, pos);
    std::string p(address.substr(pos + 1));
    retPort = (uint16_t) strtoul(p.c_str(), nullptr, 0);
    return true;
}

/**
 * Parse command line
 * Return 0- success
 *        ERR_CODE_PARAM_INVALID- show help and exit, or command syntax error
 **/
static int parseCmd(
    TemperatureSSEConfiguration *retConfig,
    int argc,
    char *argv[]
)
{
    // device path
    struct arg_str *a_address_port = arg_str0(nullptr, nullptr, _("<ip-addr:port>"), _("Server address and port Default 127.0.0.1:1234"));
    struct arg_lit *a_daemonize = arg_lit0("d", "daemonize", _("Run as daemon"));
    struct arg_str *a_pid_file = arg_str0("p", "pidfile", _("<file>"), _("Check whether a process has created the file pidfile"));
    struct arg_lit *a_verbosity = arg_litn("v", "verbose", 0, 2, _("Verbosity level"));
    struct arg_lit *a_help = arg_lit0("?", "help", _("Show this help"));
    struct arg_end *a_end = arg_end(20);

    void *argtable[] = {
            a_address_port,
            a_daemonize, a_pid_file, a_verbosity, a_help, a_end
    };

    // verify the argtable[] entries were allocated successfully
    if (arg_nullcheck(argtable) != 0) {
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return ERR_CODE_PARAM_INVALID;
    }
    // Parse the command line as defined by argtable[]
    int nErrors = arg_parse(argc, argv, argtable);

    if (a_address_port->count)
        splitAddress(retConfig->intf, retConfig->port, *a_address_port->sval);
    else {
        retConfig->intf = "127.0.0.1";
        retConfig->port = 1234;
    }
    retConfig->daemonize = (a_daemonize->count > 0);
    if (a_pid_file->count)
        retConfig->pidfile = *a_pid_file->sval;
    else
        retConfig->pidfile = "";

    retConfig->verbosity = a_verbosity->count;

    // special case: '--help' takes precedence over error reporting
    if ((a_help->count) || nErrors) {
        if (nErrors)
            arg_print_errors(stderr, a_end, programName);
        std::cerr << _("Usage: ") << programName << std::endl;
        arg_print_syntax(stderr, argtable, "\n");
        std::cerr << programName << std::endl;
        arg_print_glossary(stderr, argtable, "  %-25s %s\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return ERR_CODE_PARAM_INVALID;
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return CODE_OK;
}

static void setSignalHandler()
{
#ifdef _MSC_VER
#else
	struct sigaction action = {};
	action.sa_handler = &signalHandler;
	sigaction(SIGINT, &action, nullptr);
	sigaction(SIGHUP, &action, nullptr);
#endif
}

int main(int argc, char **argv)
{
    if (parseCmd(&config, argc, argv))
        return ERR_CODE_PARAM_INVALID;

    if (config.daemonize) {
		char workDir[PATH_MAX];
		std::string programPath = getcwd(workDir, PATH_MAX);
		if (config.verbosity)
			std::cerr << MSG_LISTENER_DAEMON_RUN
                      << "(" << programPath << "/" << programName << "). "
                      << MSG_CHECK_SYSLOG << std::endl;
		OPEN_SYSLOG(programName)
        Daemonize daemon(programName, programPath, run, stop, done, 0, config.pidfile);
	} else {
		setSignalHandler();
		run();
		done();
	}
}
