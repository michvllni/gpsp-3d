/* The systems Icon Maker makes icons for: see systems.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include "systems.h"

#define FILES_MAX 64              /* .cfg files looked at in the folder */

int sys_builtin(sys_desc *list)
{
   sys_desc *g = &list[SYS_GBA], *s = &list[SYS_SNES];
   memset(list, 0, SYS_BUILTIN * sizeof(sys_desc));
   strcpy(g->name, "gpSP 3D");
   strcpy(g->console, "GBA");
   g->tid = 0x000400000BAC8100ULL;
   strcpy(g->ext[0], ".gba");
   strcpy(g->ext[1], ".zip");
   g->next = 2;
   g->style = SYS_STYLE_GBA;
   g->tag_rgb = 0x23D5C8;
   strcpy(g->boxart, "Nintendo_-_Game_Boy_Advance");

   strcpy(s->name, "Snes9x 3D");
   strcpy(s->console, "SNES");
   s->tid = 0x00040000038A7100ULL;
   strcpy(s->launch, "sdmc:/3ds/snes9x_3ds/launch.txt");
   strcpy(s->ext[0], ".sfc");
   strcpy(s->ext[1], ".smc");
   strcpy(s->ext[2], ".fig");
   strcpy(s->ext[3], ".swc");
   strcpy(s->ext[4], ".zip");
   s->next = 5;
   s->style = SYS_STYLE_SNES;
   s->tag_rgb = 0xB59CFF;
   strcpy(s->boxart, "Nintendo_-_Super_Nintendo_Entertainment_System");
   return SYS_BUILTIN;
}

/* ---- checking values ---- */

static int is_space(char c)
{
   return c == ' ' || c == '\t';
}

/* s (n bytes) is plain printable ASCII */
static int printable(const char *s, size_t n)
{
   size_t i;
   for (i = 0; i < n; i++)
      if ((unsigned char)s[i] < 32 || (unsigned char)s[i] > 126)
         return 0;
   return 1;
}

/* a file name in a game's folder: no folders, no ".." */
static int plain_file_name(const char *s)
{
   return *s && strcmp(s, ".") && strcmp(s, "..") && !strpbrk(s, "/\\:");
}

/* "0x" and 1 to 16 hex digits */
static int parse_hex64(const char *s, uint64_t *v)
{
   int n = 0;
   if (s[0] != '0' || (s[1] != 'x' && s[1] != 'X'))
      return 0;
   *v = 0;
   for (s += 2; *s; s++, n++)
   {
      int d = *s >= '0' && *s <= '9' ? *s - '0' : *s >= 'a' && *s <= 'f' ? *s - 'a' + 10
              : *s >= 'A' && *s <= 'F' ? *s - 'A' + 10 : -1;
      if (d < 0 || n == 16)
         return 0;
      *v = *v << 4 | (uint64_t)d;
   }
   return n > 0;
}

/* the next comma-separated item of *p into out (spaces around it left
 * out); 0 at the end, -1 if it's longer than max */
static int next_item(const char **p, char *out, int max)
{
   const char *s = *p, *e;
   size_t n;
   if (!*s)
      return 0;
   e = strchr(s, ',');
   if (!e)
      e = s + strlen(s);
   *p = *e ? e + 1 : e;
   while (s < e && is_space(*s)) s++;
   while (e > s && is_space(e[-1])) e--;
   n = (size_t)(e - s);
   if (n > (size_t)max)
      return -1;
   memcpy(out, s, n);
   out[n] = 0;
   return 1;
}

/* ---- one file ---- */

enum { K_NAME, K_CONSOLE, K_TITLE_ID, K_LAUNCH, K_EXTENSIONS, K_FOLDER_GAME, K_TITLE_FROM,
       K_ICON, K_TAG_COLOR, K_BOXART, K_COUNT };
static const char *const keys[K_COUNT] = {
   "name", "console", "title_id", "launch", "extensions", "folder_game", "title_from",
   "icon", "tag_color", "boxart"
};

