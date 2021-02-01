/*
 * Exported functions of the Wine preprocessor
 *
 * Copyright 1998 Bertho A. Stultiens
 * Copyright 2002 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "wpp_private.h"
#include "wine/wpp.h"

struct pp_status pp_status;

#define HASHKEY		2039

static struct list pp_defines[HASHKEY];

#define MAXIFSTACK	64
static pp_if_state_t if_stack[MAXIFSTACK];
static int if_stack_idx = 0;

int ppy_debug, pp_flex_debug;

struct define
{
    struct list    entry;
    char          *name;
    char          *value;
};

static struct list cmdline_defines = LIST_INIT( cmdline_defines );

void *pp_xmalloc(size_t size)
{
    void *res;

    assert(size > 0);
    res = malloc(size);
    if(res == NULL)
    {
        fprintf( stderr, "Virtual memory exhausted\n" );
        exit(1);
    }
    return res;
}

void *pp_xrealloc(void *p, size_t size)
{
    void *res;

    assert(size > 0);
    res = realloc(p, size);
    if(res == NULL)
    {
        fprintf( stderr, "Virtual memory exhausted\n" );
        exit(1);
    }
    return res;
}

char *pp_xstrdup(const char *str)
{
	int len = strlen(str)+1;
	return memcpy(pp_xmalloc(len), str, len);
}

char *wpp_lookup(const char *name, int type, const char *parent_name,
                 char **include_path, int include_path_count)
{
    char *cpy;
    char *cptr;
    char *path;
    const char *ccptr;
    int i, fd;

    cpy = pp_xmalloc(strlen(name)+1);
    cptr = cpy;

    for(ccptr = name; *ccptr; ccptr++)
    {
        /* Convert to forward slash */
        if(*ccptr == '\\') {
            /* kill double backslash */
            if(ccptr[1] == '\\')
                ccptr++;
            *cptr = '/';
        }else {
            *cptr = *ccptr;
        }
        cptr++;
    }
    *cptr = '\0';

    if(type && parent_name)
    {
        /* Search directory of parent include and then -I path */
        const char *p;

        if ((p = strrchr( parent_name, '/' ))) p++;
        else p = parent_name;
        path = pp_xmalloc( (p - parent_name) + strlen(cpy) + 1 );
        memcpy( path, parent_name, p - parent_name );
        strcpy( path + (p - parent_name), cpy );
        fd = open( path, O_RDONLY );
        if (fd != -1)
        {
            close( fd );
            free( cpy );
            return path;
        }
        free( path );
    }
    /* Search -I path */
    for(i = 0; i < include_path_count; i++)
    {
        path = pp_xmalloc(strlen(include_path[i]) + strlen(cpy) + 2);
        strcpy(path, include_path[i]);
        strcat(path, "/");
        strcat(path, cpy);
        fd = open( path, O_RDONLY );
        if (fd != -1)
        {
            close( fd );
            free( cpy );
            return path;
        }
        free( path );
    }
    free( cpy );
    return NULL;
}

/* Don't comment on the hash, it's primitive but functional... */
static int pphash(const char *str)
{
	int sum = 0;
	while(*str)
		sum += *str++;
	return sum % HASHKEY;
}

pp_entry_t *pplookup(const char *ident)
{
	int idx;
	pp_entry_t *ppp;

	if(!ident)
		return NULL;
	idx = pphash(ident);
        LIST_FOR_EACH_ENTRY( ppp, &pp_defines[idx], pp_entry_t, entry )
	{
		if(!strcmp(ident, ppp->ident))
			return ppp;
	}
	return NULL;
}

static void free_pp_entry( pp_entry_t *ppp, int idx )
{
	if(ppp->iep)
	{
                list_remove( &ppp->iep->entry );
		free(ppp->iep->filename);
		free(ppp->iep);
	}
        list_remove( &ppp->entry );
	free(ppp);
}

/* initialize the define state */
void pp_init_define_state(void)
{
    int i;

    for (i = 0; i < HASHKEY; i++) list_init( &pp_defines[i] );
}

