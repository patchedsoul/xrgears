#pragma once

#include <stdint.h>
#include <getopt.h>

#include "vksLog.hpp"

#include "vikWindow.hpp"

namespace vik {
class Settings {
public:

    /*
    struct option long_options[] = {
	{"benchmark", 1, 0, 0},
	{"size", 1, 0, 0},
	{"fullscreen", 0, 0, 0},
	{"present-mode", 1, 0, 0},
	{"pixel-format", 1, 0, 0},
	{"list-scenes", 0, 0, 0},
	{"show-all-options", 0, 0, 0},
	{"winsys-dir", 1, 0, 0},
	{"data-dir", 1, 0, 0},
	{"winsys", 1, 0, 0},
	{"winsys-options", 1, 0, 0},
	{"run-forever", 0, 0, 0},
	{"debug", 0, 0, 0},
	{"help", 0, 0, 0},
	{0, 0, 0, 0}
    };
    */

    /** @brief Activates validation layers (and message output) when set to true */
    bool validation = false;
    /** @brief Set to true if fullscreen mode has been requested via command line */
    bool fullscreen = false;
    /** @brief Set to true if v-sync will be forced for the swapchain */
    bool vsync = false;

    uint32_t gpu_index = 0;

    bool list_gpus_and_exit = false;

    enum Window::window_type type = Window::AUTO;

    void parse_args(int argc, char *argv[]) {
      /* Setting '+' in the optstring is the same as setting POSIXLY_CORRECT in
	   * the enviroment. It tells getopt to stop parsing argv when it encounters
	   * the first non-option argument; it also prevents getopt from permuting
	   * argv during parsing.
	   *
	   * The initial ':' in the optstring makes getopt return ':' when an option
	   * is missing a required argument.
	   */
      static const char *optstring = "+:nm:o:";

      int opt;
      while ((opt = getopt(argc, argv, optstring)) != -1) {
	switch (opt) {
	  case 'm':
	    type = vik::Window::window_type_from_string(optarg);
	    if (type == vik::Window::INVALID)
	      vik_log_f("option -m given bad display mode");
	    break;
	  case '?':
	    vik_log_f("invalid option '-%c'", optopt);
	    break;
	  case ':':
	    vik_log_f("option -%c requires an argument", optopt);
	    break;
	  default:
	    assert(!"unreachable");
	    break;
	}
      }

      if (optind != argc)
	vik_log_w("trailing args");
    }

};
}
