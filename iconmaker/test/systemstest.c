/* PC test for systems.c: system files read as Icon Maker reads them, good
 * ones and every way a file can be bad. No arguments; prints each failed
 * check and exits 1 if any failed:
 *   gcc -I../source systemstest.c ../source/systems.c -o systemstest && ./systemstest */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../source/systems.h"

static int fails, checks;

#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static char why[100];
static sys_desc s;

static int parse(const char *text)
{
   why[0] = 0;
   return sys_parse(text, strlen(text), &s, why, sizeof(why));
}

/* parse a file that's good but for one line, which must fail with a reason
 * containing want */
static void bad(int line, const char *text, const char *want)
{
   checks++;
   if (parse(text) == 0)
   {
      fails++;
      printf("FAIL line %d: accepted:\n%s\n", line, text);
   }
   else if (!strstr(why, want))
   {
      fails++;
      printf("FAIL line %d: reason '%s', wanted '%s'\n", line, why, want);
   }
}
#define BAD(text, want) bad(__LINE__, text, want)

#define BASE "name=Test\nconsole=TST\ntitle_id=0x0004000000ABC000\nextensions=.tst\n"

/* the files of the plan (iconmaker-plugin-plan.md): the built-in ones as files */
static const char gba_cfg[] =
   "# gba.cfg\nname=gpSP 3D\nconsole=GBA\ntitle_id=0x000400000BAC8100\nlaunch=\n"
   "extensions=.gba,.zip\nicon=gba\ntag_color=#23D5C8\nboxart=Nintendo_-_Game_Boy_Advance\n";
static const char snes_cfg[] =
   "# snes.cfg\nname=Snes9x 3D\nconsole=SNES\ntitle_id=0x00040000038A7100\n"
   "launch=sdmc:/3ds/snes9x_3ds/launch.txt\nextensions=.sfc,.smc,.fig,.swc,.zip\nicon=snes\n"
   "tag_color=#B59CFF\nboxart=Nintendo_-_Super_Nintendo_Entertainment_System\n";
static const char rmxp_cfg[] =
   "# rmxp3ds.cfg\r\nname=rmxp3ds\r\nconsole=RMXP\r\ntitle_id=0x0004000000F3E100\r\n"
   "launch=sdmc:/rmxp3ds/launch.txt\r\nfolder_game=Game.ini,mkxp.json\r\n"
   "title_from=Game.ini:Game:Title\r\nicon=sdmc:/3ds/rmxp3ds/icon.png\r\ntag_color=#E5604F\r\n"
   "boxart=none\r\n";

static void test_good(void)
{
   sys_desc b[SYS_BUILTIN];
   CHECK(sys_builtin(b) == SYS_BUILTIN);
   CHECK(parse(gba_cfg) == 0);
   CHECK(!memcmp(&s, &b[SYS_GBA], sizeof(s)));
   CHECK(parse(snes_cfg) == 0);
   CHECK(!memcmp(&s, &b[SYS_SNES], sizeof(s)));

   CHECK(parse(rmxp_cfg) == 0);
   CHECK(!strcmp(s.name, "rmxp3ds"));
   CHECK(!strcmp(s.console, "RMXP"));
   CHECK(s.tid == 0x0004000000F3E100ULL);
   CHECK(!strcmp(s.launch, "sdmc:/rmxp3ds/launch.txt"));
   CHECK(s.next == 0);
   CHECK(s.nfolder_game == 2 && !strcmp(s.folder_game[0], "Game.ini") && !strcmp(s.folder_game[1], "mkxp.json"));
   CHECK(!strcmp(s.title_file, "Game.ini") && !strcmp(s.title_section, "Game") && !strcmp(s.title_key, "Title"));
   CHECK(s.style == SYS_STYLE_PNG && !strcmp(s.icon_png, "sdmc:/3ds/rmxp3ds/icon.png"));
   CHECK(s.tag_rgb == 0xE5604F);
   CHECK(s.boxart[0] == 0);

   /* the least: defaults for the rest */
   CHECK(parse(BASE) == 0);
   CHECK(s.launch[0] == 0 && s.style == SYS_STYLE_GENERIC && s.boxart[0] == 0 && s.tag_rgb == 0xC5CBD6);
   CHECK(s.next == 1 && !strcmp(s.ext[0], ".tst"));
   /* folder_game alone is enough */
   CHECK(parse("name=a\nconsole=b\ntitle_id=0x0004000000ABC000\nfolder_game=Game.ini\n") == 0);

   /* spaces, tabs, comments, a BOM, unknown keys, no newline at the end */
   CHECK(parse("\xEF\xBB\xBF  # comment\n; also one\n\n\t name \t=  Spaced Name  \nconsole = X\n"
               "title_id=0x0004000000ABC000\nextensions = .a , .B1 ,.zip\nfuture_key=whatever\n"
               "icon=GENERIC\nboxart=NONE") == 0);
   CHECK(!strcmp(s.name, "Spaced Name") && !strcmp(s.console, "X"));
   CHECK(s.next == 3 && !strcmp(s.ext[1], ".B1") && !strcmp(s.ext[2], ".zip"));
   /* empty values */
   CHECK(parse(BASE "launch=\nicon=\nboxart=\ntitle_from=\n") == 0);
   CHECK(s.style == SYS_STYLE_GENERIC && s.title_file[0] == 0);
   CHECK(parse(BASE "icon=snes\n") == 0 && s.style == SYS_STYLE_SNES);
   CHECK(parse(BASE "tag_color=#abcdef\n") == 0 && s.tag_rgb == 0xABCDEF);
   CHECK(parse(BASE "title_from=a b.ini:My Section:Key 1\n") == 0 &&
         !strcmp(s.title_file, "a b.ini") && !strcmp(s.title_section, "My Section") && !strcmp(s.title_key, "Key 1"));
   CHECK(parse("name=a\nconsole=b\ntitle_id=0x40000000ABC00\nextensions=.x\n") == 0 && s.tid == 0x40000000ABC00ULL);
}