/* free the current define state */
void pp_free_define_state(void)
{
    int i;
    pp_entry_t *ppp, *ppp2;

    for (i = 0; i < HASHKEY; i++)
    {
        LIST_FOR_EACH_ENTRY_SAFE( ppp, ppp2, &pp_defines[i], pp_entry_t, entry )
        {
            free( ppp->ident );
            free( ppp->subst.text );
            free( ppp->filename );
            free_pp_entry( ppp, i );
        }
    }
}

void pp_del_define(const char *name)
{
	pp_entry_t *ppp;
	int idx = pphash(name);

	if((ppp = pplookup(name)) == NULL)
	{
		if(pp_status.pedantic)
			ppy_warning("%s was not defined", name);
		return;
	}

	if(pp_status.debug)
		printf("Deleting (%s, %d) <%s>\n", pp_status.input, pp_status.line_number, name);

	free( ppp->ident );
	free( ppp->subst.text );
	free( ppp->filename );
	free_pp_entry( ppp, idx );
}

pp_entry_t *pp_add_define(const char *def, const char *text)
{
	int len;
	char *cptr;
	int idx;
	pp_entry_t *ppp;

	idx = pphash(def);
	if((ppp = pplookup(def)) != NULL)
	{
		if(pp_status.pedantic)
			ppy_warning("Redefinition of %s\n\tPrevious definition: %s:%d", def, ppp->filename, ppp->linenumber);
		pp_del_define(def);
	}
	ppp = pp_xmalloc(sizeof(pp_entry_t));
	memset( ppp, 0, sizeof(*ppp) );
	ppp->ident = pp_xstrdup(def);
	ppp->type = def_define;
	ppp->subst.text = text ? pp_xstrdup(text) : NULL;
	ppp->filename = pp_xstrdup(pp_status.input ? pp_status.input : "<internal or cmdline>");
	ppp->linenumber = pp_status.input ? pp_status.line_number : 0;
        list_add_head( &pp_defines[idx], &ppp->entry );
	if(ppp->subst.text)
	{
		/* Strip trailing white space from subst text */
		len = strlen(ppp->subst.text);
		while(len && strchr(" \t\r\n", ppp->subst.text[len-1]))
		{
			ppp->subst.text[--len] = '\0';
		}
		/* Strip leading white space from subst text */
		for(cptr = ppp->subst.text; *cptr && strchr(" \t\r", *cptr); cptr++)
		;
		if(ppp->subst.text != cptr)
			memmove(ppp->subst.text, cptr, strlen(cptr)+1);
	}
	if(pp_status.debug)
		printf("Added define (%s, %d) <%s> to <%s>\n", pp_status.input, pp_status.line_number, ppp->ident, ppp->subst.text ? ppp->subst.text : "(null)");

	return ppp;
}

pp_entry_t *pp_add_macro(char *id, char *args[], int nargs, mtext_t *exp)
{
	int idx;
	pp_entry_t *ppp;

	idx = pphash(id);
	if((ppp = pplookup(id)) != NULL)
	{
		if(pp_status.pedantic)
			ppy_warning("Redefinition of %s\n\tPrevious definition: %s:%d", id, ppp->filename, ppp->linenumber);
		pp_del_define(id);
	}
	ppp = pp_xmalloc(sizeof(pp_entry_t));
	memset( ppp, 0, sizeof(*ppp) );
	ppp->ident	= id;
	ppp->type	= def_macro;
	ppp->margs	= args;
	ppp->nargs	= nargs;
	ppp->subst.mtext= exp;
	ppp->filename = pp_xstrdup(pp_status.input ? pp_status.input : "<internal or cmdline>");
	ppp->linenumber = pp_status.input ? pp_status.line_number : 0;
        list_add_head( &pp_defines[idx], &ppp->entry );
	if(pp_status.debug)
	{
		fprintf(stderr, "Added macro (%s, %d) <%s(%d)> to <", pp_status.input, pp_status.line_number, ppp->ident, nargs);
		for(; exp; exp = exp->next)
		{
			switch(exp->type)
			{
			case exp_text:
				fprintf(stderr, " \"%s\" ", exp->subst.text);
				break;
			case exp_stringize:
				fprintf(stderr, " #(%d) ", exp->subst.argidx);
				break;
			case exp_concat:
				fprintf(stderr, "##");
				break;
			case exp_subst:
				fprintf(stderr, " <%d> ", exp->subst.argidx);
				break;
			}
		}
		fprintf(stderr, ">\n");
	}
	return ppp;
}


