/* readelf.c -- display contents of an ELF format file
   Copyright 1998, 1999, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

   Originally developed by Eric Youngdale <eric@andante.jic.com>
   Modifications by Nick Clifton <nickc@redhat.com>

   This file is part of GNU Binutils.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */


/*
 * This file was directly derived from readelf.c from binutils 2.14
 * and stripped down to perform only the decoding of the dwarf2 line
 * information by: Jason Wessel <jason.wessel@windriver.com>
 * Copyright (c) 2012 Wind River Systems, Inc.  All Rights Reserved.
 */
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <getopt.h>

#if __GNUC__ >= 2
/* Define BFD64 here, even if our default architecture is 32 bit ELF
   as this will allow us to read in and parse 64bit and 32bit ELF files.
   Only do this if we believe that the compiler can support a 64 bit
   data type.  For now we only rely on GCC being able to do this.  */
#define BFD64
#endif

#define PARAMS(ARGS)            ARGS
#define VPARAMS(ARGS)           ARGS
#define VA_START(VA_LIST, VAR)  va_start(VA_LIST, VAR)
#define VA_OPEN(AP, VAR)        { va_list AP; va_start(AP, VAR); { struct Qdmy
#define VA_CLOSE(AP)            } va_end(AP); }
#define VA_FIXEDARG(AP, T, N)   struct Qdmy
#define PTR             char *
#ifndef ATTRIBUTE_UNUSED
#define ATTRIBUTE_UNUSED __attribute__ ((__unused__))
#endif /* ATTRIBUTE_UNUSED */

/* 64 bit bfd types */

#ifdef __i386__
typedef unsigned long long bfd_vma;
typedef unsigned long long bfd_size_type;
typedef long long bfd_signed_vma;
#define LSTR "0x%llx"
#else
typedef unsigned long bfd_vma;
typedef unsigned long bfd_size_type;
typedef long bfd_signed_vma;
#define LSTR "0x%lx"
#endif
typedef bfd_signed_vma file_ptr;

#include <elf.h>

/* From elf/external.h */
typedef struct {
  unsigned char	e_ident[16];		/* ELF "magic number" */
  unsigned char	e_type[2];		/* Identifies object file type */
  unsigned char	e_machine[2];		/* Specifies required architecture */
  unsigned char	e_version[4];		/* Identifies object file version */
  unsigned char	e_entry[4];		/* Entry point virtual address */
  unsigned char	e_phoff[4];		/* Program header table file offset */
  unsigned char	e_shoff[4];		/* Section header table file offset */
  unsigned char	e_flags[4];		/* Processor-specific flags */
  unsigned char	e_ehsize[2];		/* ELF header size in bytes */
  unsigned char	e_phentsize[2];		/* Program header table entry size */
  unsigned char	e_phnum[2];		/* Program header table entry count */
  unsigned char	e_shentsize[2];		/* Section header table entry size */
  unsigned char	e_shnum[2];		/* Section header table entry count */
  unsigned char	e_shstrndx[2];		/* Section header string table index */
} Elf32_External_Ehdr;

typedef struct {
  unsigned char	e_ident[16];		/* ELF "magic number" */
  unsigned char	e_type[2];		/* Identifies object file type */
  unsigned char	e_machine[2];		/* Specifies required architecture */
  unsigned char	e_version[4];		/* Identifies object file version */
  unsigned char	e_entry[8];		/* Entry point virtual address */
  unsigned char	e_phoff[8];		/* Program header table file offset */
  unsigned char	e_shoff[8];		/* Section header table file offset */
  unsigned char	e_flags[4];		/* Processor-specific flags */
  unsigned char	e_ehsize[2];		/* ELF header size in bytes */
  unsigned char	e_phentsize[2];		/* Program header table entry size */
  unsigned char	e_phnum[2];		/* Program header table entry count */
  unsigned char	e_shentsize[2];		/* Section header table entry size */
  unsigned char	e_shnum[2];		/* Section header table entry count */
  unsigned char	e_shstrndx[2];		/* Section header string table index */
} Elf64_External_Ehdr;

/* Section header */
typedef struct {
  unsigned char	sh_name[4];		/* Section name, index in string tbl */
  unsigned char	sh_type[4];		/* Type of section */
  unsigned char	sh_flags[4];		/* Miscellaneous section attributes */
  unsigned char	sh_addr[4];		/* Section virtual addr at execution */
  unsigned char	sh_offset[4];		/* Section file offset */
  unsigned char	sh_size[4];		/* Size of section in bytes */
  unsigned char	sh_link[4];		/* Index of another section */
  unsigned char	sh_info[4];		/* Additional section information */
  unsigned char	sh_addralign[4];	/* Section alignment */
  unsigned char	sh_entsize[4];		/* Entry size if section holds table */
} Elf32_External_Shdr;

typedef struct {
  unsigned char	sh_name[4];		/* Section name, index in string tbl */
  unsigned char	sh_type[4];		/* Type of section */
  unsigned char	sh_flags[8];		/* Miscellaneous section attributes */
  unsigned char	sh_addr[8];		/* Section virtual addr at execution */
  unsigned char	sh_offset[8];		/* Section file offset */
  unsigned char	sh_size[8];		/* Size of section in bytes */
  unsigned char	sh_link[4];		/* Index of another section */
  unsigned char	sh_info[4];		/* Additional section information */
  unsigned char	sh_addralign[8];	/* Section alignment */
  unsigned char	sh_entsize[8];		/* Entry size if section holds table */
} Elf64_External_Shdr;


/* End from elf/external.h */

/* From elf/internal.h */
typedef struct elf_internal_ehdr {
  unsigned char		e_ident[EI_NIDENT]; /* ELF "magic number" */
  bfd_vma		e_entry;	/* Entry point virtual address */
  bfd_size_type		e_phoff;	/* Program header table file offset */
  bfd_size_type		e_shoff;	/* Section header table file offset */
  unsigned long		e_version;	/* Identifies object file version */
  unsigned long		e_flags;	/* Processor-specific flags */
  unsigned short	e_type;		/* Identifies object file type */
  unsigned short	e_machine;	/* Specifies required architecture */
  unsigned int		e_ehsize;	/* ELF header size in bytes */
  unsigned int		e_phentsize;	/* Program header table entry size */
  unsigned int		e_phnum;	/* Program header table entry count */
  unsigned int		e_shentsize;	/* Section header table entry size */
  unsigned int		e_shnum;	/* Section header table entry count */
  unsigned int		e_shstrndx;	/* Section header string table index */
} Elf_Internal_Ehdr;

