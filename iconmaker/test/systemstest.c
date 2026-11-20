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

int main(void)
{
   test_random();
   test_good();
   test_limits();
   test_missing();
   test_numbers();
   test_duplicate_keys();
   test_folder();
   printf("%d checks, %d failed\n", checks, fails);
   return fails ? 1 : 0;
}