/*
 *-------------------------------------------------------------------------
 * Include management
 *-------------------------------------------------------------------------
 */
#if defined(_WIN32) || defined(__MSDOS__)
#define INCLUDESEPARATOR	";"
#else
#define INCLUDESEPARATOR	":"
#endif

static char **includepath;
static int nincludepath = 0;

void wpp_add_include_path(const char *path)
{
	char *tok;
	char *cpy = pp_xstrdup(path);

	tok = strtok(cpy, INCLUDESEPARATOR);
	while(tok)
	{
		if(*tok) {
			char *dir;
			char *cptr;

			dir = pp_xstrdup(tok);
			for(cptr = dir; *cptr; cptr++)
			{
				/* Convert to forward slash */
				if(*cptr == '\\')
					*cptr = '/';
			}
			/* Kill eventual trailing '/' */
			if(*(cptr = dir + strlen(dir)-1) == '/')
				*cptr = '\0';

			/* Add to list */
			includepath = pp_xrealloc(includepath, (nincludepath+1) * sizeof(*includepath));
			includepath[nincludepath] = dir;
			nincludepath++;
		}
		tok = strtok(NULL, INCLUDESEPARATOR);
	}
	free(cpy);
}

char *wpp_find_include(const char *name, const char *parent_name)
{
    return wpp_lookup(name, !!parent_name, parent_name, includepath, nincludepath);
}

void *pp_open_include(const char *name, int type, const char *parent_name, char **newpath)
{
    char *path;
    void *fp;

    if (!(path = wpp_lookup(name, type, parent_name, includepath, nincludepath))) return NULL;
    fp = fopen(path, "rt");

    if (fp)
    {
        if (pp_status.debug)
            printf("Going to include <%s>\n", path);
        if (newpath) *newpath = path;
        else free( path );
        return fp;
    }
    free( path );
    return NULL;
}

/*
 *-------------------------------------------------------------------------
 * #if, #ifdef, #ifndef, #else, #elif and #endif state management
 *
 * #if state transitions are made on basis of the current TOS and the next
 * required state. The state transitions are required to housekeep because
 * #if:s can be nested. The ignore case is activated to prevent output from
 * within a false clause.
 * Some special cases come from the fact that the #elif cases are not
 * binary, but three-state. The problem is that all other elif-cases must
 * be false when one true one has been found. A second problem is that the
 * #else clause is a final clause. No extra #else:s may follow.
 *
 * The states mean:
 * if_true	Process input to output
 * if_false	Process input but no output
 * if_ignore	Process input but no output
 * if_elif	Process input but no output
 * if_elsefalse	Process input but no output
 * if_elsettrue	Process input to output
 *
 * The possible state-sequences are [state(stack depth)] (rest can be deduced):
 *	TOS		#if 1		#else			#endif
 *	if_true(n)	if_true(n+1)	if_elsefalse(n+1)
 *	if_false(n)	if_ignore(n+1)	if_ignore(n+1)
 *	if_elsetrue(n)	if_true(n+1)	if_elsefalse(n+1)
 *	if_elsefalse(n)	if_ignore(n+1)	if_ignore(n+1)
 *	if_elif(n)	if_ignore(n+1)	if_ignore(n+1)
 *	if_ignore(n)	if_ignore(n+1)	if_ignore(n+1)
 *
 *	TOS		#if 1		#elif 0		#else		#endif
 *	if_true(n)	if_true(n+1)	if_elif(n+1)	if_elif(n+1)
 *	if_false(n)	if_ignore(n+1)	if_ignore(n+1)	if_ignore(n+1)
 *	if_elsetrue(n)	if_true(n+1)	if_elif(n+1)	if_elif(n+1)
 *	if_elsefalse(n)	if_ignore(n+1)	if_ignore(n+1)	if_ignore(n+1)
 *	if_elif(n)	if_ignore(n+1)	if_ignore(n+1)	if_ignore(n+1)
 *	if_ignore(n)	if_ignore(n+1)	if_ignore(n+1)	if_ignore(n+1)
 *
 *	TOS		#if 0		#elif 1		#else		#endif
 *	if_true(n)	if_false(n+1)	if_true(n+1)	if_elsefalse(n+1)
 *	if_false(n)	if_ignore(n+1)	if_ignore(n+1)	if_ignore(n+1)
 *	if_elsetrue(n)	if_false(n+1)	if_true(n+1)	if_elsefalse(n+1)
 *	if_elsefalse(n)	if_ignore(n+1)	if_ignore(n+1)	if_ignore(n+1)
 *	if_elif(n)	if_ignore(n+1)	if_ignore(n+1)	if_ignore(n+1)
 *	if_ignore(n)	if_ignore(n+1)	if_ignore(n+1)	if_ignore(n+1)
 *
 *-------------------------------------------------------------------------
 */
