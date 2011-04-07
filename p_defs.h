/* -HEADER----------------------------------------------------------------------
UniCC LALR(1) Parser Generator 
Copyright (C) 2006-2009 by Phorward Software Technologies, Jan Max Meyer
http://www.phorward-software.com ++ mail@phorward-software.com

File:	p_defs.h
Author:	Jan Max Meyer
Usage:	Definition strings and constant values
----------------------------------------------------------------------------- */

#ifndef P_DEFS_H
#define P_DEFS_H

/* Special symbol names */
#define P_WHITESPACE			"(whitespace)"
#define P_ERROR_RESYNC			"(error_resync)"
#define P_EMBEDDED				"(embedded_%d)"

/* Characters for virtual nonterminal names */
#define P_POSITIVE_CLOSURE		'+'
#define P_KLEENE_CLOSURE		'*'
#define P_OPTIONAL_CLOSURE		'?'

/* Character for rewritten virtual nonterminal */
#define P_REWRITTEN_TOKEN		"\'"
#define P_REWRITTEN_CCL			"#"
#define P_REWRITTEN_KW			"~"

/* Regular expression terminals */
#define P_REGEX_AUTO_NAME		"regex"

/* Default End-of-Input string */
#define P_DEF_EOF_SYMBOL		"\\0"

#endif