static void test_limits(void)
{
   char t[2000], v[600];

   /* name: 63 fits, 64 doesn't */
   memset(v, 'n', 63); v[63] = 0;
   snprintf(t, sizeof(t), "name=%s\nconsole=X\ntitle_id=0x0004000000ABC000\nextensions=.x\n", v);
   CHECK(parse(t) == 0 && strlen(s.name) == 63);
   strcat(v, "n");
   snprintf(t, sizeof(t), "name=%s\nconsole=X\ntitle_id=0x0004000000ABC000\nextensions=.x\n", v);
   BAD(t, "name");
   BAD("name=\nconsole=X\ntitle_id=0x0004000000ABC000\nextensions=.x\n", "name");

   /* console: 15 / 16 */
   CHECK(parse("name=a\nconsole=123456789012345\ntitle_id=0x0004000000ABC000\nextensions=.x\n") == 0);
   BAD("name=a\nconsole=1234567890123456\ntitle_id=0x0004000000ABC000\nextensions=.x\n", "console");
   BAD("name=a\nconsole=\ntitle_id=0x0004000000ABC000\nextensions=.x\n", "console");

   /* launch: 127 / 128, and on the SD card */
   memset(v, 'l', 127); memcpy(v, "sdmc:/", 6); v[127] = 0;
   snprintf(t, sizeof(t), BASE "launch=%s\n", v);
   CHECK(parse(t) == 0 && strlen(s.launch) == 127);
   strcat(v, "l");
   snprintf(t, sizeof(t), BASE "launch=%s\n", v);
   BAD(t, "launch");
   BAD(BASE "launch=romfs:/x.txt\n", "launch");

   /* extensions: 8 / 9, 15 letters / 16, what's in them */
   CHECK(parse("name=a\nconsole=b\ntitle_id=0x0004000000ABC000\nextensions=.1,.2,.3,.4,.5,.6,.7,.8\n") == 0 && s.next == 8);
   BAD("name=a\nconsole=b\ntitle_id=0x0004000000ABC000\nextensions=.1,.2,.3,.4,.5,.6,.7,.8,.9\n", "more than 8");
   CHECK(parse("name=a\nconsole=b\ntitle_id=0x0004000000ABC000\nextensions=.12345678901234\n") == 0);
   BAD("name=a\nconsole=b\ntitle_id=0x0004000000ABC000\nextensions=.123456789012345\n", "extensions");
   BAD("name=a\nconsole=b\ntitle_id=0x0004000000ABC000\nextensions=gba\n", "extensions");
   BAD("name=a\nconsole=b\ntitle_id=0x0004000000ABC000\nextensions=.\n", "extensions");
   BAD("name=a\nconsole=b\ntitle_id=0x0004000000ABC000\nextensions=.g/a\n", "extensions");
   BAD("name=a\nconsole=b\ntitle_id=0x0004000000ABC000\nextensions=.gba,,.zip\n", "extensions");

   /* folder_game: 4 / 5, 63 / 64, plain file names only */
   CHECK(parse("name=a\nconsole=b\ntitle_id=0x0004000000ABC000\nfolder_game=a,b,c,d\n") == 0 && s.nfolder_game == 4);
   BAD("name=a\nconsole=b\ntitle_id=0x0004000000ABC000\nfolder_game=a,b,c,d,e\n", "more than 4");
   memset(v, 'f', 63); v[63] = 0;
   snprintf(t, sizeof(t), "name=a\nconsole=b\ntitle_id=0x0004000000ABC000\nfolder_game=%s\n", v);
   CHECK(parse(t) == 0);
   strcat(v, "f");
   snprintf(t, sizeof(t), "name=a\nconsole=b\ntitle_id=0x0004000000ABC000\nfolder_game=%s\n", v);
   BAD(t, "folder_game");
   BAD("name=a\nconsole=b\ntitle_id=0x0004000000ABC000\nfolder_game=../Game.ini\n", "folder_game");
   BAD("name=a\nconsole=b\ntitle_id=0x0004000000ABC000\nfolder_game=..\n", "folder_game");
   BAD("name=a\nconsole=b\ntitle_id=0x0004000000ABC000\nfolder_game=sdmc:x\n", "folder_game");

   /* title_from */
   BAD(BASE "title_from=Game.ini:Title\n", "title_from");
   BAD(BASE "title_from=Game.ini:Game:Title:x\n", "title_from");
   BAD(BASE "title_from=:Game:Title\n", "title_from");
   BAD(BASE "title_from=Game.ini::Title\n", "title_from");
   BAD(BASE "title_from=Game.ini:Game:\n", "title_from");
   BAD(BASE "title_from=../Game.ini:Game:Title\n", "title_from");
   BAD(BASE "title_from=Game.ini:Ga]me:Title\n", "title_from");
   memset(v, 'k', 64); v[64] = 0;
   snprintf(t, sizeof(t), BASE "title_from=Game.ini:Game:%s\n", v);
   BAD(t, "title_from");

   /* icon */
   BAD(BASE "icon=nes\n", "icon");
   BAD(BASE "icon=sdmc:/icon.jpg\n", "icon");
   BAD(BASE "icon=/3ds/icon.png\n", "icon");
   CHECK(parse(BASE "icon=sdmc:/a.PNG\n") == 0 && s.style == SYS_STYLE_PNG);
   memset(v, 'i', 255); memcpy(v, "sdmc:/", 6); memcpy(v + 251, ".png", 5);
   snprintf(t, sizeof(t), BASE "icon=%s\n", v);
   CHECK(parse(t) == 0 && strlen(s.icon_png) == 255);
   memset(v, 'i', 256); memcpy(v, "sdmc:/", 6); memcpy(v + 252, ".png", 5);
   snprintf(t, sizeof(t), BASE "icon=%s\n", v);
   BAD(t, "icon");

   /* boxart: a folder name only (it goes into URLs) */
   BAD(BASE "boxart=../x\n", "boxart");
   BAD(BASE "boxart=a/b\n", "boxart");
   BAD(BASE "boxart=a b\n", "boxart");
   BAD(BASE "boxart=.hidden\n", "boxart");
   memset(v, 'b', 128); v[128] = 0;
   snprintf(t, sizeof(t), BASE "boxart=%s\n", v);
   BAD(t, "boxart");
   v[127] = 0;
   snprintf(t, sizeof(t), BASE "boxart=%s\n", v);
   CHECK(parse(t) == 0 && strlen(s.boxart) == 127);

   /* a line over 400 letters (a key that isn't known, too) */
   memset(v, 'x', 400); v[400] = 0;
   snprintf(t, sizeof(t), BASE "x=%s\n", v);
   BAD(t, "line 5");
   /* the file over 4096 bytes */
   {
      static char big[5000];
      size_t n, i;
      strcpy(big, BASE);
      n = strlen(big);
      memset(big + n, '#', 4097 - n);      /* comment lines of 80 */
      for (i = n + 80; i < 4097; i += 81)
         big[i] = '\n';
      big[4097] = 0;
      BAD(big, "bigger");
      big[4096] = 0;
      CHECK(parse(big) == 0);
   }
   /* not text */
   {
      static const char nul[] = "name=a\0\nconsole=b\n";
      CHECK(sys_parse(nul, sizeof(nul) - 1, &s, why, sizeof(why)) && strstr(why, "text"));
   }
   BAD(BASE "launch=sdmc:/l\xC3\xA4unch.txt\n", "ASCII");
   BAD(BASE "launch=sdmc:/a\tb\n", "ASCII");
   BAD(BASE "just some words\n", "line 5");
}

