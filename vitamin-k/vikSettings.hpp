#pragma once

#include <stdint.h>
#include <getopt.h>

#include "vksLog.hpp"

#include "vikWindow.hpp"

namespace vik {
class Settings {
public:
    /** @brief Activates validation layers (and message output) when set to true */
    bool validation = false;
    /** @brief Set to true if fullscreen mode has been requested via command line */
    bool fullscreen = false;
    /** @brief Set to true if v-sync will be forced for the swapchain */
    bool vsync = false;

    uint32_t gpu_index = 0;

    bool list_gpus_and_exit = false;

    enum Window::window_type type = Window::AUTO;

    std::string help_string() {
	std::string help =
	        "A XR demo for Vulkan and OpenHMD\n"
	        "\n"
	        "Options:\n"
	        "  -s, --size WxH              Size of the output window (default: 800x600)\n"
	        "      --fullscreen            Run fullscreen (equivalent to --size -1x-1)\n"
	        "  -w  --window WS             Window system plugin to use (default: choose best)\n"
	        "                              [xcb, wayland, kms]\n"
	        "  -h, --help                  Display help\n";

	//for (auto const& wsh : window_system_help)
	//    help += wsh;

	return help;
    }

    bool parse_args(int argc, char *argv[]) {
	/* Setting '+' in the optstring is the same as setting POSIXLY_CORRECT in
	   * the enviroment. It tells getopt to stop parsing argv when it encounters
	   * the first non-option argument; it also prevents getopt from permuting
	   * argv during parsing.
	   *
	   * The initial ':' in the optstring makes getopt return ':' when an option
	   * is missing a required argument.
	   */

	int option_index = -1;
	static const char *optstring = "m:hs:w:";


	struct option long_options[] = {
	    {"window", 1, 0, 0},
	    {"size", 1, 0, 0},
	    {"help", 0, 0, 0},
	    {0, 0, 0, 0}
        };

	std::string optname;

	int opt;
	while ((opt = getopt_long(argc, argv, optstring,
	                          long_options, &option_index)) != -1) {
	    if (opt == '?' || opt == ':')
		return false;

	    if (option_index != -1)
		optname = long_options[option_index].name;

	    if (opt == 'h' || optname == "help") {
		printf("%s\n", help_string().c_str());
		exit(0);
	    } else if (opt == 's' || optname == "size") {
	    } else if (opt == 'w' || optname == "window") {
		type = vik::Window::window_type_from_string(optarg);
		if (type == vik::Window::INVALID)
		    vik_log_f("option -m given bad display mode");
	    } else
		vik_log_f("Unknown option %s", optname.c_str());
	}

	if (optind != argc)
	    vik_log_w("trailing args");

	return true;
    }

};
}