static const char * const pp_if_state_str[] = {
	"if_false",
	"if_true",
	"if_elif",
	"if_elsefalse",
	"if_elsetrue",
	"if_ignore"
};

void pp_push_if(pp_if_state_t s)
{
	if(if_stack_idx >= MAXIFSTACK)
		pp_internal_error(__FILE__, __LINE__, "#if-stack overflow; #{if,ifdef,ifndef} nested too deeply (> %d)", MAXIFSTACK);

	if(pp_flex_debug)
		fprintf(stderr, "Push if %s:%d: %s(%d) -> %s(%d)\n", pp_status.input, pp_status.line_number, pp_if_state_str[pp_if_state()], if_stack_idx, pp_if_state_str[s], if_stack_idx+1);

	if_stack[if_stack_idx++] = s;

	switch(s)
	{
	case if_true:
	case if_elsetrue:
		break;
	case if_false:
	case if_elsefalse:
	case if_elif:
	case if_ignore:
		pp_push_ignore_state();
		break;
	default:
		pp_internal_error(__FILE__, __LINE__, "Invalid pp_if_state (%d)", (int)pp_if_state());
	}
}

pp_if_state_t pp_pop_if(void)
{
	if(if_stack_idx <= 0)
	{
		ppy_error("#{endif,else,elif} without #{if,ifdef,ifndef} (#if-stack underflow)");
		return if_error;
	}

	switch(pp_if_state())
	{
	case if_true:
	case if_elsetrue:
		break;
	case if_false:
	case if_elsefalse:
	case if_elif:
	case if_ignore:
		pp_pop_ignore_state();
		break;
	default:
		pp_internal_error(__FILE__, __LINE__, "Invalid pp_if_state (%d)", (int)pp_if_state());
	}

	if(pp_flex_debug)
		fprintf(stderr, "Pop if %s:%d: %s(%d) -> %s(%d)\n",
				pp_status.input,
				pp_status.line_number,
				pp_if_state_str[pp_if_state()],
				if_stack_idx,
				pp_if_state_str[if_stack[if_stack_idx <= 1 ? if_true : if_stack_idx-2]],
				if_stack_idx-1);

	return if_stack[--if_stack_idx];
}

pp_if_state_t pp_if_state(void)
{
	if(!if_stack_idx)
		return if_true;
	else
		return if_stack[if_stack_idx-1];
}


void pp_next_if_state(int i)
{
	switch(pp_if_state())
	{
	case if_true:
	case if_elsetrue:
		pp_push_if(i ? if_true : if_false);
		break;
	case if_false:
	case if_elsefalse:
	case if_elif:
	case if_ignore:
		pp_push_if(if_ignore);
		break;
	default:
		pp_internal_error(__FILE__, __LINE__, "Invalid pp_if_state (%d) in #{if,ifdef,ifndef} directive", (int)pp_if_state());
	}
}

int pp_get_if_depth(void)
{
	return if_stack_idx;
}