static void test_missing(void)
{
   BAD("console=X\ntitle_id=0x0004000000ABC000\nextensions=.x\n", "name=");
   BAD("name=a\ntitle_id=0x0004000000ABC000\nextensions=.x\n", "console=");
   BAD("name=a\nconsole=X\nextensions=.x\n", "title_id=");
   BAD("name=a\nconsole=X\ntitle_id=0x0004000000ABC000\n", "extensions= or folder_game=");
   BAD("name=a\nconsole=X\ntitle_id=0x0004000000ABC000\nextensions=\nfolder_game=\n", "extensions= or folder_game=");
   BAD("", "name=");
}

static void test_numbers(void)
{
   BAD("name=a\nconsole=b\ntitle_id=0x\nextensions=.x\n", "hex");
   BAD("name=a\nconsole=b\ntitle_id=0004000000ABC000\nextensions=.x\n", "hex");
   BAD("name=a\nconsole=b\ntitle_id=0x0004000000ABCG00\nextensions=.x\n", "hex");
   BAD("name=a\nconsole=b\ntitle_id=0x00004000000ABC000\nextensions=.x\n", "hex");   /* 17 digits */
   BAD("name=a\nconsole=b\ntitle_id=-0x0004000000ABC000\nextensions=.x\n", "hex");
   BAD("name=a\nconsole=b\ntitle_id=0x0004000000XXXX00\nextensions=.x\n", "hex");   /* the plan's placeholder */
   BAD("name=a\nconsole=b\ntitle_id=0x0004001000ABC000\nextensions=.x\n", "0x00040000");
   BAD("name=a\nconsole=b\ntitle_id=0x00040000000000FF\nextensions=.x\n", "0x00040000");
   BAD("name=a\nconsole=b\ntitle_id=0x000400000C000000\nextensions=.x\n", "range");   /* the icons' own */
   BAD("name=a\nconsole=b\ntitle_id=0x000400000EFFFF00\nextensions=.x\n", "range");
   CHECK(parse("name=a\nconsole=b\ntitle_id=0x000400000BFFFF00\nextensions=.x\n") == 0);
   CHECK(parse("name=a\nconsole=b\ntitle_id=0x000400000F000000\nextensions=.x\n") == 0);
   BAD(BASE "tag_color=23D5C8\n", "tag_color");
   BAD(BASE "tag_color=#23D5C\n", "tag_color");
   BAD(BASE "tag_color=#23D5C8F\n", "tag_color");
   BAD(BASE "tag_color=#23D5CG\n", "tag_color");
   BAD(BASE "tag_color=#-3D5C8\n", "tag_color");
}

static void test_duplicate_keys(void)
{
   BAD(BASE "name=b\n", "line 5: name is there twice");
   BAD(BASE "extensions=.y\n", "twice");
   BAD(BASE "launch=\nlaunch=\n", "twice");
}

/* ---- the folder ---- */

static char dir[] = "/tmp/systemstest.XXXXXX";

static void put(const char *name, const char *text)
{
   char p[300];
   FILE *f;
   snprintf(p, sizeof(p), "%s/%s", dir, name);
   f = fopen(p, "wb");
   fputs(text, f);
   fclose(f);
}

static void clean(void)
{
   char cmd[300];
   snprintf(cmd, sizeof(cmd), "rm -rf '%s'/* '%s'/.[!.]*", dir, dir);
   if (system(cmd)) {}
}

static int has_note(const sys_note *notes, int n, const char *file, const char *want)
{
   int i;
   for (i = 0; i < n && i < SYS_NOTES; i++)
      if (!strcmp(notes[i].file, file) && strstr(notes[i].why, want))
         return 1;
   return 0;
}