/* one key's value into s; 0, or -1 with why */
static int set_key(sys_desc *s, int k, const char *v, char *why, int whylen)
{
   size_t n = strlen(v);
   char item[SYS_ICON_MAX + 1];
   const char *p = v;
   int r;
   switch (k)
   {
   case K_NAME:
      if (!n || n > SYS_NAME_MAX)
         return snprintf(why, whylen, "name must be 1 to %d letters", SYS_NAME_MAX), -1;
      strcpy(s->name, v);
      return 0;
   case K_CONSOLE:
      if (!n || n > SYS_CONSOLE_MAX)
         return snprintf(why, whylen, "console must be 1 to %d letters", SYS_CONSOLE_MAX), -1;
      strcpy(s->console, v);
      return 0;
   case K_TITLE_ID:
   {
      uint64_t t;
      uint32_t uid;
      if (!parse_hex64(v, &t))
         return snprintf(why, whylen, "title_id isn't 0x and hex digits"), -1;
      uid = (uint32_t)t >> 8;
      if ((t >> 32) != 0x00040000 || !uid)
         return snprintf(why, whylen, "title_id must be 0x00040000xxxxxxxx"), -1;
      if (uid >= 0xC0000 && uid < 0xF0000)
         return snprintf(why, whylen, "title_id is in the icons' own range"), -1;
      s->tid = t;
      return 0;
   }
   case K_LAUNCH:
      if (n > SYS_LAUNCH_MAX)
         return snprintf(why, whylen, "launch is over %d letters", SYS_LAUNCH_MAX), -1;
      if (n && strncmp(v, "sdmc:/", 6))
         return snprintf(why, whylen, "launch must start with sdmc:/"), -1;
      strcpy(s->launch, v);
      return 0;
   case K_EXTENSIONS:
      while ((r = next_item(&p, item, SYS_EXT_MAX)))
      {
         size_t i;
         if (r < 0 || item[0] != '.' || !item[1])
            return snprintf(why, whylen, "extensions: like .gba, up to %d letters", SYS_EXT_MAX - 1), -1;
         for (i = 1; item[i]; i++)
            if (!((item[i] >= 'a' && item[i] <= 'z') || (item[i] >= 'A' && item[i] <= 'Z') ||
                  (item[i] >= '0' && item[i] <= '9')))
               return snprintf(why, whylen, "extensions: like .gba, letters and digits"), -1;
         if (s->next == SYS_EXT_N)
            return snprintf(why, whylen, "more than %d extensions", SYS_EXT_N), -1;
         strcpy(s->ext[s->next++], item);
      }
      return 0;
   case K_FOLDER_GAME:
      while ((r = next_item(&p, item, SYS_WORD_MAX)))
      {
         if (r < 0 || !plain_file_name(item))
            return snprintf(why, whylen, "folder_game: file names, up to %d letters", SYS_WORD_MAX), -1;
         if (s->nfolder_game == SYS_MARKER_N)
            return snprintf(why, whylen, "more than %d folder_game files", SYS_MARKER_N), -1;
         strcpy(s->folder_game[s->nfolder_game++], item);
      }
      return 0;
   case K_TITLE_FROM:
   {
      /* file:section:key, each 1 to SYS_WORD_MAX */
      const char *c1 = strchr(v, ':'), *c2 = c1 ? strchr(c1 + 1, ':') : NULL;
      size_t a, b, c;
      if (!n)
         return 0;
      if (!c2 || strchr(c2 + 1, ':'))
         return snprintf(why, whylen, "title_from must be file:section:key"), -1;
      a = (size_t)(c1 - v);
      b = (size_t)(c2 - c1 - 1);
      c = strlen(c2 + 1);
      if (!a || !b || !c || a > SYS_WORD_MAX || b > SYS_WORD_MAX || c > SYS_WORD_MAX)
         return snprintf(why, whylen, "title_from: each part 1 to %d letters", SYS_WORD_MAX), -1;
      memcpy(s->title_file, v, a);
      s->title_file[a] = 0;
      memcpy(s->title_section, c1 + 1, b);
      s->title_section[b] = 0;
      strcpy(s->title_key, c2 + 1);
      if (!plain_file_name(s->title_file) || strchr(s->title_section, ']') || strchr(s->title_key, '='))
         return snprintf(why, whylen, "title_from must be file:section:key"), -1;
      return 0;
   }
   case K_ICON:
      if (!strcasecmp(v, "gba"))
         s->style = SYS_STYLE_GBA;
      else if (!strcasecmp(v, "snes"))
         s->style = SYS_STYLE_SNES;
      else if (!strcasecmp(v, "generic") || !n)
         s->style = SYS_STYLE_GENERIC;
      else
      {
         if (n > SYS_ICON_MAX || strncmp(v, "sdmc:/", 6) || n < 10 || strcasecmp(v + n - 4, ".png"))
            return snprintf(why, whylen, "icon: gba, snes, generic or sdmc:/....png"), -1;
         s->style = SYS_STYLE_PNG;
         strcpy(s->icon_png, v);
      }
      return 0;
   case K_TAG_COLOR:
   {
      uint64_t c;
      char hex[20];
      if (n != 7 || v[0] != '#')
         return snprintf(why, whylen, "tag_color must be #RRGGBB"), -1;
      snprintf(hex, sizeof(hex), "0x%s", v + 1);
      if (!parse_hex64(hex, &c))
         return snprintf(why, whylen, "tag_color must be #RRGGBB"), -1;
      s->tag_rgb = (uint32_t)c;
      return 0;
   }
   case K_BOXART:
   {
      size_t i;
      if (!n || !strcasecmp(v, "none"))
      {
         s->boxart[0] = 0;
         return 0;
      }
      if (n > SYS_BOXART_MAX || v[0] == '.')
         return snprintf(why, whylen, "boxart: a folder name, or none"), -1;
      for (i = 0; i < n; i++)
         if (!((v[i] >= 'a' && v[i] <= 'z') || (v[i] >= 'A' && v[i] <= 'Z') ||
               (v[i] >= '0' && v[i] <= '9') || v[i] == '_' || v[i] == '-' || v[i] == '.'))
            return snprintf(why, whylen, "boxart: a folder name, or none"), -1;
      strcpy(s->boxart, v);
      return 0;
   }
   }
   return 0;
}