/* Section header */
typedef struct elf_internal_shdr {
  unsigned int	sh_name;		/* Section name, index in string tbl */
  unsigned int	sh_type;		/* Type of section */
  bfd_vma	sh_flags;		/* Miscellaneous section attributes */
  bfd_vma	sh_addr;		/* Section virtual addr at execution */
  bfd_size_type	sh_size;		/* Size of section in bytes */
  bfd_size_type	sh_entsize;		/* Entry size if section holds table */
  unsigned long	sh_link;		/* Index of another section */
  unsigned long	sh_info;		/* Additional section information */
  file_ptr	sh_offset;		/* Section file offset */
  unsigned int	sh_addralign;		/* Section alignment */

  /* The internal rep also has some cached info associated with it. */
#if 0
  asection *	bfd_section;		/* Associated BFD section.  */
  unsigned char *contents;		/* Section contents.  */
#endif
} Elf_Internal_Shdr;

/* End from elf/internal.h */

/* From dwarf2.h */
/* Line number opcodes.  */
enum dwarf_line_number_ops
  {
    DW_LNS_extended_op = 0,
    DW_LNS_copy = 1,
    DW_LNS_advance_pc = 2,
    DW_LNS_advance_line = 3,
    DW_LNS_set_file = 4,
    DW_LNS_set_column = 5,
    DW_LNS_negate_stmt = 6,
    DW_LNS_set_basic_block = 7,
    DW_LNS_const_add_pc = 8,
    DW_LNS_fixed_advance_pc = 9,
    /* DWARF 3.  */
    DW_LNS_set_prologue_end = 10,
    DW_LNS_set_epilogue_begin = 11,
    DW_LNS_set_isa = 12
  };

/* Line number extended opcodes.  */
enum dwarf_line_number_x_ops
  {
    DW_LNE_end_sequence = 1,
    DW_LNE_set_address = 2,
    DW_LNE_define_file = 3
  };

typedef struct
{
  unsigned long  li_length;
  unsigned short li_version;
  unsigned int   li_prologue_length;
  unsigned char  li_min_insn_length;
  unsigned char  li_default_is_stmt;
  int            li_line_base;
  unsigned char  li_line_range;
  unsigned char  li_opcode_base;
}
DWARF2_Internal_LineInfo;
/* End from dwarf2.h */

char *program_name = "readelf";
unsigned long dynamic_addr;
bfd_size_type dynamic_size;
char *dynamic_strings;
char *string_table;
unsigned long string_table_length;
unsigned long num_dynamic_syms;
char program_interpreter[64];
long dynamic_info[DT_JMPREL + 1];
long version_info[16];
long loadaddr = 0;
Elf_Internal_Ehdr elf_header;
Elf_Internal_Shdr *section_headers;
Elf_Internal_Shdr *symtab_shndx_hdr;
int show_name;
int do_dump;
int do_quiet;
int do_wide;
int do_debug_lines;
int is_32bit_elf;

/* A dynamic array of flags indicating which sections require dumping.  */
char *dump_sects = NULL;
unsigned int num_dump_sects = 0;

#define HEX_DUMP	(1 << 0)
#define DISASS_DUMP	(1 << 1)
#define DEBUG_DUMP	(1 << 2)

/* How to rpint a vma value.  */
typedef enum print_mode
{
  HEX,
  DEC,
  DEC_5,
  UNSIGNED,
  PREFIX_HEX,
  FULL_HEX,
  LONG_HEX
}
print_mode;

/* Forward declarations for dumb compilers.  */
static bfd_vma (*byte_get)
  PARAMS ((unsigned char *, int));
static bfd_vma byte_get_little_endian
  PARAMS ((unsigned char *, int));
static bfd_vma byte_get_big_endian
  PARAMS ((unsigned char *, int));
static void (*byte_put)
  PARAMS ((unsigned char *, bfd_vma, int));
static void byte_put_little_endian
  PARAMS ((unsigned char *, bfd_vma, int));
static void byte_put_big_endian
  PARAMS ((unsigned char *, bfd_vma, int));
static void usage
  PARAMS ((void));
static void parse_args
  PARAMS ((int, char **));
static int process_file_header
  PARAMS ((void));
static int process_section_headers
  PARAMS ((FILE *));
static int process_section_contents
  PARAMS ((FILE *));
static int process_file
  PARAMS ((char *));
static int get_32bit_section_headers
  PARAMS ((FILE *, unsigned int));
static int get_64bit_section_headers
  PARAMS ((FILE *, unsigned int));
static int get_file_header
  PARAMS ((FILE *));
static int display_debug_section
  PARAMS ((Elf_Internal_Shdr *, FILE *));
static int prescan_debug_info
  PARAMS ((Elf_Internal_Shdr *, unsigned char *, FILE *));
static int display_debug_lines
  PARAMS ((Elf_Internal_Shdr *, unsigned char *, FILE *));
static unsigned long read_leb128
  PARAMS ((unsigned char *, int *, int));
static int process_extended_line_op
  PARAMS ((unsigned char *, int, int));
static void reset_state_machine
  PARAMS ((int));
static void request_dump
  PARAMS ((unsigned int, int));

#define UNKNOWN -1

#define SECTION_NAME(X)	((X) == NULL ? "<none>" : \
				 ((X)->sh_name >= string_table_length \
				  ? "<corrupt>" : string_table + (X)->sh_name))

/* Given st_shndx I, map to section_headers index.  */
#define SECTION_HEADER_INDEX(I)				\
  ((I) < SHN_LORESERVE					\
   ? (I)						\
   : ((I) <= SHN_HIRESERVE				\
      ? 0						\
      : (I) - (SHN_HIRESERVE + 1 - SHN_LORESERVE)))

/* Reverse of the above.  */
#define SECTION_HEADER_NUM(N)				\
  ((N) < SHN_LORESERVE					\
   ? (N)						\
   : (N) + (SHN_HIRESERVE + 1 - SHN_LORESERVE))

#define SECTION_HEADER(I) (section_headers + SECTION_HEADER_INDEX (I))

#define DT_VERSIONTAGIDX(tag)	(DT_VERNEEDNUM - (tag))	/* Reverse order!  */

#define BYTE_GET(field)	byte_get (field, sizeof (field))

/* If we can support a 64 bit data type then BFD64 should be defined
   and sizeof (bfd_vma) == 8.  In this case when translating from an
   external 8 byte field to an internal field, we can assume that the
   internal field is also 8 bytes wide and so we can extract all the data.
   If, however, BFD64 is not defined, then we must assume that the
   internal data structure only has 4 byte wide fields that are the
   equivalent of the 8 byte wide external counterparts, and so we must
   truncate the data.  */
#ifdef  BFD64
#define BYTE_GET8(field)	byte_get (field, -8)
#else
#define BYTE_GET8(field)	byte_get (field, 8)
#endif

#define NUM_ELEM(array) 	(sizeof (array) / sizeof ((array)[0]))

#define GET_ELF_SYMBOLS(file, section)			\
  (is_32bit_elf ? get_32bit_elf_symbols (file, section)	\
   : get_64bit_elf_symbols (file, section))


static void
error VPARAMS ((const char *message, ...))
{
  VA_OPEN (args, message);
  VA_FIXEDARG (args, const char *, message);

  fprintf (stderr, "%s: Error: ", program_name);
  vfprintf (stderr, message, args);
  VA_CLOSE (args);
}