static void test_folder(void)
{
   static sys_desc list[SYS_MAX];
   sys_note notes[SYS_NOTES];
   char name[300], text[300], p[300];
   int n, nnotes, i;

   CHECK(mkdtemp(dir) != NULL);

   /* no folder: the built-in ones */
   n = sys_load("/tmp/no/such/folder", list, sys_builtin(list), notes, &nnotes);
   CHECK(n == 2 && nnotes == 0);

   /* the plan's three files: GBA and SNES replaced in place (the same), rmxp3ds added;
    * other files, folders and hidden files left alone */
   put("gba.cfg", gba_cfg);
   put("snes.cfg", snes_cfg);
   put("rmxp3ds.cfg", rmxp_cfg);
   put("readme.txt", "name=x\n");
   put(".hidden.cfg", "broken");
   snprintf(p, sizeof(p), "%s/folder.cfg", dir);
   mkdir(p, 0777);
   n = sys_load(dir, list, sys_builtin(list), notes, &nnotes);
   CHECK(n == 3 && nnotes == 0);
   CHECK(!strcmp(list[SYS_GBA].name, "gpSP 3D") && !strcmp(list[SYS_GBA].file, "gba.cfg"));
   CHECK(!strcmp(list[SYS_SNES].name, "Snes9x 3D") && !strcmp(list[SYS_SNES].file, "snes.cfg"));
   CHECK(!strcmp(list[2].name, "rmxp3ds") && !strcmp(list[2].file, "rmxp3ds.cfg"));
   clean();

   /* a built-in one replaced keeps its place; bad files noted, the rest read */
   put("a_snes.cfg", "name=Snes9x 3D\nconsole=SFC\ntitle_id=0x00040000038A7100\nextensions=.sfc\n");
   put("b_bad.cfg", "name=Bad\nconsole=B\ntitle_id=0x0004000000XXXX00\nextensions=.b\n");
   put("c_good.cfg", "name=Good\nconsole=G\ntitle_id=0x0004000000ABC000\nextensions=.g\n");
   put("d_same.cfg", "name=Good\nconsole=G2\ntitle_id=0x0004000000ABD000\nextensions=.g2\n");
   put("e_snes_again.cfg", "name=Snes9x 3D\nconsole=S2\ntitle_id=0x00040000038A7100\nextensions=.s2\n");
   put("F_upper.CFG", "name=Upper\nconsole=U\ntitle_id=0x0004000000ABE000\nextensions=.u\n");
   put("g_empty.cfg", "");
   n = sys_load(dir, list, sys_builtin(list), notes, &nnotes);
   CHECK(n == 4);
   CHECK(!strcmp(list[SYS_GBA].console, "GBA") && !list[SYS_GBA].file[0]);
   CHECK(!strcmp(list[SYS_SNES].console, "SFC") && list[SYS_SNES].next == 1);
   CHECK(!strcmp(list[2].name, "Good") && !strcmp(list[3].name, "Upper"));
   CHECK(nnotes == 4);
   CHECK(has_note(notes, nnotes, "b_bad.cfg", "hex"));
   CHECK(has_note(notes, nnotes, "d_same.cfg", "same name as c_good.cfg"));
   CHECK(has_note(notes, nnotes, "e_snes_again.cfg", "same name as a_snes.cfg"));
   CHECK(has_note(notes, nnotes, "g_empty.cfg", "name="));
   clean();

   /* a file too big, a file name too long */
   {
      static char big[6000];
      memset(big, '#', sizeof(big) - 1);
      put("big.cfg", big);
   }
   memset(name, 'x', 64);
   strcpy(name + 60, ".cfg");
   put(name, "name=Long\nconsole=L\ntitle_id=0x0004000000ABC000\nextensions=.l\n");
   n = sys_load(dir, list, sys_builtin(list), notes, &nnotes);
   CHECK(n == 2 && nnotes == 2);
   CHECK(has_note(notes, nnotes, "big.cfg", "bigger"));
   CHECK(notes[0].why[0] && (strstr(notes[0].why, "too long") || strstr(notes[1].why, "too long")));
   clean();

   /* more than 16 systems: 2 built in + 14 files, the rest (in name order) noted */
   for (i = 0; i < 20; i++)
   {
      snprintf(name, sizeof(name), "s%02d.cfg", i);
      snprintf(text, sizeof(text), "name=S%02d\nconsole=S\ntitle_id=0x0004000000A%02X000\nextensions=.s\n", i, i);
      put(name, text);
   }
   n = sys_load(dir, list, sys_builtin(list), notes, &nnotes);
   CHECK(n == SYS_MAX && nnotes == 6);
   CHECK(!strcmp(list[2].name, "S00") && !strcmp(list[15].name, "S13"));
   CHECK(has_note(notes, nnotes, "s14.cfg", "more than 16") && has_note(notes, nnotes, "s19.cfg", "more than 16"));
   /* replacing a built-in one still works when the list is full */
   put("zz_gba.cfg", "name=gpSP 3D\nconsole=AGB\ntitle_id=0x000400000BAC8100\nextensions=.gba\n");
   n = sys_load(dir, list, sys_builtin(list), notes, &nnotes);
   CHECK(n == SYS_MAX && !strcmp(list[SYS_GBA].console, "AGB") && nnotes == 6);
   clean();

   /* more than 64 files: only 64 looked at; notes kept to SYS_NOTES, all counted */
   for (i = 0; i < 70; i++)
   {
      snprintf(name, sizeof(name), "m%02d.cfg", i);
      snprintf(text, sizeof(text), "name=M%02d\nconsole=M\ntitle_id=0x0004000000B%02X000\nextensions=.m\n", i, i);
      put(name, text);
   }
   n = sys_load(dir, list, sys_builtin(list), notes, &nnotes);
   CHECK(n == SYS_MAX && nnotes == 64 - 14 + 1);
   clean();
   rmdir(dir);
}

/* random files made of pieces of good ones: never a crash (build with
 * -fsanitize=address to see writes past a buffer), and what's accepted
 * keeps to the limits */