int sys_parse(const char *text, size_t len, sys_desc *s, char *why, int whylen)
{
   char line[SYS_LINE_MAX + 1];
   int seen[K_COUNT] = { 0 }, ln = 0, k;
   size_t pos = 0;
   memset(s, 0, sizeof(*s));
   s->style = SYS_STYLE_GENERIC;
   s->tag_rgb = 0xC5CBD6;              /* the list's light grey */
   if (len > SYS_FILE_MAX)
      return snprintf(why, whylen, "bigger than %d bytes", SYS_FILE_MAX), -1;
   if (memchr(text, 0, len))
      return snprintf(why, whylen, "not a text file"), -1;
   if (len >= 3 && !memcmp(text, "\xEF\xBB\xBF", 3))   /* Notepad's mark */
      pos = 3;
   while (pos < len)
   {
      const char *nl = memchr(text + pos, '\n', len - pos);
      size_t n = nl ? (size_t)(nl - (text + pos)) : len - pos;
      char *a, *e, *eq, *v;
      ln++;
      if (n > SYS_LINE_MAX)
         return snprintf(why, whylen, "line %d: over %d letters", ln, SYS_LINE_MAX), -1;
      memcpy(line, text + pos, n);
      line[n] = 0;
      pos += n + 1;
      a = line;
      e = line + n;
      while (e > a && (is_space(e[-1]) || e[-1] == '\r')) e--;
      *e = 0;
      while (is_space(*a)) a++;
      if (!*a || *a == '#' || *a == ';')
         continue;
      if (!(eq = strchr(a, '=')))
         return snprintf(why, whylen, "line %d: no key=value", ln), -1;
      v = eq + 1;
      while (eq > a && is_space(eq[-1])) eq--;
      *eq = 0;
      while (is_space(*v)) v++;
      if (!printable(a, strlen(a)) || !printable(v, strlen(v)))
         return snprintf(why, whylen, "line %d: only plain ASCII letters", ln), -1;
      for (k = 0; k < K_COUNT && strcmp(a, keys[k]); k++)
         ;
      if (k == K_COUNT)                 /* not known: left out */
         continue;
      if (seen[k]++)
         return snprintf(why, whylen, "line %d: %s is there twice", ln, keys[k]), -1;
      if (set_key(s, k, v, why, whylen))
         return -1;
   }
   for (k = K_NAME; k <= K_TITLE_ID; k++)
      if (!seen[k])
         return snprintf(why, whylen, "%s= is missing", keys[k]), -1;
   if (!s->next && !s->nfolder_game)
      return snprintf(why, whylen, "needs extensions= or folder_game="), -1;
   return 0;
}

/* ---- the folder ---- */

static int cmp_str(const void *a, const void *b)
{
   return strcasecmp(*(char *const*)a, *(char *const*)b);
}

static void note(sys_note *notes, int *nnotes, const char *file, const char *why)
{
   if (*nnotes < SYS_NOTES)
   {
      /* a file name too long is cut */
      snprintf(notes[*nnotes].file, sizeof(notes[0].file), "%.*s", (int)sizeof(notes[0].file) - 1, file);
      snprintf(notes[*nnotes].why, sizeof(notes[0].why), "%s", why);
   }
   (*nnotes)++;
}

/* the file's text (0 at the end), or NULL with why */
static char *read_cfg(const char *path, size_t *len, char *why, int whylen)
{
   FILE *f = fopen(path, "rb");
   char *buf;
   if (!f)
      return snprintf(why, whylen, "can't be read"), NULL;
   /* one byte more than allowed tells a file that's too big */
   buf = malloc(SYS_FILE_MAX + 2);
   if (!buf)
   {
      fclose(f);
      return snprintf(why, whylen, "not enough memory"), NULL;
   }
   *len = fread(buf, 1, SYS_FILE_MAX + 1, f);
   fclose(f);
   if (*len > SYS_FILE_MAX)
   {
      free(buf);
      return snprintf(why, whylen, "bigger than %d bytes", SYS_FILE_MAX), NULL;
   }
   buf[*len] = 0;
   return buf;
}