static void
warn VPARAMS ((const char *message, ...))
{
  VA_OPEN (args, message);
  VA_FIXEDARG (args, const char *, message);

  fprintf (stderr, "%s: Warning: ", program_name);
  vfprintf (stderr, message, args);
  VA_CLOSE (args);
}

static PTR get_data PARAMS ((PTR, FILE *, long, size_t, const char *));

static PTR
get_data (var, file, offset, size, reason)
     PTR var;
     FILE *file;
     long offset;
     size_t size;
     const char *reason;
{
  PTR mvar;

  if (size == 0)
    return NULL;

  if (fseek (file, offset, SEEK_SET))
    {
      error ("Unable to seek to %x for %s\n", offset, reason);
      return NULL;
    }

  mvar = var;
  if (mvar == NULL)
    {
      mvar = (PTR) malloc (size);

      if (mvar == NULL)
	{
	  error ("Out of memory allocating %d bytes for %s\n",
		 size, reason);
	  return NULL;
	}
    }

  if (fread (mvar, size, 1, file) != 1)
    {
      error ("Unable to read in %d bytes of %s\n", size, reason);
      if (mvar != var)
	free (mvar);
      return NULL;
    }

  return mvar;
}

static bfd_vma
byte_get_little_endian (field, size)
     unsigned char *field;
     int size;
{
  switch (size)
    {
    case 1:
      return *field;

    case 2:
      return  ((unsigned int) (field[0]))
	|    (((unsigned int) (field[1])) << 8);

#ifndef BFD64
    case 8:
      /* We want to extract data from an 8 byte wide field and
	 place it into a 4 byte wide field.  Since this is a little
	 endian source we can just use the 4 byte extraction code.  */
      /* Fall through.  */
#endif
    case 4:
      return  ((unsigned long) (field[0]))
	|    (((unsigned long) (field[1])) << 8)
	|    (((unsigned long) (field[2])) << 16)
	|    (((unsigned long) (field[3])) << 24);

#ifdef BFD64
    case 8:
    case -8:
      /* This is a special case, generated by the BYTE_GET8 macro.
	 It means that we are loading an 8 byte value from a field
	 in an external structure into an 8 byte value in a field
	 in an internal strcuture.  */
      return  ((bfd_vma) (field[0]))
	|    (((bfd_vma) (field[1])) << 8)
	|    (((bfd_vma) (field[2])) << 16)
	|    (((bfd_vma) (field[3])) << 24)
	|    (((bfd_vma) (field[4])) << 32)
	|    (((bfd_vma) (field[5])) << 40)
	|    (((bfd_vma) (field[6])) << 48)
	|    (((bfd_vma) (field[7])) << 56);
#endif
    default:
      error ("Unhandled data length: %d\n", size);
      abort ();
    }
}

static void
byte_put_little_endian (field, value, size)
     unsigned char * field;
     bfd_vma	     value;
     int             size;
{
  switch (size)
    {
    case 8:
      field[7] = (((value >> 24) >> 24) >> 8) & 0xff;
      field[6] = ((value >> 24) >> 24) & 0xff;
      field[5] = ((value >> 24) >> 16) & 0xff;
      field[4] = ((value >> 24) >> 8) & 0xff;
      /* Fall through.  */
    case 4:
      field[3] = (value >> 24) & 0xff;
      field[2] = (value >> 16) & 0xff;
      /* Fall through.  */
    case 2:
      field[1] = (value >> 8) & 0xff;
      /* Fall through.  */
    case 1:
      field[0] = value & 0xff;
      break;

    default:
      error ("Unhandled data length: %d\n", size);
      abort ();
    }
}

static bfd_vma
byte_get_big_endian (field, size)
     unsigned char *field;
     int size;
{
  switch (size)
    {
    case 1:
      return *field;

    case 2:
      return ((unsigned int) (field[1])) | (((int) (field[0])) << 8);

    case 4:
      return ((unsigned long) (field[3]))
	|   (((unsigned long) (field[2])) << 8)
	|   (((unsigned long) (field[1])) << 16)
	|   (((unsigned long) (field[0])) << 24);

#ifndef BFD64
    case 8:
      /* Although we are extracing data from an 8 byte wide field, we
	 are returning only 4 bytes of data.  */
      return ((unsigned long) (field[7]))
	|   (((unsigned long) (field[6])) << 8)
	|   (((unsigned long) (field[5])) << 16)
	|   (((unsigned long) (field[4])) << 24);
#else
    case 8:
    case -8:
      /* This is a special case, generated by the BYTE_GET8 macro.
	 It means that we are loading an 8 byte value from a field
	 in an external structure into an 8 byte value in a field
	 in an internal strcuture.  */
      return ((bfd_vma) (field[7]))
	|   (((bfd_vma) (field[6])) << 8)
	|   (((bfd_vma) (field[5])) << 16)
	|   (((bfd_vma) (field[4])) << 24)
	|   (((bfd_vma) (field[3])) << 32)
	|   (((bfd_vma) (field[2])) << 40)
	|   (((bfd_vma) (field[1])) << 48)
	|   (((bfd_vma) (field[0])) << 56);
#endif

    default:
      error ("Unhandled data length: %d\n", size);
      abort ();
    }
}

static void
byte_put_big_endian (field, value, size)
     unsigned char * field;
     bfd_vma	     value;
     int             size;
{
  switch (size)
    {
    case 8:
      field[7] = value & 0xff;
      field[6] = (value >> 8) & 0xff;
      field[5] = (value >> 16) & 0xff;
      field[4] = (value >> 24) & 0xff;
      value >>= 16;
      value >>= 16;
      /* Fall through.  */
    case 4:
      field[3] = value & 0xff;
      field[2] = (value >> 8) & 0xff;
      value >>= 16;
      /* Fall through.  */
    case 2:
      field[1] = value & 0xff;
      value >>= 8;
      /* Fall through.  */
    case 1:
      field[0] = value & 0xff;
      break;

    default:
      error ("Unhandled data length: %d\n", size);
      abort ();
    }
}

#define OPTION_DEBUG_DUMP	512

struct option options[] =
{
  {"debug-dump",       optional_argument, 0, OPTION_DEBUG_DUMP},
  {"version",	       no_argument, 0, 'v'},
  {"wide",	       no_argument, 0, 'W'},
  {"help",	       no_argument, 0, 'H'},
  {0,		       no_argument, 0, 0}
};

static void
usage ()
{
  fprintf (stdout, "Usage: readelf <option(s)> elf-file(s)\n");
  fprintf (stdout, " Display information about the contents of ELF format files\n");
  fprintf (stdout, " Options are:\n\
  --debug-dump=line      Display the contents of DWARF2 debug sections\n");
  fprintf (stdout, "\
  -q                     Only emit line number data\n\
  -W --wide              Allow output width to exceed 80 characters\n\
  -H --help              Display this information\n");

  exit (0);
}

