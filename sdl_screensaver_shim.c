/* SPDX-License-Identifier: MIT */

/*
 * Original code from:
 * https://gist.githubusercontent.com/InfoTeddy/4d41a5b5b5fc39f52666923a12cfce1e/
 * */

/*
 * On Linux, Steam periodically calls SDL_DisableScreenSaver() so your
 * screensaver doesn't work with the Steam client open even if you aren't
 * playing a game, as described in
 * https://github.com/ValveSoftware/steam-for-linux/issues/5607 .
 *
 * To fix this, LD_PRELOAD a library that replaces SDL_DisableScreenSaver()
 * with a no-op if the executable calling it is Steam, but otherwise let it
 * through for other applications. (And print some messages for debugging.)
 *
 * Compile this file with
 *     gcc -shared -fPIC -ldl -m32 -o fix_steam_screensaver_lib.so
 * fix_steam_screensaver.c
 *
 *     gcc -shared -fPIC -ldl -m64 -o fix_steam_screensaver_lib64.so
 * fix_steam_screensaver.c
 *
 * and launch Steam with
 *     LD_PRELOAD="fix_steam_screensaver_\$LIB.so"
 * .
 */

#include <time.h>
/* RTLD_NEXT is a GNU extension. */
#ifndef _GNU_SOURCE
#error Must be compiled with -D_GNU_SOURCE
#endif
#include <dlfcn.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef __linux__
#error Platform not supported.
#endif

#define CONFIG_FILE "sdl_screensaver_shim/banlist.conf"

#ifdef __i386__
#define ARCH "i386"
#elif defined(__amd64__)
#define ARCH "amd64"
#else
#error Architecture not supported.
#endif

static void (*real_function)(void);

struct list_entry {
  struct list_entry *next;
  char *value;
};

static struct list_entry *banlist = NULL;
static char exe_name[1024];

static void vlog(const char *text) {
  fprintf(stderr, "[" ARCH "] %s: %s\n", exe_name, text);
}

static void call_real_function(void) {
  if (real_function == NULL) {
    /* Thankfully SDL_DisableScreenSaver() only exists since SDL
     * 2.0, else I'd have to detect the SDL version. */
    real_function =
        dlvsym(RTLD_NEXT, "SDL_DisableScreenSaver", "libSDL2-2.0.so.0");

    if (real_function == NULL) {
      printf("no real function, retry...\n");
      real_function = dlsym(RTLD_NEXT, "SDL_DisableScreenSaver");
    }

    /* Oh god I hope it works, I don't want to implement more
     * libTAS logic... */

    if (real_function != NULL)
      vlog("Successfully linked SDL_DisableScreenSaver().");
  }

  if (real_function != NULL) {
    vlog("Allowing SDL_DisableScreenSaver().");
    real_function();
  } else
    vlog("Could not link SDL_DisableScreenSaver().");
}

static const char *find_config_file(void) {
  static int result_valid = 0;
  static char result[1024];

  if (result_valid)
    return result;

  result[0] = '\0';
  result_valid = 1;

  char *config_dir = getenv("XDG_CONFIG_HOME");
  if (!config_dir) {
    const char *home = getenv("HOME");
    if (!home) {
      vlog("Error: could not find $HOME!");
      return result;
    }

    snprintf(result, sizeof(result), "%s/.config/%s", home, CONFIG_FILE);
  } else {
    snprintf(result, sizeof(result), "%s/%s", config_dir, CONFIG_FILE);
  }
  return result;
}

static void refresh_banlist(void) {
  struct stat attr;
  struct timespec config_mtime;
  const char *config_path = find_config_file();
  if (!config_path || stat(config_path, &attr) != 0) {
    vlog("Can't find config file!");
    return;
  }

  if (config_mtime.tv_nsec != attr.st_mtim.tv_nsec ||
      config_mtime.tv_sec != attr.st_mtim.tv_sec) {
    memcpy(&config_mtime, &attr.st_mtim, sizeof(config_mtime));

    while (banlist != NULL) {
    struct list_entry *next = banlist->next;
      free(banlist->value);
      free(banlist);
      banlist = next;
    }

    FILE *fp = fopen(config_path, "r");
    if (!fp) {
      vlog("Could not open config file!");
      return;
    }

    char buf[1024];
    struct list_entry *tail = banlist;
    while (1) {
      if (fgets(buf, sizeof(buf), fp) == NULL)
        break;
      size_t line_size = strnlen(buf, sizeof(buf));
      if (line_size > 0 && buf[line_size - 1] == '\n') {
        buf[line_size - 1] = '\0';
        --line_size;
      }
      if (line_size == 0)
        continue;

      struct list_entry *next = malloc(sizeof(struct list_entry));
      next->next = NULL;
      next->value = malloc(line_size + 1);
      strncpy(next->value, buf, line_size);
      if (!tail) {
        tail = next;
        banlist = next;
      } else {
        tail->next = next;
        tail = next;
      }
    }
  }
}

static int check_exe(void) {
  if (exe_name[0] == '\0') {
    ssize_t len = readlink("/proc/self/exe", exe_name, sizeof(exe_name) - 1);

    if (len == -1)
      strcpy(exe_name, "(unknown)");
    else
      exe_name[len] = '\0';
  }

  refresh_banlist();

  struct list_entry *entry = banlist;
  while (entry) {
    if (fnmatch(entry->value, exe_name, 0) == 0) {
      return 0;
    }
    entry = entry->next;
  }

  return 1;
}

void SDL_DisableScreenSaver(void) {
  if (check_exe()) {
    call_real_function();
  } else {
    vlog("Prevented SDL_DisableScreenSaver().");
  }
}