static void test_random(void)
{
   static const char *const piece[] = {
      "name=", "console=", "title_id=", "launch=", "extensions=", "folder_game=", "title_from=",
      "icon=", "tag_color=", "boxart=", "0x0004000000ABC000", "sdmc:/", ".gba", ",", ":", "=",
      "#", "\n", "\r\n", " ", "\t", "Game.ini", "none", "generic", ".png", "\xC3\xA4", "\0",
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", "]", "/", ".."
   };
   static char text[SYS_FILE_MAX + 200];
   static const char *const good[] = {
      "name=R\n", "console=R\n", "title_id=0x0004000000ABC000\n", "extensions=.r\n", "folder_game=g\n"
   };
   unsigned seed = 12345;
   int i, ok = 0;
#define RND(m) ((int)(((seed = seed * 1103515245 + 12345) >> 16) % (m)))
   for (i = 0; i < 200000; i++)
   {
      size_t n = 0;
      int k, lines = RND(12);
      /* half of them start with the keys needed, so some get through */
      if (i & 1)
         for (k = 0; k < 4; k++)
         {
            const char *g = good[k == 3 ? 3 + RND(2) : k];
            memcpy(text + n, g, strlen(g));
            n += strlen(g);
         }
      for (k = 0; k < lines; k++)
      {
         int parts = RND(5), j;
         /* a key (the first ten pieces), then a few random pieces */
         const char *p = piece[RND(10)];
         for (j = 0; j <= parts; j++, p = piece[RND(sizeof(piece) / sizeof(piece[0]))])
         {
            size_t len = *p ? strlen(p) : 1;
            if (n + len + 1 > sizeof(text))
               break;
            memcpy(text + n, p, len);
            n += len;
         }
         if (n < sizeof(text))
            text[n++] = '\n';
      }
      if (sys_parse(text, n, &s, why, sizeof(why)) == 0)
      {
         ok++;
         if (strlen(s.name) > SYS_NAME_MAX || !s.name[0] || strlen(s.console) > SYS_CONSOLE_MAX ||
             strlen(s.launch) > SYS_LAUNCH_MAX || s.next > SYS_EXT_N || s.nfolder_game > SYS_MARKER_N ||
             (!s.next && !s.nfolder_game))
         {
            fails++;
            printf("FAIL: random file %d accepted out of limits\n", i);
         }
      }
      checks++;
   }
   printf("random files: %d of 200000 accepted\n", ok);
}

/* a zip in dir whose first file is named first (its local header; the rest
 * isn't read), or len bytes of junk if first is NULL */
static void put_zip(const char *name, const char *first, int len)
{
   unsigned char hdr[30] = { 'P', 'K', 3, 4 };
   char p[300];
   FILE *f;
   snprintf(p, sizeof(p), "%s/%s", dir, name);
   f = fopen(p, "wb");
   if (first)
   {
      int n = strlen(first);
      hdr[26] = n & 255;
      hdr[27] = n >> 8;
      fwrite(hdr, 1, 30, f);
      fwrite(first, 1, n, f);
   }
   else
      while (len-- > 0)
         fputc('z', f);
   fclose(f);
}

static int of_file(const sys_desc *list, int n, const char *name)
{
   char p[300];
   snprintf(p, sizeof(p), "%s/%s", dir, name);
   return sys_of_file(list, n, p);
}