static void
request_dump (section, type)
     unsigned int section;
     int type;
{
  if (section >= num_dump_sects)
    {
      char *new_dump_sects;

      new_dump_sects = (char *) calloc (section + 1, 1);

      if (new_dump_sects == NULL)
	error ("Out of memory allocating dump request table.");
      else
	{
	  /* Copy current flag settings.  */
	  memcpy (new_dump_sects, dump_sects, num_dump_sects);

	  free (dump_sects);

	  dump_sects = new_dump_sects;
	  num_dump_sects = section + 1;
	}
    }

  if (dump_sects)
    dump_sects[section] |= type;

  return;
}

static void
parse_args (argc, argv)
     int argc;
     char **argv;
{
  int c;

  if (argc < 2)
    usage ();

  while ((c = getopt_long
	  (argc, argv, "qWH", options, NULL)) != EOF)
    {

      switch (c)
	{
	case 0:
	  /* Long options.  */
	  break;
	case 'H':
	  usage ();
	  break;

	case 'q':
	  do_quiet = 1;
	  break;

	case OPTION_DEBUG_DUMP:
	  do_dump++;
	    {
	      static const char *debug_dump_opt[]
		= { "line", NULL };
	      unsigned int index;
	      const char *p;

	      p = optarg;
	      while (*p)
		{
		  for (index = 0; debug_dump_opt[index]; index++)
		    {
		      size_t len = strlen (debug_dump_opt[index]);

		      if (strncmp (p, debug_dump_opt[index], len) == 0
			  && (p[len] == ',' || p[len] == '\0'))
			{
			  switch (p[0])
			    {
			    case 'l':
			      if (p[1] == 'i')
				do_debug_lines = 1;
			      break;

			    }

			  p += len;
			  break;
			}
		    }

		  if (debug_dump_opt[index] == NULL)
		    {
		      warn ("Unrecognized debug option '%s'\n", p);
		      p = strchr (p, ',');
		      if (p == NULL)
			break;
		    }

		  if (*p == ',')
		    p++;
		}
	    }
	  break;
	case 'W':
	  do_wide++;
	  break;
	default:
	  /* Drop through.  */
	case '?':
	  usage ();
	}
    }

  if (!do_dump)
    usage ();
  else if (argc < 3)
    {
      warn ("Nothing to do.\n");
      usage();
    }
}

/* Decode the data held in 'elf_header'.  */

static int
process_file_header ()
{
  if (   elf_header.e_ident[EI_MAG0] != ELFMAG0
      || elf_header.e_ident[EI_MAG1] != ELFMAG1
      || elf_header.e_ident[EI_MAG2] != ELFMAG2
      || elf_header.e_ident[EI_MAG3] != ELFMAG3)
    {
      error
	("Not an ELF file - it has the wrong magic bytes at the start\n");
      return 0;
    }

  if (section_headers != NULL)
    {
      if (elf_header.e_shnum == 0)
	elf_header.e_shnum = section_headers[0].sh_size;
      if (elf_header.e_shstrndx == SHN_XINDEX)
	elf_header.e_shstrndx = section_headers[0].sh_link;
      free (section_headers);
      section_headers = NULL;
    }

  return 1;
}

static int
get_32bit_section_headers (file, num)
     FILE *file;
     unsigned int num;
{
  Elf32_External_Shdr *shdrs;
  Elf_Internal_Shdr *internal;
  unsigned int i;

  shdrs = ((Elf32_External_Shdr *)
	   get_data (NULL, file, elf_header.e_shoff,
		     elf_header.e_shentsize * num,
		     "section headers"));
  if (!shdrs)
    return 0;

  section_headers = ((Elf_Internal_Shdr *)
		     malloc (num * sizeof (Elf_Internal_Shdr)));

  if (section_headers == NULL)
    {
      error ("Out of memory\n");
      return 0;
    }

  for (i = 0, internal = section_headers;
       i < num;
       i++, internal++)
    {
      internal->sh_name      = BYTE_GET (shdrs[i].sh_name);
      internal->sh_type      = BYTE_GET (shdrs[i].sh_type);
      internal->sh_flags     = BYTE_GET (shdrs[i].sh_flags);
      internal->sh_addr      = BYTE_GET (shdrs[i].sh_addr);
      internal->sh_offset    = BYTE_GET (shdrs[i].sh_offset);
      internal->sh_size      = BYTE_GET (shdrs[i].sh_size);
      internal->sh_link      = BYTE_GET (shdrs[i].sh_link);
      internal->sh_info      = BYTE_GET (shdrs[i].sh_info);
      internal->sh_addralign = BYTE_GET (shdrs[i].sh_addralign);
      internal->sh_entsize   = BYTE_GET (shdrs[i].sh_entsize);
    }

  free (shdrs);

  return 1;
}

static int
get_64bit_section_headers (file, num)
     FILE *file;
     unsigned int num;
{
  Elf64_External_Shdr *shdrs;
  Elf_Internal_Shdr *internal;
  unsigned int i;

  shdrs = ((Elf64_External_Shdr *)
	   get_data (NULL, file, elf_header.e_shoff,
		     elf_header.e_shentsize * num,
		     "section headers"));
  if (!shdrs)
    return 0;

  section_headers = ((Elf_Internal_Shdr *)
		     malloc (num * sizeof (Elf_Internal_Shdr)));

  if (section_headers == NULL)
    {
      error ("Out of memory\n");
      return 0;
    }

  for (i = 0, internal = section_headers;
       i < num;
       i++, internal++)
    {
      internal->sh_name      = BYTE_GET (shdrs[i].sh_name);
      internal->sh_type      = BYTE_GET (shdrs[i].sh_type);
      internal->sh_flags     = BYTE_GET8 (shdrs[i].sh_flags);
      internal->sh_addr      = BYTE_GET8 (shdrs[i].sh_addr);
      internal->sh_size      = BYTE_GET8 (shdrs[i].sh_size);
      internal->sh_entsize   = BYTE_GET8 (shdrs[i].sh_entsize);
      internal->sh_link      = BYTE_GET (shdrs[i].sh_link);
      internal->sh_info      = BYTE_GET (shdrs[i].sh_info);
      internal->sh_offset    = BYTE_GET (shdrs[i].sh_offset);
      internal->sh_addralign = BYTE_GET (shdrs[i].sh_addralign);
    }

  free (shdrs);

  return 1;
}

