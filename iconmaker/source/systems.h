/* The systems Icon Maker makes icons for: the app a forwarder starts, and
 * which games are its. gpSP 3D (GBA) and Snes9x 3D (SNES) are built in;
 * more come from small text files, one per system, in SYS_DIR:
 *
 *   name=rmxp3ds                          the app's name, also the icons'
 *                                         publisher ("My icons" finds them by it)
 *   console=RMXP                          short tag on the banner and in the list
 *   title_id=0x000400000ABCD000           the app's CIA title ID
 *   launch=sdmc:/rmxp3ds/launch.txt       the file the app reads the game's path
 *                                         from ("" or left out: gpSP 3D's own way)
 *   extensions=.gba,.zip                  game files
 *   folder_game=Game.ini,mkxp.json        a folder holding one of these is a game
 *   title_from=Game.ini:Game:Title        a folder game's name: file:section:key
 *   icon=generic                          gba, snes, generic or sdmc:/....png
 *   tag_color=#E5604F                     its name's colour on the banner
 *   boxart=none                           libretro-thumbnails folder, or none
 *
 * name, console, title_id and extensions or folder_game are needed. A file
 * whose name is a built-in system's replaces it. Keys it doesn't know are
 * left out (so a file for a newer Icon Maker still loads). The files are
 * untrusted: every value is checked, a bad file is skipped with a note.
 *
 * Plain C, so it also builds on a PC (test/systemstest.c). */
#ifndef SYSTEMS_H
#define SYSTEMS_H
#include <stddef.h>
#include <stdint.h>

#define SYS_DIR          "sdmc:/3ds/gpsp3d/iconmaker/systems"
#define SYS_MAX          16      /* systems at most, the built-in ones too */
#define SYS_NAME_MAX     63      /* fits the SMDH publisher */
#define SYS_CONSOLE_MAX  15
#define SYS_LAUNCH_MAX   127     /* as fwdcia_make checks */
#define SYS_PATH_MAX     511     /* a game's path, as fwdcia_make checks */
#define SYS_EXT_N        8       /* extensions per system */
#define SYS_EXT_MAX      15      /* ".gba": the dot and up to 14 letters or digits */
#define SYS_MARKER_N     4       /* folder_game files per system */
#define SYS_WORD_MAX     63      /* a folder_game file; title_from's file, section, key */
#define SYS_ICON_MAX     255     /* icon=sdmc:/....png */
#define SYS_BOXART_MAX   127
#define SYS_FILE_MAX     4096    /* a system file's size */
#define SYS_LINE_MAX     400     /* a line in it */
#define SYS_NOTES        8       /* skipped files with their reason kept */

/* the built-in ones' places (a file can replace them, in their place) */
enum { SYS_GBA, SYS_SNES, SYS_BUILTIN };

/* icon=: a cartridge (deficon.h's ICON_GBA, ICON_SNES), a plain tile with
 * the name, or a picture */
enum { SYS_STYLE_GBA, SYS_STYLE_SNES, SYS_STYLE_GENERIC, SYS_STYLE_PNG };

typedef struct
{
   char name[SYS_NAME_MAX + 1];
   char console[SYS_CONSOLE_MAX + 1];
   uint64_t tid;
   char launch[SYS_LAUNCH_MAX + 1];
   char ext[SYS_EXT_N][SYS_EXT_MAX + 1];
   int next;
   char folder_game[SYS_MARKER_N][SYS_WORD_MAX + 1];
   int nfolder_game;
   char title_file[SYS_WORD_MAX + 1], title_section[SYS_WORD_MAX + 1], title_key[SYS_WORD_MAX + 1];
   int style;
   char icon_png[SYS_ICON_MAX + 1];
   uint32_t tag_rgb;
   char boxart[SYS_BOXART_MAX + 1];     /* "": none */
   char file[SYS_WORD_MAX + 1];         /* the file it came from, "" if built in */
} sys_desc;

/* a file that was skipped, and why */
typedef struct { char file[SYS_WORD_MAX + 1]; char why[80]; } sys_note;

/* the built-in systems into list[0], list[1]; returns SYS_BUILTIN */
int sys_builtin(sys_desc *list);

/* One system file's text (len bytes, need not end in 0) into s. 0, or -1
 * with the reason in why. */
int sys_parse(const char *text, size_t len, sys_desc *s, char *why, int whylen);

/* The *.cfg files in dir (in name order) after the n systems already in
 * list (room for SYS_MAX). Returns the number of systems now. Skipped
 * files go to notes (at most SYS_NOTES; *nnotes counts them all). */
int sys_load(const char *dir, sys_desc *list, int n, sys_note *notes, int *nnotes);

/* Which of the n systems in list a game is for, or -1 if it isn't a game.
 * The first system whose extensions have the file's wins. A .zip is for
 * the first system that has both .zip and the extension of the zip's first
 * file, else for the first system that has .zip (GBA, built in).
 * sys_of_ext: by an extension (".gba", the dot included).
 * sys_of_zip: a zip whose first file is named first (NULL: unknown).
 * sys_of_file: a file on the SD card (reads a zip's first header). */
int sys_of_ext(const sys_desc *list, int n, const char *ext);
int sys_of_zip(const sys_desc *list, int n, const char *first);
int sys_of_file(const sys_desc *list, int n, const char *path);

#endif