static void test_detect(void)
{
   static sys_desc list[SYS_MAX];
   char longname[400];
   int n = sys_builtin(list), i;

   /* the built-in ones, as before */
   CHECK(sys_of_ext(list, n, ".gba") == SYS_GBA);
   CHECK(sys_of_ext(list, n, ".GBA") == SYS_GBA);
   CHECK(sys_of_ext(list, n, ".sfc") == SYS_SNES);
   CHECK(sys_of_ext(list, n, ".SMC") == SYS_SNES);
   CHECK(sys_of_ext(list, n, ".fig") == SYS_SNES);
   CHECK(sys_of_ext(list, n, ".swc") == SYS_SNES);
   CHECK(sys_of_ext(list, n, ".zip") == SYS_GBA);
   CHECK(sys_of_ext(list, n, ".nes") == -1);
   CHECK(sys_of_ext(list, n, "gba") == -1);
   CHECK(sys_of_ext(list, n, ".") == -1);
   CHECK(sys_of_ext(list, n, "") == -1);
   CHECK(sys_of_ext(list, n, NULL) == -1);

   /* a zip by its first file */
   CHECK(sys_of_zip(list, n, "Super Metroid (USA).sfc") == SYS_SNES);
   CHECK(sys_of_zip(list, n, "roms/Super Metroid.SMC") == SYS_SNES);
   CHECK(sys_of_zip(list, n, "Golf.gba") == SYS_GBA);
   CHECK(sys_of_zip(list, n, "readme.txt") == SYS_GBA);
   CHECK(sys_of_zip(list, n, "inner.zip") == SYS_GBA);
   CHECK(sys_of_zip(list, n, "game.sfc/") == SYS_GBA);
   CHECK(sys_of_zip(list, n, "dir.sfc/game") == SYS_GBA);
   CHECK(sys_of_zip(list, n, "noext") == SYS_GBA);
   CHECK(sys_of_zip(list, n, "") == SYS_GBA);
   CHECK(sys_of_zip(list, n, NULL) == SYS_GBA);

   /* files on disk */
   strcpy(dir, "/tmp/systemstest.XXXXXX");
   CHECK(mkdtemp(dir) != NULL);
   put("Golf.gba", "x");
   put("Mario.sfc", "x");
   put("notes.txt", "x");
   put("noext", "x");
   put_zip("snes.zip", "Super Metroid (USA).sfc", 0);
   put_zip("SNES2.ZIP", "Zelda.swc", 0);
   put_zip("gba.zip", "Golf.gba", 0);
   put_zip("other.zip", "readme.txt", 0);
   put_zip("junk.zip", NULL, 100);
   put_zip("empty.zip", NULL, 0);
   put_zip("short.zip", NULL, 20);
   memset(longname, 'a', sizeof(longname));
   strcpy(longname + 300, ".sfc");             /* cut to 255: no extension left */
   put_zip("long.zip", longname, 0);
   {
      /* a header that says its name is longer than the file */
      unsigned char hdr[34] = { 'P', 'K', 3, 4 };
      char p[300];
      FILE *f;
      hdr[26] = 200;
      memcpy(hdr + 30, "a.sf", 4);
      snprintf(p, sizeof(p), "%s/cut.zip", dir);
      f = fopen(p, "wb");
      fwrite(hdr, 1, sizeof(hdr), f);
      fclose(f);
   }
   CHECK(of_file(list, n, "Golf.gba") == SYS_GBA);
   CHECK(of_file(list, n, "Mario.sfc") == SYS_SNES);
   CHECK(of_file(list, n, "notes.txt") == -1);
   CHECK(of_file(list, n, "noext") == -1);
   CHECK(of_file(list, n, "snes.zip") == SYS_SNES);
   CHECK(of_file(list, n, "SNES2.ZIP") == SYS_SNES);
   CHECK(of_file(list, n, "gba.zip") == SYS_GBA);
   CHECK(of_file(list, n, "other.zip") == SYS_GBA);
   CHECK(of_file(list, n, "junk.zip") == SYS_GBA);
   CHECK(of_file(list, n, "empty.zip") == SYS_GBA);
   CHECK(of_file(list, n, "short.zip") == SYS_GBA);
   CHECK(of_file(list, n, "long.zip") == SYS_GBA);
   CHECK(of_file(list, n, "cut.zip") == SYS_GBA);
   CHECK(of_file(list, n, "missing.zip") == SYS_GBA);  /* as before: GBA if it can't be told */
   CHECK(sys_of_file(list, n, "/tmp/a.gba/game") == -1);

   /* a system from a file: its extensions, and zips of its games if it has .zip */
   CHECK(parse("name=Test\nconsole=TST\ntitle_id=0x0004000000ABC000\nextensions=.tst,.ZIP,.gba\n") == 0);
   list[n++] = s;
   CHECK(parse("name=NoZip\nconsole=NZ\ntitle_id=0x0004000000ABD000\nextensions=.nz\n") == 0);
   list[n++] = s;
   put("game.tst", "x");
   put("game.NZ", "x");
   put_zip("tst.zip", "game.tst", 0);
   put_zip("nz.zip", "game.nz", 0);
   CHECK(of_file(list, n, "game.tst") == 2);
   CHECK(of_file(list, n, "game.NZ") == 3);
   CHECK(of_file(list, n, "Golf.gba") == SYS_GBA);       /* the first system with .gba */
   CHECK(of_file(list, n, "tst.zip") == 2);
   CHECK(of_file(list, n, "nz.zip") == SYS_GBA);         /* NoZip can't open zips */
   CHECK(of_file(list, n, "snes.zip") == SYS_SNES);

   /* GBA replaced by a file without .zip: zips go to the next one that has it */
   CHECK(parse("name=gpSP 3D\nconsole=GBA\ntitle_id=0x000400000BAC8100\nextensions=.gba\n") == 0);
   list[SYS_GBA] = s;
   CHECK(of_file(list, n, "other.zip") == SYS_SNES);
   CHECK(of_file(list, n, "gba.zip") == 2);             /* Test has .gba and .zip */
   CHECK(of_file(list, n, "tst.zip") == 2);

   /* no system has .zip: a zip isn't a game, and isn't read */
   CHECK(of_file(list + 3, 1, "snes.zip") == -1);
   CHECK(sys_of_zip(list + 3, 1, "game.nz") == -1);
   CHECK(of_file(list + 3, 1, "game.NZ") == 0);
   CHECK(of_file(list, 0, "Golf.gba") == -1);

   /* every system full: still the first with the extension */
   n = sys_builtin(list);
   for (i = n; i < SYS_MAX; i++)
   {
      CHECK(parse("name=Test\nconsole=TST\ntitle_id=0x0004000000ABC000\nextensions=.a,.b,.c,.d,.e,.f,.g,.tst\n") == 0);
      list[i] = s;
   }
   CHECK(of_file(list, SYS_MAX, "game.tst") == SYS_BUILTIN);

   clean();
   rmdir(dir);
}

static int ini(const char *text, const char *sec, const char *key, char *out, int outn)
{
   return sys_ini_value(text, strlen(text), sec, key, out, outn);
}