static int
process_section_headers (file)
     FILE *file;
{
  Elf_Internal_Shdr *section;
  unsigned int i;

  section_headers = NULL;

  if (elf_header.e_shnum == 0)
    {
      return 1;
    }

  if (is_32bit_elf)
    {
      if (! get_32bit_section_headers (file, elf_header.e_shnum))
	return 0;
    }
  else if (! get_64bit_section_headers (file, elf_header.e_shnum))
    return 0;

  /* Read in the string table, so that we have names to display.  */
  section = SECTION_HEADER (elf_header.e_shstrndx);

  if (section->sh_size != 0)
    {
      string_table = (char *) get_data (NULL, file, section->sh_offset,
					section->sh_size, "string table");

      string_table_length = section->sh_size;
    }

  /* Scan the sections for the dynamic symbol table
     and dynamic string table and debug sections.  */
  dynamic_strings = NULL;
  symtab_shndx_hdr = NULL;

  for (i = 0, section = section_headers;
       i < elf_header.e_shnum;
       i++, section++)
    {
      char *name = SECTION_NAME (section);

      if (section->sh_type == SHT_DYNSYM)
	{
		;
	}
      else if (section->sh_type == SHT_STRTAB
	       && strcmp (name, ".dynstr") == 0)
	{
	  if (dynamic_strings != NULL)
	    {
	      error ("File contains multiple dynamic string tables\n");
	      continue;
	    }

	  dynamic_strings = (char *) get_data (NULL, file, section->sh_offset,
					       section->sh_size,
					       "dynamic strings");
	}
      else if (section->sh_type == SHT_SYMTAB_SHNDX)
	{
	  if (symtab_shndx_hdr != NULL)
	    {
	      error ("File contains multiple symtab shndx tables\n");
	      continue;
	    }
	  symtab_shndx_hdr = section;
	}
      else if (do_debug_lines && strncmp (name, ".debug_", 7) == 0)
	{
	  name += 7;

	  if (do_debug_lines && (strcmp (name, "line") == 0))
	    request_dump (i, DEBUG_DUMP);
	}
    }

    return 1;
}

static unsigned long int
read_leb128 (data, length_return, sign)
     unsigned char *data;
     int *length_return;
     int sign;
{
  unsigned long int result = 0;
  unsigned int num_read = 0;
  int shift = 0;
  unsigned char byte;

  do
    {
      byte = *data++;
      num_read++;

      result |= (byte & 0x7f) << shift;

      shift += 7;

    }
  while (byte & 0x80);

  if (length_return != NULL)
    *length_return = num_read;

  if (sign && (shift < 32) && (byte & 0x40))
    result |= -1 << shift;

  return result;
}

typedef struct State_Machine_Registers
{
  bfd_vma address;
  unsigned int file;
  unsigned int line;
  unsigned int column;
  int is_stmt;
  int basic_block;
  int end_sequence;
/* This variable hold the number of the last entry seen
   in the File Table.  */
  unsigned int last_file_entry;
} SMR;

static SMR state_machine_regs;

static void
reset_state_machine (is_stmt)
     int is_stmt;
{
  state_machine_regs.address = 0;
  state_machine_regs.file = 1;
  state_machine_regs.line = 1;
  state_machine_regs.column = 0;
  state_machine_regs.is_stmt = is_stmt;
  state_machine_regs.basic_block = 0;
  state_machine_regs.end_sequence = 0;
  state_machine_regs.last_file_entry = 0;
}

/* Handled an extend line op.  Returns true if this is the end
   of sequence.  */
static int
process_extended_line_op (data, is_stmt, pointer_size)
     unsigned char *data;
     int is_stmt;
     int pointer_size;
{
  unsigned char op_code;
  int bytes_read;
  unsigned int len;
  unsigned char *name;
  bfd_vma adr;
  unsigned long val;

  len = read_leb128 (data, & bytes_read, 0);
  data += bytes_read;

  if (len == 0)
    {
      warn ("badly formed extended line op encountered!\n");
      return bytes_read;
    }

  len += bytes_read;
  op_code = *data++;

  if (!do_quiet)
    printf ("  Extended opcode %d: ", op_code);

  switch (op_code)
    {
    case DW_LNE_end_sequence:
	  if (!do_quiet)
		printf ("End of Sequence\n\n");
      reset_state_machine (is_stmt);
      break;

    case DW_LNE_set_address:
      adr = byte_get (data, pointer_size);
	  if (!do_quiet)
		printf ("set Address to " LSTR "\n", adr);
      state_machine_regs.address = adr;
      break;

    case DW_LNE_define_file:
	  if (!do_quiet)
	    {
		  printf ("  define new File Table entry\n");
		  printf ("  Entry\tDir\tTime\tSize\tName\n");
		}

	  ++state_machine_regs.last_file_entry;
	  if (!do_quiet)
		printf ("   %d\t", state_machine_regs.last_file_entry);
      name = data;
      data += strlen ((char *) data) + 1;
	  val = read_leb128 (data, & bytes_read, 0);
	  if (!do_quiet)
		printf ("%lu\t", val);
      data += bytes_read;
	  val = read_leb128 (data, & bytes_read, 0);
	  if (!do_quiet)
		printf ("%lu\t", val);
      data += bytes_read;
	  val = read_leb128 (data, & bytes_read, 0);
	  if (!do_quiet)
	    {
		  printf ("%lu\t", val);
		  printf ("%s\n\n", name);
		}
      break;

    default:
	  if (!do_quiet)
		printf ("UNKNOWN: length %d\n", len - bytes_read);
      break;
    }

  return len;
}

/* Size of pointers in the .debug_line section.  This information is not
   really present in that section.  It's obtained before dumping the debug
   sections by doing some pre-scan of the .debug_info section.  */
static int debug_line_pointer_size = 4;