static void generic_msg(const char *s, const char *t, const char *n, va_list ap)
{
	fprintf(stderr, "%s:%d:%d: %s: ", pp_status.input ? pp_status.input : "stdin",
                pp_status.line_number, pp_status.char_number, t);
	vfprintf(stderr, s, ap);
	fprintf(stderr, "\n");
}

int ppy_error(const char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	generic_msg(s, "error", ppy_text, ap);
	va_end(ap);
	exit(1);
}

int ppy_warning(const char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	generic_msg(s, "warning", ppy_text, ap);
	va_end(ap);
	return 0;
}

void pp_internal_error(const char *file, int line, const char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	fprintf(stderr, "Internal error (please report) %s %d: ", file, line);
	vfprintf(stderr, s, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(3);
}

static void add_cmdline_defines(void)
{
    struct define *def;

    LIST_FOR_EACH_ENTRY( def, &cmdline_defines, struct define, entry )
    {
        if (def->value) pp_add_define( def->name, def->value );
    }
}

static void add_special_defines(void)
{
    time_t now = time(NULL);
    pp_entry_t *ppp;
    char buf[32];

    strftime(buf, sizeof(buf), "\"%b %d %Y\"", localtime(&now));
    pp_add_define( "__DATE__", buf );

    strftime(buf, sizeof(buf), "\"%H:%M:%S\"", localtime(&now));
    pp_add_define( "__TIME__", buf );

    ppp = pp_add_define( "__FILE__", "" );
    ppp->type = def_special;

    ppp = pp_add_define( "__LINE__", "" );
    ppp->type = def_special;
}

/* add a define to the preprocessor list */
static void wpp_add_define( const char *name, const char *value )
{
    struct define *def;

    if (!value) value = "";

    LIST_FOR_EACH_ENTRY( def, &cmdline_defines, struct define, entry )
    {
        if (!strcmp( def->name, name ))
        {
            free( def->value );
            def->value = pp_xstrdup(value);
            return;
        }
    }

    def = pp_xmalloc( sizeof(*def) );
    def->name  = pp_xstrdup(name);
    def->value = pp_xstrdup(value);
    list_add_head( &cmdline_defines, &def->entry );
}


/* undefine a previously added definition */
void wpp_del_define( const char *name )
{
    struct define *def;

    LIST_FOR_EACH_ENTRY( def, &cmdline_defines, struct define, entry )
    {
        if (!strcmp( def->name, name ))
        {
            free( def->value );
            def->value = NULL;
            return;
        }
    }
}


/* add a command-line define of the form NAME=VALUE */
void wpp_add_cmdline_define( const char *value )
{
    char *p;
    char *str = pp_xstrdup(value);

    p = strchr( str, '=' );
    if (p) *p++ = 0;
    wpp_add_define( str, p );
    free( str );
}


/* set the various debug flags */
void wpp_set_debug( int lex_debug, int parser_debug, int msg_debug )
{
    pp_flex_debug   = lex_debug;
    ppy_debug       = parser_debug;
    pp_status.debug = msg_debug;
}


/* set the pedantic mode */
void wpp_set_pedantic( int on )
{
    pp_status.pedantic = on;
}


/* the main preprocessor parsing loop */
int wpp_parse( const char *input, FILE *output )
{
    int ret;

    pp_status.input = NULL;
    pp_status.line_number = 1;
    pp_status.char_number = 1;

    pp_init_define_state();
    add_cmdline_defines();
    add_special_defines();

    if (!input) pp_status.file = stdin;
    else if (!(pp_status.file = fopen(input, "rt")))
        ppy_error("Could not open %s\n", input);

    pp_status.input = input ? pp_xstrdup(input) : NULL;

    ppy_out = output;
    pp_writestring("# 1 \"%s\" 1\n", input ? input : "");

    ret = ppy_parse();

    if (input)
    {
	fclose(pp_status.file);
	free(pp_status.input);
    }
    /* Clean if_stack, it could remain dirty on errors */
    while (pp_get_if_depth()) pp_pop_if();
    pp_free_define_state();
    return ret;
}