static void test_ini(void)
{
   char v[64];
   /* RPG Maker XP's Game.ini */
   static const char game_ini[] =
      "[Game]\r\nLibrary=RGSS104E.dll\r\nScripts=Data\\Scripts.rxdata\r\nTitle=Aedemphia\r\nRTP1=Standard\r\nRTP2=\r\nRTP3=\r\n";
   CHECK(ini(game_ini, "Game", "Title", v, sizeof(v)) == 0 && !strcmp(v, "Aedemphia"));
   CHECK(ini(game_ini, "game", "TITLE", v, sizeof(v)) == 0 && !strcmp(v, "Aedemphia"));
   CHECK(ini(game_ini, "Game", "RTP2", v, sizeof(v)) == -1 && !v[0]);       /* empty */
   CHECK(ini(game_ini, "Game", "Name", v, sizeof(v)) == -1);
   CHECK(ini(game_ini, "Other", "Title", v, sizeof(v)) == -1);
   CHECK(ini(game_ini, "Game", "Library", v, sizeof(v)) == 0 && !strcmp(v, "RGSS104E.dll"));
   /* spaces, a BOM, quotes, comments, LF only, no newline at the end */
   CHECK(ini("\xEF\xBB\xBF; c\n# c\n[ Game ]\n  Title  =  \"  My Game \"  ", "Game", "Title", v, sizeof(v)) == 0 && !strcmp(v, "My Game"));
   CHECK(ini("[Game]\nTitle=\"\"\n", "Game", "Title", v, sizeof(v)) == -1);
   CHECK(ini("[Game]\nTitle=\"\n", "Game", "Title", v, sizeof(v)) == 0 && !strcmp(v, "\""));
   /* the right section only; the first value wins */
   CHECK(ini("Title=Top\n[A]\nTitle=A\n[Game]\nTitle=G\nTitle=G2\n[B]\nTitle=B\n", "Game", "Title", v, sizeof(v)) == 0 && !strcmp(v, "G"));
   CHECK(ini("[Game]\nX=1\n[Other]\nTitle=O\n", "Game", "Title", v, sizeof(v)) == -1);
   CHECK(ini("[Game\nTitle=T\n", "Game", "Title", v, sizeof(v)) == -1);      /* no ']': not a section */
   CHECK(ini("[Game]\nTitle\nTitles=x\nTitle=T\n", "Game", "Title", v, sizeof(v)) == 0 && !strcmp(v, "T"));
   CHECK(ini("[Game]\nTitle=a=b\n", "Game", "Title", v, sizeof(v)) == 0 && !strcmp(v, "a=b"));
   CHECK(ini("[Game]\nTi tle=x\n", "Game", "Ti tle", v, sizeof(v)) == 0);
   CHECK(ini("", "Game", "Title", v, sizeof(v)) == -1);
   CHECK(ini("[Game]\nTitle=T\n", "Game", "Title", v, 0) == -1);
   /* UTF-8 kept, cut at a whole letter */
   CHECK(ini("[Game]\nTitle=Pok\xC3\xA9mon \xE3\x81\x82\n", "Game", "Title", v, sizeof(v)) == 0 && !strcmp(v, "Pok\xC3\xA9mon \xE3\x81\x82"));
   CHECK(ini("[Game]\nTitle=ab\xC3\xA9z\n", "Game", "Title", v, 4) == 0 && !strcmp(v, "ab"));
   CHECK(ini("[Game]\nTitle=ab\xC3\xA9z\n", "Game", "Title", v, 5) == 0 && !strcmp(v, "ab\xC3\xA9"));
   CHECK(ini("[Game]\nTitle=\xF0\x9F\x98\x80!\n", "Game", "Title", v, 4) == 0 && !strcmp(v, ""));
   CHECK(ini("[Game]\nTitle=abc\n", "Game", "Title", v, 1) == 0 && !strcmp(v, ""));
   /* not UTF-8 text: Shift-JIS, Windows-1252, control characters, overlong,
    * surrogates, past U+10FFFF, cut letters */
   CHECK(ini("[Game]\nTitle=\x83\x51\x81\x5B\x83\x80\n", "Game", "Title", v, sizeof(v)) == -1 && !v[0]);
   CHECK(ini("[Game]\nTitle=Pok\xE9mon\n", "Game", "Title", v, sizeof(v)) == -1);
   CHECK(ini("[Game]\nTitle=a\tb\n", "Game", "Title", v, sizeof(v)) == -1);  /* a tab inside it too */
   CHECK(ini("[Game]\nTitle=a\x01" "b\n", "Game", "Title", v, sizeof(v)) == -1);
   CHECK(ini("[Game]\nTitle=a\x7F" "b\n", "Game", "Title", v, sizeof(v)) == -1);
   CHECK(ini("[Game]\nTitle=\xC0\xAF\n", "Game", "Title", v, sizeof(v)) == -1);
   CHECK(ini("[Game]\nTitle=\xE0\x80\xAF\n", "Game", "Title", v, sizeof(v)) == -1);
   CHECK(ini("[Game]\nTitle=\xED\xA0\x80\n", "Game", "Title", v, sizeof(v)) == -1);
   CHECK(ini("[Game]\nTitle=\xF4\x90\x80\x80\n", "Game", "Title", v, sizeof(v)) == -1);
   CHECK(ini("[Game]\nTitle=a\xC3", "Game", "Title", v, sizeof(v)) == -1);
   CHECK(ini("[Game]\nTitle=\xE3\x81\n", "Game", "Title", v, sizeof(v)) == -1);
   /* a NUL in the text: up to it */
   CHECK(sys_ini_value("[Game]\nTitle=a\0b\n", 16, "Game", "Title", v, sizeof(v)) == -1);
   /* a long title: cut to SYS_TITLE_MAX */
   {
      char big[400], want[SYS_TITLE_MAX + 1];
      strcpy(big, "[Game]\nTitle=");
      memset(big + 13, 'x', 300);
      strcpy(big + 313, "\n");
      memset(want, 'x', SYS_TITLE_MAX);
      want[SYS_TITLE_MAX] = 0;
      CHECK(ini(big, "Game", "Title", v, SYS_TITLE_MAX + 1) == 0 && !strcmp(v, want));
   }
   /* random bytes: never past out, out always ends in 0 and is UTF-8 */
   {
      char text[300], out[16];
      int i, k;
      srand(7);
      for (i = 0; i < 100000; i++)
      {
         int n = rand() % 300;
         static const char *const bits[] = { "[Game]", "\n", "Title", "=", " ", "\"", "\xC3", "\xA9", "\xE3", "\x81", "\r", "[", "]" };
         int len = 0;
         while (len < n)
         {
            const char *b = rand() % 3 ? bits[rand() % 13] : NULL;
            if (b && len + (int)strlen(b) < n)
            {
               memcpy(text + len, b, strlen(b));
               len += strlen(b);
            }
            else
               text[len++] = (char)(rand() % 256);
         }
         memset(out, 'Q', sizeof(out));
         if (sys_ini_value(text, len, "Game", "Title", out, 12) == 0)
         {
            int ok = strlen(out) < 12;
            for (k = 0; out[k]; )
            {
               int l = (unsigned char)out[k] < 0x80 ? 1 : (unsigned char)out[k] < 0xE0 ? 2 : (unsigned char)out[k] < 0xF0 ? 3 : 4;
               if ((unsigned char)out[k] < 0x20)
                  ok = 0;
               k += l;
               if (k > 11)
                  ok = 0;
            }
            CHECK(ok);
         }
         CHECK(out[12] == 'Q' && memchr(out, 0, 12));
      }
   }
}