static int
display_debug_lines (section, start, file)
     Elf_Internal_Shdr *section;
     unsigned char * start;
     FILE *file ATTRIBUTE_UNUSED;
{
  unsigned char *hdrptr;
  DWARF2_Internal_LineInfo info;
  unsigned char *standard_opcodes;
  unsigned char *data = start;
  unsigned char *end = start + section->sh_size;
  unsigned char *end_of_sequence;
  unsigned int max_ftable = 0;
  unsigned char **ftable = NULL;

  int i;
  int offset_size;
  int initial_length_size;

  if (!do_quiet)
    printf ("\nDump of debug contents of section %s:\n\n",
	  SECTION_NAME (section));

  while (data < end)
    {
      hdrptr = data;

      /* Check the length of the block.  */
      info.li_length = byte_get (hdrptr, 4);
      hdrptr += 4;

      if (info.li_length == 0xffffffff)
	{
	  /* This section is 64-bit DWARF 3.  */
	  info.li_length = byte_get (hdrptr, 8);
	  hdrptr += 8;
	  offset_size = 8;
	  initial_length_size = 12;
	}
      else
	{
	  offset_size = 4;
	  initial_length_size = 4;
	}

      if (info.li_length + initial_length_size > section->sh_size)
	{
	  warn
	    ("The line info appears to be corrupt - the section is too small\n");
	  return 0;
	}

      /* Check its version number.  */
      info.li_version = byte_get (hdrptr, 2);
      hdrptr += 2;
      if (info.li_version != 2 && info.li_version != 3)
	{
	  warn ("Only DWARF version 2 and 3 line info is currently supported.\n");
	  return 0;
	}

      info.li_prologue_length = byte_get (hdrptr, offset_size);
      hdrptr += offset_size;
      info.li_min_insn_length = byte_get (hdrptr, 1);
      hdrptr++;
      info.li_default_is_stmt = byte_get (hdrptr, 1);
      hdrptr++;
      info.li_line_base = byte_get (hdrptr, 1);
      hdrptr++;
      info.li_line_range = byte_get (hdrptr, 1);
      hdrptr++;
      info.li_opcode_base = byte_get (hdrptr, 1);
      hdrptr++;

      /* Sign extend the line base field.  */
      info.li_line_base <<= 24;
      info.li_line_base >>= 24;

      if (!do_quiet)
        {
	  printf ("  Length:                      %ld\n", info.li_length);
	  printf ("  DWARF Version:               %d\n", info.li_version);
	  printf ("  Prologue Length:             %d\n", info.li_prologue_length);
	  printf ("  Minimum Instruction Length:  %d\n", info.li_min_insn_length);
	  printf ("  Initial value of 'is_stmt':  %d\n", info.li_default_is_stmt);
	  printf ("  Line Base:                   %d\n", info.li_line_base);
	  printf ("  Line Range:                  %d\n", info.li_line_range);
	  printf ("  Opcode Base:                 %d\n", info.li_opcode_base);
	}

      end_of_sequence = data + info.li_length + initial_length_size;

      reset_state_machine (info.li_default_is_stmt);

      /* Display the contents of the Opcodes table.  */
      standard_opcodes = hdrptr;

      if (!do_quiet) {
	printf ("\n Opcodes:\n");

        for (i = 1; i < info.li_opcode_base; i++)
	  printf ("  Opcode %d has %d args\n", i, standard_opcodes[i - 1]);
      }

      /* Display the contents of the Directory table.  */
      data = standard_opcodes + info.li_opcode_base - 1;

      if (*data == 0)
        {
	  if (!do_quiet)
	    printf ("\n The Directory Table is empty.\n");
        }
      else
	{
	  if (!do_quiet)
	    printf ("\n The Directory Table:\n");

	  while (*data != 0)
	    {
	      if (!do_quiet)
	        printf ("  %s\n", data);

	      data += strlen ((char *) data) + 1;
	    }
	}

      /* Skip the NUL at the end of the table.  */
      data++;

      if (ftable)
        {
	  while (max_ftable)
	    {
	      max_ftable--;
	      free(ftable[max_ftable]);
	    }
	  free(ftable);
	  ftable = NULL;
	}
      max_ftable = 0;
      /* Display the contents of the File Name table.  */
      if (*data == 0)
        {
	  if (!do_quiet)
	    printf ("\n The File Name Table is empty.\n");
	}
      else
	{
	  unsigned char *data_reset = data;
	  if (!do_quiet)
	    {
	      printf ("\n The File Name Table:\n");
	      printf ("  Entry\tDir\tTime\tSize\tName\n");
	    }

	  while (*data != 0)
	    {
	      unsigned char *name;
	      int bytes_read;
	      unsigned long ret;

	      max_ftable++;
	      if (!do_quiet)
	        printf ("  %d\t", ++state_machine_regs.last_file_entry);
	      name = data;

	      data += strlen ((char *) data) + 1;

	      ret =  read_leb128 (data, & bytes_read, 0);
	      if (!do_quiet)
	        printf ("%lu\t", ret);
	      data += bytes_read;
	      ret =  read_leb128 (data, & bytes_read, 0);
	      if (!do_quiet)
	        printf ("%lu\t", ret);
	      data += bytes_read;
	      ret =  read_leb128 (data, & bytes_read, 0);
	      if (!do_quiet)
	        printf ("%lu\t", ret);
	      data += bytes_read;
	      if (!do_quiet)
	        printf ("%s\n", name);
	    }
	  /* Allocate file table array */
	  ftable = malloc(sizeof(char *) * max_ftable);
	  data = data_reset;
	  max_ftable = 0;
	  while (*data != 0)
	    {
	      int bytes_read;

	      ftable[max_ftable] = (unsigned char *)strdup((char *)data);
	      if (!ftable[max_ftable])
		abort();
	      max_ftable++;

	      data += strlen ((char *) data) + 1;

	      read_leb128 (data, & bytes_read, 0);
	      data += bytes_read;
	      read_leb128 (data, & bytes_read, 0);
	      data += bytes_read;
	      read_leb128 (data, & bytes_read, 0);
	      data += bytes_read;
	    }
	}

      /* Skip the NUL at the end of the table.  */
      data++;

      /* Now display the statements.  */
      if (!do_quiet)
        printf ("\n Line Number Statements:\n");


      while (data < end_of_sequence)
	{
	  unsigned char op_code;
	  int adv;
	  int bytes_read;

	  op_code = *data++;

	  if (op_code >= info.li_opcode_base)
	    {
	      op_code -= info.li_opcode_base;
	      adv      = (op_code / info.li_line_range) * info.li_min_insn_length;
	      state_machine_regs.address += adv;
	      if (!do_quiet)
	        printf ("  Special opcode %d: advance Address by %d to " LSTR,
		      op_code, adv, state_machine_regs.address);
	      adv = (op_code % info.li_line_range) + info.li_line_base;
	      state_machine_regs.line += adv;
	      if (!do_quiet)
	        printf (" and Line by %d to %d\n",
		      adv, state_machine_regs.line);
	      if (do_quiet)
		printf("%s:%u " LSTR "\n", ftable[state_machine_regs.file - 1],
state_machine_regs.line, state_machine_regs.address);
	    }
	  else switch (op_code)
	    {
	    case DW_LNS_extended_op:
	      data += process_extended_line_op (data, info.li_default_is_stmt,
						debug_line_pointer_size);
	      break;

	    case DW_LNS_copy:
	      if (!do_quiet)
	        printf ("  Copy\n");
	      break;

	    case DW_LNS_advance_pc:
	      adv = info.li_min_insn_length * read_leb128 (data, & bytes_read, 0);
	      data += bytes_read;
	      state_machine_regs.address += adv;
	      if (!do_quiet)
	        printf ("  Advance PC by %d to " LSTR "\n", adv,
		      state_machine_regs.address);
	      break;

	    case DW_LNS_advance_line:
	      adv = read_leb128 (data, & bytes_read, 1);
	      data += bytes_read;
	      state_machine_regs.line += adv;
	      if (!do_quiet)
	        printf ("  Advance Line by %d to %d\n", adv,
		      state_machine_regs.line);
	      break;

	    case DW_LNS_set_file:
	      adv = read_leb128 (data, & bytes_read, 0);
	      data += bytes_read;
	      if (!do_quiet)
	        printf ("  Set File Name to entry %d in the File Name Table\n",
		      adv);
	      state_machine_regs.file = adv;
	      break;

	    case DW_LNS_set_column:
	      adv = read_leb128 (data, & bytes_read, 0);
	      data += bytes_read;
	      if (!do_quiet)
	        printf ("  Set column to %d\n", adv);
	      state_machine_regs.column = adv;
	      break;

	    case DW_LNS_negate_stmt:
	      adv = state_machine_regs.is_stmt;
	      adv = ! adv;
	      if (!do_quiet)
	        printf ("  Set is_stmt to %d\n", adv);
	      state_machine_regs.is_stmt = adv;
	      break;

	    case DW_LNS_set_basic_block:
	      if (!do_quiet)
	        printf ("  Set basic block\n");
	      state_machine_regs.basic_block = 1;
	      break;

	    case DW_LNS_const_add_pc:
	      adv = (((255 - info.li_opcode_base) / info.li_line_range)
		     * info.li_min_insn_length);
	      state_machine_regs.address += adv;
	      if (!do_quiet)
	        printf ("  Advance PC by constant %d to " LSTR "\n", adv,
		      state_machine_regs.address);
	      break;

	    case DW_LNS_fixed_advance_pc:
	      adv = byte_get (data, 2);
	      data += 2;
	      state_machine_regs.address += adv;
	      if (!do_quiet)
	        printf ("  Advance PC by fixed size amount %d to " LSTR "\n",
		      adv, state_machine_regs.address);
	      break;

	    case DW_LNS_set_prologue_end:
	      if (!do_quiet)
	        printf ("  Set prologue_end to true\n");
	      break;

	    case DW_LNS_set_epilogue_begin:
	      if (!do_quiet)
	        printf ("  Set epilogue_begin to true\n");
	      break;

	    case DW_LNS_set_isa:
	      adv = read_leb128 (data, & bytes_read, 0);
	      data += bytes_read;
	      if (!do_quiet)
	        printf ("  Set ISA to %d\n", adv);
	      break;

	    default:
	      if (!do_quiet)
	        printf ("  Unknown opcode %d with operands: ", op_code);
	      {
		int i;
		for (i = standard_opcodes[op_code - 1]; i > 0 ; --i)
		  {
		    if (!do_quiet)
		      printf ("0x%lx%s", read_leb128 (data, &bytes_read, 0),
			    i == 1 ? "" : ", ");
		    data += bytes_read;
		  }
		if (!do_quiet)
		  putchar ('\n');
	      }
	      break;
	    }
	}
      if (!do_quiet)
        putchar ('\n');
    }

  return 1;
}

