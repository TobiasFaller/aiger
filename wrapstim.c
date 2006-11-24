#include "aiger.h"

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static unsigned k;
static int foundk;

static aiger * model;
static const char *model_file_name;
static unsigned ** m2e;

static aiger * expansion;
static const char *expansion_file_name;
static char * assignment;

static const char *stimulus_file_name;
static int close_stimulus_file;
static FILE * stimulus_file;

static void
die (const char * fmt, ...)
{
  va_list ap;
  fputs ("*** [wrapstim] ", stderr);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  exit (1);
}

static void
link (void)
{
  const char * p, * start_of_name, * end_of_name, * q;
  aiger_symbol * esym, * msym;
  unsigned i, mpos, epos;
  char ch;

  m2e = malloc (sizeof (m2e[0]) * model->num_inputs);
  for (mpos = 0; mpos < model->num_inputs; mpos++)
    m2e[mpos] = calloc (k + 1, sizeof (m2e[mpos][0]));

  for (epos = 0; epos < expansion->num_inputs; epos++)
    {
      esym = expansion->inputs + epos;
      p = esym->name; 
      if (!p)
	die ("input %u does not have an expanded symbol in '%s'", 
	     epos, expansion_file_name);

      ch = *p++;
      if (!isdigit (ch))
	die ("symbol for input %u of '%s' does not have a time prefix",
	     epos, expansion_file_name);

      i = ch - '0';
      while (isdigit (ch = *p++))
	i = 10 * i + (ch - '0');

      if (i > k)
	die ("time %u prefix of symbol for input %u in '%s' exceeds bound %u", 
	     i, epos, expansion_file_name, k);

      if (ch != ' ')
SPACE_MISSING:
	die ("symbol of input %u of '%s' is missing a space seperator",
	     epos, expansion_file_name);

      start_of_name = p;
      while (*p++)
	;

      while (*--p != ' ')		/* we have ' ' as sentinel */
	;

      end_of_name = p;
      if (end_of_name < start_of_name)
	goto SPACE_MISSING;

      mpos = 0;
      while ((ch = *++p))
	{
	  if (!isdigit (ch))
	    die ("invalid position suffix in symbol for input %u of '%s'",
		 epos, expansion_file_name);

	  mpos = 10 * mpos + (ch - '0');
	}

      assert (i <= k);
      if (mpos >= model->num_inputs)
	die ("invalid model position %u in symbol of input %u of '%s'",  
	     mpos, epos, expansion_file_name);
	  
      if (m2e[mpos][k])
	die ("input %u of '%s' at time %u expanded twice in '%s'", 
	     mpos, model_file_name, i, expansion_file_name);

      msym = model->inputs + mpos;
      if (msym->name)
	{
	  for (p = start_of_name, q = msym->name; 
	       p < end_of_name && *q && *p == *q;
	      p++, q++)
	    ;
	  
	  if ((p == end_of_name) != !*q)
	    die ("symbol of input %u in '%s' "
		 "does not match its expansion at time %u in '%s'",
		 mpos, model_file_name, i, expansion_file_name);
	}

      m2e[mpos][i] = epos;
    }
}

static void
parse (void)
{
  unsigned pos;
  int ch;

  assignment = calloc (expansion->num_inputs, sizeof (assignment[0]));

  for (pos = 0; pos < expansion->num_inputs; pos++)
    {
      ch = getc (stimulus_file);

      if (ch == EOF || ch == '\n')
	die ("only got %u values out of %u in line 1 of '%s'",
	     pos, expansion->num_inputs, stimulus_file_name);

      if (ch != '0' && ch != '1' && ch != 'x')
	die ("expected '0', '1', or 'x' at character %u in line 1 of '%s'",
	     pos + 1, stimulus_file_name);

      assignment[pos] = ch;
    }

  ch = getc (stimulus_file);
  if (ch != '\n')
    die ("expected new line after %u values in line 1 of '%s'",
	 pos, stimulus_file_name);

  ch = getc (stimulus_file);
  if (ch != EOF)
    die ("trailing characters after line 1 of '%s'", stimulus_file_name);
}


#define USAGE \
  "usage: wrapstim [-h] <model> <expansion> <k> [<stimulus>]\n"

int
main (int argc, char ** argv)
{
  const char * err;
  int i;

  for (i = 1; i < argc; i++)
    {
      if (!strcmp (argv[i], "-h"))
	{
	  fprintf (stderr, USAGE);
	  exit (0);
	}
      else if (argv[i][0] == '-')
        die ("invalid command line option '%s'", argv[i]);
      else if (!model_file_name)
	model_file_name = argv[i];
      else if (!expansion_file_name)
	expansion_file_name = argv[i];
      else if (!foundk)
	{
	  const char * p = argv[i];
	  while (*p)
	    if (!isdigit (*p++))
	      die ("expected bound as third argument");

	  k = atoi (argv[i]);
	  foundk = 1;
	}
      else if (!stimulus_file)
	stimulus_file_name = argv[i];
      else
	die ("too many paramaters");
    }

  if (!model_file_name)
    die ("no model specified");

  if (!expansion_file_name)
    die ("no expansion specified");

  if (!foundk)
    die ("unspecified bound");

  model = aiger_init ();
  if ((err = aiger_open_and_read_from_file (model, model_file_name)))
    die ("%s", err);

  expansion = aiger_init ();
  if ((err = aiger_open_and_read_from_file (expansion, expansion_file_name)))
    die ("%s", err);

  link ();

  if (!stimulus_file_name)
    {
      if (!(stimulus_file = fopen (stimulus_file_name, "r")))
	die ("failed to read '%s'", stimulus_file_name);

      close_stimulus_file = 1;
    }
  else
    stimulus_file = stdin;

  parse ();

  if (close_stimulus_file)
    fclose (stimulus_file);

  aiger_reset (expansion);

  for (i = 0; i < model->num_inputs; i++)
    free (m2e[i]);
  free (m2e);

  aiger_reset (model);
   
  return 0;
}