static void test_folder_games(void)
{
   static sys_desc list[SYS_MAX];
   char p[600], v[SYS_TITLE_MAX + 1];
   int n = sys_builtin(list), i;

   strcpy(dir, "/tmp/systemstest.XXXXXX");
   CHECK(mkdtemp(dir) != NULL);
   /* the built-in ones have no folder games: folders aren't looked in */
   CHECK(!sys_has_folder_games(list, n));
   snprintf(p, sizeof(p), "%s/aedemphia", dir);
   mkdir(p, 0777);
   put("aedemphia/Game.ini", "[Game]\r\nLibrary=RGSS104E.dll\r\nTitle=Aedemphia\r\n");
   put("aedemphia/Game.exe", "x");
   CHECK(sys_of_folder(list, n, p) == -1);

   CHECK(parse(rmxp_cfg) == 0);
   list[n++] = s;
   CHECK(sys_has_folder_games(list, n));
   CHECK(sys_of_folder(list, n, p) == 2);
   CHECK(sys_folder_title(&list[2], p, v, sizeof(v)) == 0 && !strcmp(v, "Aedemphia"));

   /* the other marker, any case; no Game.ini: no title (the folder's name is used) */
   snprintf(p, sizeof(p), "%s/mkxp", dir);
   mkdir(p, 0777);
   put("mkxp/MKXP.JSON", "{}");
   CHECK(sys_of_folder(list, n, p) == 2);
   CHECK(sys_folder_title(&list[2], p, v, sizeof(v)) == -1 && !v[0]);

   /* Game.ini without a title, with a Shift-JIS one, very long */
   snprintf(p, sizeof(p), "%s/notitle", dir);
   mkdir(p, 0777);
   put("notitle/game.ini", "[Game]\nLibrary=RGSS104E.dll\n");
   CHECK(sys_of_folder(list, n, p) == 2);
   CHECK(sys_folder_title(&list[2], p, v, sizeof(v)) == -1);
   snprintf(p, sizeof(p), "%s/sjis", dir);
   mkdir(p, 0777);
   put("sjis/Game.ini", "[Game]\nTitle=\x83\x51\x81\x5B\x83\x80\n");
   CHECK(sys_folder_title(&list[2], p, v, sizeof(v)) == -1);
   snprintf(p, sizeof(p), "%s/big", dir);
   mkdir(p, 0777);
   {
      /* the title after the first SYS_INI_MAX bytes isn't seen */
      char *big = malloc(SYS_INI_MAX + 100);
      memset(big, '\n', SYS_INI_MAX + 99);
      memcpy(big, "[Game]\n", 7);
      memcpy(big + SYS_INI_MAX - 5, "Title=Late\n", 11);
      big[SYS_INI_MAX + 99] = 0;
      put("big/Game.ini", big);
      CHECK(sys_folder_title(&list[2], p, v, sizeof(v)) == -1);
      memcpy(big + SYS_INI_MAX - 5, "\n\n\n\n\n\n\n\n\n\n\n", 11);
      memcpy(big + SYS_INI_MAX - 12, "Title=OK\n", 9);
      put("big/Game.ini", big);
      CHECK(sys_folder_title(&list[2], p, v, sizeof(v)) == 0 && !strcmp(v, "OK"));
      free(big);
   }

   /* not games: no marker; a folder named like a marker; no folder; a
    * marker past the first SYS_FOLDER_SCAN entries isn't seen */
   snprintf(p, sizeof(p), "%s/plain", dir);
   mkdir(p, 0777);
   put("plain/readme.txt", "x");
   CHECK(sys_of_folder(list, n, p) == -1);
   snprintf(p, sizeof(p), "%s/dirmarker", dir);
   mkdir(p, 0777);
   snprintf(p, sizeof(p), "%s/dirmarker/Game.ini", dir);
   mkdir(p, 0777);
   snprintf(p, sizeof(p), "%s/dirmarker", dir);
   CHECK(sys_of_folder(list, n, p) == -1);
   snprintf(p, sizeof(p), "%s/missing", dir);
   CHECK(sys_of_folder(list, n, p) == -1);
   CHECK(sys_folder_title(&list[2], p, v, sizeof(v)) == -1);
   snprintf(p, sizeof(p), "%s/crowd", dir);
   mkdir(p, 0777);
   for (i = 0; i < SYS_FOLDER_SCAN + 50; i++)
   {
      char f[64];
      snprintf(f, sizeof(f), "crowd/f%03d.dat", i);
      put(f, "x");
   }
   CHECK(sys_of_folder(list, n, p) == -1 || sys_of_folder(list, n, p) == 2);  /* readdir's order isn't fixed */

   /* a system without title_from: no title */
   CHECK(parse("name=T\nconsole=T\ntitle_id=0x0004000000ABC000\nfolder_game=Game.ini\n") == 0);
   snprintf(p, sizeof(p), "%s/aedemphia", dir);
   CHECK(sys_folder_title(&s, p, v, sizeof(v)) == -1);

   /* two systems with markers there: the first in the list wins */
   CHECK(parse("name=Exe\nconsole=EXE\ntitle_id=0x0004000000ABC000\nfolder_game=game.exe\n") == 0);
   list[n++] = s;
   CHECK(sys_of_folder(list, n, p) == 2);
   list[2] = list[3];
   CHECK(parse(rmxp_cfg) == 0);
   list[3] = s;
   CHECK(sys_of_folder(list, n, p) == 2);
   CHECK(sys_of_folder(list, 2, p) == -1);

   clean();
   rmdir(dir);
}

int main(void)
{
   test_random();
   test_good();
   test_limits();
   test_missing();
   test_numbers();
   test_duplicate_keys();
   test_folder();
   test_detect();
   test_ini();
   test_folder_games();
   printf("%d checks, %d failed\n", checks, fails);
   return fails ? 1 : 0;
}