/* Pre-scan the .debug_info section to record the size of address.
   When dumping the .debug_line, we use that size information, assuming
   that all compilation units have the same address size.  */
static int
prescan_debug_info (section, start, file)
     Elf_Internal_Shdr *section ATTRIBUTE_UNUSED;
     unsigned char *start;
     FILE *file ATTRIBUTE_UNUSED;
{
  unsigned long length;

  /* Read the first 4 bytes.  For a 32-bit DWARF section, this will
     be the length.  For a 64-bit DWARF section, it'll be the escape
     code 0xffffffff followed by an 8 byte length.  For the purposes
     of this prescan, we don't care about the actual length, but the
     presence of the escape bytes does affect the location of the byte
     which describes the address size.  */
  length = byte_get (start, 4);

  if (length == 0xffffffff)
    {
      /* For 64-bit DWARF, the 1-byte address_size field is 22 bytes
         from the start of the section.  This is computed as follows:

	    unit_length:         12 bytes
	    version:              2 bytes
	    debug_abbrev_offset:  8 bytes
	    -----------------------------
	    Total:               22 bytes  */

      debug_line_pointer_size = byte_get (start + 22, 1);
    }
  else
    {
      /* For 32-bit DWARF, the 1-byte address_size field is 10 bytes from
         the start of the section:
	    unit_length:          4 bytes
	    version:              2 bytes
	    debug_abbrev_offset:  4 bytes
	    -----------------------------
	    Total:               10 bytes  */

      debug_line_pointer_size = byte_get (start + 10, 1);
    }
  return 0;
}

  /* A structure containing the name of a debug section and a pointer
     to a function that can decode it.  The third field is a prescan
     function to be run over the section before displaying any of the
     sections.  */
struct
{
  const char *const name;
  int (*display) PARAMS ((Elf_Internal_Shdr *, unsigned char *, FILE *));
  int (*prescan) PARAMS ((Elf_Internal_Shdr *, unsigned char *, FILE *));
}
debug_displays[] =
{
  { ".debug_line",		display_debug_lines, NULL },
};

static int
display_debug_section (section, file)
     Elf_Internal_Shdr *section;
     FILE *file;
{
  char *name = SECTION_NAME (section);
  bfd_size_type length;
  unsigned char *start;
  int i;

  length = section->sh_size;
  if (length == 0)
    {
      printf ("\nSection '%s' has no debugging data.\n", name);
      return 0;
    }

  start = (unsigned char *) get_data (NULL, file, section->sh_offset, length,
				      "debug section data");
  if (!start)
    return 0;

  /* See if we know how to display the contents of this section.  */
  if (strncmp (name, ".gnu.linkonce.wi.", 17) == 0)
    name = ".debug_info";

  for (i = NUM_ELEM (debug_displays); i--;)
    if (strcmp (debug_displays[i].name, name) == 0)
      {
	debug_displays[i].display (section, start, file);
	break;
      }

  if (i == -1)
    printf ("Unrecognized debug section: %s\n", name);

  free (start);

  return 1;
}

static int
process_section_contents (file)
     FILE *file;
{
  Elf_Internal_Shdr *section;
  unsigned int i;

  if (! do_dump)
    return 1;

  /* Pre-scan the debug sections to find some debug information not
     present in some of them.  For the .debug_line, we must find out the
     size of address (specified in .debug_info and .debug_aranges).  */
  for (i = 0, section = section_headers;
       i < elf_header.e_shnum && i < num_dump_sects;
       i++, section++)
    {
      char *name = SECTION_NAME (section);

      if (section->sh_size == 0)
	continue;

      if (strcmp (".debug_info", name) == 0)
	{
	  bfd_size_type length;
	  unsigned char *start;

	  length = section->sh_size;
	  start = ((unsigned char *)
		   get_data (NULL, file, section->sh_offset, length,
			     "debug section data"));
	  if (!start)
	    return 0;

	  prescan_debug_info (section, start, file);
	  free (start);
	}
    }

  for (i = 0, section = section_headers;
       i < elf_header.e_shnum && i < num_dump_sects;
       i++, section++)
    {
#ifdef SUPPORT_DISASSEMBLY
      if (dump_sects[i] & DISASS_DUMP)
	disassemble_section (section, file);
#endif

      if (dump_sects[i] & DEBUG_DUMP)
	display_debug_section (section, file);
    }

  if (i < num_dump_sects)
    warn ("Some sections were not dumped because they do not exist!\n");

  return 1;
}