int sys_load(const char *dir, sys_desc *list, int n, sys_note *notes, int *nnotes)
{
   char *names[FILES_MAX], why[80], path[SYS_WORD_MAX + 300];
   int nnames = 0, more = 0, nbuiltin = n, i, k;
   int replaced[SYS_MAX] = { 0 };    /* a built-in one a file replaced */
   DIR *d = opendir(dir);
   struct dirent *e;
   *nnotes = 0;
   if (!d)
      return n;
   while ((e = readdir(d)))
   {
      size_t len = strlen(e->d_name);
      if (e->d_type == DT_DIR || e->d_name[0] == '.' || len < 5 || strcasecmp(e->d_name + len - 4, ".cfg"))
         continue;
      if (len > SYS_WORD_MAX)
      {
         note(notes, nnotes, e->d_name, "its file name is too long");
         continue;
      }
      if (nnames == FILES_MAX)
      {
         more++;
         continue;
      }
      if ((names[nnames] = strdup(e->d_name)))
         nnames++;
   }
   closedir(d);
   qsort(names, nnames, sizeof(char*), cmp_str);
   for (i = 0; i < nnames; i++)
   {
      sys_desc s;
      size_t len;
      char *text;
      int at = -1;
      snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
      if (!(text = read_cfg(path, &len, why, sizeof(why))) || sys_parse(text, len, &s, why, sizeof(why)))
      {
         free(text);
         note(notes, nnotes, names[i], why);
         continue;
      }
      free(text);
      snprintf(s.file, sizeof(s.file), "%s", names[i]);
      for (k = 0; k < n && at < 0; k++)
         if (!strcmp(list[k].name, s.name))
            at = k;
      if (at >= 0 && (at >= nbuiltin || replaced[at]))
      {
         snprintf(why, sizeof(why), "same name as %.60s", list[at].file);
         note(notes, nnotes, names[i], why);
         continue;
      }
      if (at < 0 && n == SYS_MAX)
      {
         snprintf(why, sizeof(why), "more than %d systems", SYS_MAX);
         note(notes, nnotes, names[i], why);
         continue;
      }
      if (at >= 0)                      /* a built-in one, replaced in its place */
         replaced[at] = 1;
      else
         at = n++;
      list[at] = s;
   }
   for (i = 0; i < nnames; i++)
      free(names[i]);
   if (more)
   {
      snprintf(why, sizeof(why), "more than %d files: %d left out", FILES_MAX, more);
      note(notes, nnotes, "systems folder", why);
   }
   return n;
}

/* ------------------------------------------------------------------ */
/* which system a game is for                                          */
/* ------------------------------------------------------------------ */

static int has_ext(const sys_desc *s, const char *ext)
{
   int i;
   for (i = 0; i < s->next; i++)
      if (!strcasecmp(s->ext[i], ext))
         return 1;
   return 0;
}

int sys_of_ext(const sys_desc *list, int n, const char *ext)
{
   int i;
   if (!ext || ext[0] != '.')
      return -1;
   for (i = 0; i < n; i++)
      if (has_ext(&list[i], ext))
         return i;
   return -1;
}

int sys_of_zip(const sys_desc *list, int n, const char *first)
{
   const char *dot = first ? strrchr(first, '.') : NULL;
   int i;
   if (dot && !strchr(dot, '/') && strcasecmp(dot, ".zip"))
      for (i = 0; i < n; i++)
         if (has_ext(&list[i], dot) && has_ext(&list[i], ".zip"))
            return i;
   return sys_of_ext(list, n, ".zip");
}

int sys_of_file(const sys_desc *list, int n, const char *path)
{
   const char *dot = strrchr(path, '.');
   unsigned char hdr[30];
   char first[256];
   int got = 0;
   FILE *f;
   if (!dot || strchr(dot, '/'))
      return -1;
   if (strcasecmp(dot, ".zip"))
      return sys_of_ext(list, n, dot);
   if (sys_of_ext(list, n, ".zip") < 0)
      return -1;                         /* no system opens zips: don't read it */
   /* the first entry's name, from its local header */
   if ((f = fopen(path, "rb")))
   {
      if (fread(hdr, 1, 30, f) == 30 && hdr[0] == 'P' && hdr[1] == 'K' && hdr[2] == 3 && hdr[3] == 4)
      {
         int len = hdr[26] | hdr[27] << 8;
         if (len > 255)
            len = 255;
         if (fread(first, 1, len, f) == (size_t)len)
         {
            first[len] = 0;
            got = 1;
         }
      }
      fclose(f);
   }
   return sys_of_zip(list, n, got ? first : NULL);
}