static int
get_file_header (file)
     FILE *file;
{
  /* Read in the identity array.  */
  if (fread (elf_header.e_ident, EI_NIDENT, 1, file) != 1)
    return 0;

  /* Determine how to read the rest of the header.  */
  switch (elf_header.e_ident[EI_DATA])
    {
    default: /* fall through */
    case ELFDATANONE: /* fall through */
    case ELFDATA2LSB:
      byte_get = byte_get_little_endian;
      byte_put = byte_put_little_endian;
      break;
    case ELFDATA2MSB:
      byte_get = byte_get_big_endian;
      byte_put = byte_put_big_endian;
      break;
    }

  /* For now we only support 32 bit and 64 bit ELF files.  */
  is_32bit_elf = (elf_header.e_ident[EI_CLASS] != ELFCLASS64);

  /* Read in the rest of the header.  */
  if (is_32bit_elf)
    {
      Elf32_External_Ehdr ehdr32;

      if (fread (ehdr32.e_type, sizeof (ehdr32) - EI_NIDENT, 1, file) != 1)
	return 0;

      elf_header.e_type      = BYTE_GET (ehdr32.e_type);
      elf_header.e_machine   = BYTE_GET (ehdr32.e_machine);
      elf_header.e_version   = BYTE_GET (ehdr32.e_version);
      elf_header.e_entry     = BYTE_GET (ehdr32.e_entry);
      elf_header.e_phoff     = BYTE_GET (ehdr32.e_phoff);
      elf_header.e_shoff     = BYTE_GET (ehdr32.e_shoff);
      elf_header.e_flags     = BYTE_GET (ehdr32.e_flags);
      elf_header.e_ehsize    = BYTE_GET (ehdr32.e_ehsize);
      elf_header.e_phentsize = BYTE_GET (ehdr32.e_phentsize);
      elf_header.e_phnum     = BYTE_GET (ehdr32.e_phnum);
      elf_header.e_shentsize = BYTE_GET (ehdr32.e_shentsize);
      elf_header.e_shnum     = BYTE_GET (ehdr32.e_shnum);
      elf_header.e_shstrndx  = BYTE_GET (ehdr32.e_shstrndx);
    }
  else
    {
      Elf64_External_Ehdr ehdr64;

      /* If we have been compiled with sizeof (bfd_vma) == 4, then
	 we will not be able to cope with the 64bit data found in
	 64 ELF files.  Detect this now and abort before we start
	 overwritting things.  */
      if (sizeof (bfd_vma) < 8)
	{
	  error ("This instance of readelf has been built without support for a\n\
64 bit data type and so it cannot read 64 bit ELF files.\n");
	  return 0;
	}

      if (fread (ehdr64.e_type, sizeof (ehdr64) - EI_NIDENT, 1, file) != 1)
	return 0;

      elf_header.e_type      = BYTE_GET (ehdr64.e_type);
      elf_header.e_machine   = BYTE_GET (ehdr64.e_machine);
      elf_header.e_version   = BYTE_GET (ehdr64.e_version);
      elf_header.e_entry     = BYTE_GET8 (ehdr64.e_entry);
      elf_header.e_phoff     = BYTE_GET8 (ehdr64.e_phoff);
      elf_header.e_shoff     = BYTE_GET8 (ehdr64.e_shoff);
      elf_header.e_flags     = BYTE_GET (ehdr64.e_flags);
      elf_header.e_ehsize    = BYTE_GET (ehdr64.e_ehsize);
      elf_header.e_phentsize = BYTE_GET (ehdr64.e_phentsize);
      elf_header.e_phnum     = BYTE_GET (ehdr64.e_phnum);
      elf_header.e_shentsize = BYTE_GET (ehdr64.e_shentsize);
      elf_header.e_shnum     = BYTE_GET (ehdr64.e_shnum);
      elf_header.e_shstrndx  = BYTE_GET (ehdr64.e_shstrndx);
    }

  if (elf_header.e_shoff)
    {
      /* There may be some extensions in the first section header.  Don't
	 bomb if we can't read it.  */
      if (is_32bit_elf)
	get_32bit_section_headers (file, 1);
      else
	get_64bit_section_headers (file, 1);
    }

  return 1;
}

static int
process_file (file_name)
     char *file_name;
{
  FILE *file;
  struct stat statbuf;
  unsigned int i;

  if (stat (file_name, & statbuf) < 0)
    {
      error ("Cannot stat input file %s.\n", file_name);
      return 1;
    }

  file = fopen (file_name, "rb");
  if (file == NULL)
    {
      error ("Input file %s not found.\n", file_name);
      return 1;
    }

  if (! get_file_header (file))
    {
      error ("%s: Failed to read file header\n", file_name);
      fclose (file);
      return 1;
    }

  /* Initialise per file variables.  */
  for (i = NUM_ELEM (version_info); i--;)
    version_info[i] = 0;

  for (i = NUM_ELEM (dynamic_info); i--;)
    dynamic_info[i] = 0;

  /* Process the file.  */
  if (show_name)
    printf ("\nFile: %s\n", file_name);

  if (! process_file_header ())
    {
      fclose (file);
      return 1;
    }

  if (! process_section_headers (file))
    {
      /* Without loaded section headers we
	 cannot process lots of things.  */
      do_dump = 0;

    }

  process_section_contents (file);

  fclose (file);

  if (section_headers)
    {
      free (section_headers);
      section_headers = NULL;
    }

  if (string_table)
    {
      free (string_table);
      string_table = NULL;
      string_table_length = 0;
    }

  if (dynamic_strings)
    {
      free (dynamic_strings);
      dynamic_strings = NULL;
    }

  return 0;
}

#ifdef SUPPORT_DISASSEMBLY
/* Needed by the i386 disassembler.  For extra credit, someone could
   fix this so that we insert symbolic addresses here, esp for GOT/PLT
   symbols.  */

void
print_address (unsigned int addr, FILE *outfile)
{
  fprintf (outfile,"0x%8.8x", addr);
}

/* Needed by the i386 disassembler.  */
void
db_task_printsym (unsigned int addr)
{
  print_address (addr, stderr);
}
#endif

int main PARAMS ((int, char **));

int
main (argc, argv)
     int argc;
     char **argv;
{
  int err;
  char *cmdline_dump_sects = NULL;
  unsigned num_cmdline_dump_sects = 0;

  parse_args (argc, argv);

  if (optind < (argc - 1))
    show_name = 1;

  /* When processing more than one file remember the dump requests
     issued on command line to reset them after each file.  */
  if (optind + 1 < argc && dump_sects != NULL)
    {
      cmdline_dump_sects = malloc (num_dump_sects);
      if (cmdline_dump_sects == NULL)
	error ("Out of memory allocating dump request table.");
      else
	{
	  memcpy (cmdline_dump_sects, dump_sects, num_dump_sects);
	  num_cmdline_dump_sects = num_dump_sects;
	}
    }

  err = 0;
  while (optind < argc)
    {
      err |= process_file (argv[optind++]);

      /* Reset dump requests.  */
      if (optind < argc && dump_sects != NULL)
	{
	  num_dump_sects = num_cmdline_dump_sects;
	  if (num_cmdline_dump_sects > 0)
	    memcpy (dump_sects, cmdline_dump_sects, num_cmdline_dump_sects);
	}
    }

  if (dump_sects != NULL)
    free (dump_sects);
  if (cmdline_dump_sects != NULL)
    free (cmdline_dump_sects);

  return err;
}
