/* -MODULE----------------------------------------------------------------------
UniCC LALR(1) Parser Generator 
Copyright (C) 2006-2010 by Phorward Software Technologies, Jan Max Meyer
http://unicc.phorward-software.com/ ++ unicc<<AT>>phorward-software<<DOT>>com

File:	p_build.c
Author:	Jan Max Meyer
Usage:	Builds the target parser based on configurations from a
		parser definition file.
		
You may use, modify and distribute this software under the terms and conditions
of the Artistic License, version 2. Please see LICENSE for more information.
----------------------------------------------------------------------------- */

/*
 * Includes
 */
#include "p_global.h"
#include "p_error.h"
#include "p_proto.h"

/*
 * Defines
 */
#define	LEN_EXT		"_len"

/*
 * Global variables
 */

/*
 * Functions
 */

/* -FUNCTION--------------------------------------------------------------------
	Function:		p_build_code()
	
	Author:			Jan Max Meyer
	
	Usage:			This is the main function for the code-generator.
					It first reads a target language generator, and then
					constructs code segments, which are finally pasted into
					the parser template (which is defined within the
					<driver>-tag of the generator file).
					
	Parameters:		FILE*		stream				Stream, where the output
													file is written to.
					PARSER*		parser				Parser information structure
	
	Returns:		void
  
	~~~ CHANGES & NOTES ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	Date:		Author:			Note:
----------------------------------------------------------------------------- */
void p_build_code( PARSER* parser )
{
	GENERATOR		generator;
	GENERATOR*		gen					= (GENERATOR*)NULL;
	XML_T			file;
	FILE*			stream;
	
	uchar			xml_file			[ BUFSIZ + 1 ];
	uchar*			tpldir;
	uchar*			complete			= (uchar*)NULL;
	uchar*			all					= (uchar*)NULL;
	uchar*			action_table		= (uchar*)NULL;
	uchar*			action_table_row	= (uchar*)NULL;
	uchar*			goto_table			= (uchar*)NULL;
	uchar*			goto_table_row		= (uchar*)NULL;
	uchar*			prod_rhs_count		= (uchar*)NULL;
	uchar*			prod_lhs			= (uchar*)NULL;
	uchar*			def_prod			= (uchar*)NULL;
	uchar*			char_map			= (uchar*)NULL;
	uchar*			char_map_sym		= (uchar*)NULL;
	uchar*			whitespaces			= (uchar*)NULL;
	uchar*			symbols				= (uchar*)NULL;
	uchar*			productions			= (uchar*)NULL;
	uchar*			dfa_select			= (uchar*)NULL;
	uchar*			dfa_idx				= (uchar*)NULL;
	uchar*			dfa_idx_row			= (uchar*)NULL;
	uchar*			dfa_char			= (uchar*)NULL;
	uchar*			dfa_trans			= (uchar*)NULL;
	uchar*			dfa_accept			= (uchar*)NULL;
	uchar*			dfa_accept_row		= (uchar*)NULL;
	uchar*			kw_invalid_suffix	= (uchar*)NULL;
	uchar*			type_def			= (uchar*)NULL;
	uchar*			actions				= (uchar*)NULL;
	uchar*			scan_actions		= (uchar*)NULL;
	uchar*			top_value			= (uchar*)NULL;
	uchar*			act					= (uchar*)NULL;
	uchar*			filename;

	int				max_action			= 0;
	int				max_goto			= 0;
	int				max_dfa_idx			= 0;
	int				max_dfa_accept		= 0;
	int				kw_count			= 0;
	int				max_symbol_name		= 0;
	int				column;
	int				charmap_count		= 0;
	int				row;
	LIST*			l;
	LIST*			m;
	LIST*			n;
	pregex_dfa*		dfa;
	pregex_dfa_st*	dfa_st;
	pregex_dfa_ent*	dfa_ent;
	SYMBOL*			sym;
	STATE*			st;
	TABCOL*			col;
	PROD*			p;
	PROD*			goalprod;
	VTYPE*			vt;
	CCL				c;
	int				i;
	BOOLEAN			is_default_code;

	PROC( "p_build_code" );
	PARMS( "parser", "%p", parser );

	gen = &generator;
	memset( gen, 0, sizeof( GENERATOR ) );

	sprintf( xml_file, "%s.xml", parser->p_language );
	VARS( "xml_file", "%s", xml_file );

	if( !pfileexists( xml_file ) && ( tpldir = getenv( "UNICC_TPLDIR" ) ) )
	{
		MSG( "File not found, trying to get it from UNICC_TPLDIR" );

		sprintf( xml_file, "%s%c%s.xml",
			tpldir, 
				( ( tpldir[ pstrlen( tpldir ) ] == PPATHSEP )
					? '\0' : PPATHSEP ), parser->p_language );

		VARS( "xml_file", "%s", xml_file );
	}
	
	VARS( "xml_file", "%s", xml_file );
	
	MSG( "Loading generator" );
	if( !p_load_generator( gen, xml_file ) )
		VOIDRET;

	/* Now that we have the generator, do some code generation-related
		integrity preparatories on the grammar */
	
	MSG( "Performing code generation-related integrity preparatories" );
	for( l = parser->symbols; l; l = l->next )
	{
		sym = (SYMBOL*)( l->pptr );
		
		if( !( sym->vtype ) )
			sym->vtype = parser->p_def_type;

		if( sym->type == SYM_NON_TERMINAL && !( sym->vtype ) &&
			( gen->vstack_def_type && *( gen->vstack_def_type ) ) )
			sym->vtype = p_create_vtype( parser, gen->vstack_def_type );
		else if( IS_TERMINAL( sym ) && !( sym->keyword )
			&& !( sym->vtype ) && ( gen->vstack_term_type &&
				*( gen->vstack_term_type ) ) )
			sym->vtype = p_create_vtype( parser, gen->vstack_term_type );
	}

	/* Create piece of code for the value at the top of the value stack
		(e.g. used to store the next terminal character onto the value stack) */
	if( list_count( parser->vtypes ) == 1 )
		top_value = p_tpl_insert( gen->action_single,
			GEN_WILD_PREFIX "offset", p_int_to_str( 0 ), TRUE,
				(uchar*)NULL );
	else if( ( vt = p_find_vtype( parser, gen->vstack_term_type ) ) )
			top_value = p_tpl_insert( gen->action_union,
				GEN_WILD_PREFIX "offset", p_int_to_str( 0 ), TRUE,
					GEN_WILD_PREFIX "attribute",
						p_tpl_insert( gen->vstack_union_att,
							GEN_WILD_PREFIX "value-type-id",
									p_int_to_str( vt->id ), TRUE,
										(uchar*)NULL ), TRUE,
											(uchar*)NULL );
	else
		/* ERROR */
		;

	/* Build action, goto and dfa_select tables */
	MSG( "Action, Goto and DFA selection table" );
	for( l = parser->lalr_states, i = 0; l; l = l->next, i++ )
	{
		st = (STATE*)(l->pptr);
		
		/* Action table */
		action_table_row = p_tpl_insert( gen->acttab.row_start,
				GEN_WILD_PREFIX "number-of-columns",
					p_int_to_str( list_count( st->actions ) ), TRUE,
				GEN_WILD_PREFIX "state-number",
					p_int_to_str( st->state_id ), TRUE,
						(uchar*)NULL );

		if( max_action < list_count( st->actions ) )
			max_action = list_count( st->actions );

		for( m = st->actions, column = 0, kw_count = 0;
				m; m = m->next, column++ )
		{
			col = (TABCOL*)(m->pptr);

			action_table_row = p_str_append( action_table_row,
				p_tpl_insert( gen->acttab.col,
					GEN_WILD_PREFIX "symbol",
							p_int_to_str( col->symbol->id ), TRUE,
					GEN_WILD_PREFIX "action", p_int_to_str( col->action ), TRUE,
					GEN_WILD_PREFIX "index", p_int_to_str( col->index ), TRUE,
					GEN_WILD_PREFIX "column", p_int_to_str( column ), TRUE,
						(uchar*)NULL ), TRUE );

			if( col->symbol->keyword )
				kw_count++;

			if( m->next )
				action_table_row = p_str_append( action_table_row,
					gen->acttab.col_sep, FALSE );
		}

		action_table_row = p_str_append( action_table_row,
				p_tpl_insert( gen->acttab.row_end,
					GEN_WILD_PREFIX "number-of-columns",
						p_int_to_str( list_count( st->actions ) ), TRUE,
					GEN_WILD_PREFIX "state-number",
						p_int_to_str( st->state_id ), TRUE,
							(uchar*)NULL ), TRUE );

		if( l->next )
			action_table_row = p_str_append( action_table_row,
				gen->acttab.row_sep, FALSE );

		action_table = p_str_append( action_table, action_table_row, TRUE );

		/* Goto table */
		goto_table_row = p_tpl_insert( gen->gotab.row_start,
				GEN_WILD_PREFIX "number-of-columns",
					p_int_to_str( list_count( st->gotos ) ), TRUE,
				GEN_WILD_PREFIX "state-number",
					p_int_to_str( st->state_id ), TRUE,
						(uchar*)NULL );

		if( max_goto < list_count( st->gotos ) )
			max_goto = list_count( st->gotos );

		for( m = st->gotos, column = 0; m; m = m->next, column++ )
		{
			col = (TABCOL*)(m->pptr);

			goto_table_row = p_str_append( goto_table_row,
				p_tpl_insert( gen->gotab.col,
					GEN_WILD_PREFIX "symbol",
						p_int_to_str( col->symbol->id ), TRUE,
					GEN_WILD_PREFIX "action",
						p_int_to_str( col->action ), TRUE,
					GEN_WILD_PREFIX "index",
						p_int_to_str( col->index ), TRUE,
					GEN_WILD_PREFIX "column",
						p_int_to_str( column ), TRUE,
					(uchar*)NULL ), TRUE );

			if( m->next )
				goto_table_row = p_str_append( goto_table_row,
					gen->gotab.col_sep, FALSE );
		}

		goto_table_row = p_str_append( goto_table_row,
				p_tpl_insert( gen->gotab.row_end,
					GEN_WILD_PREFIX "number-of-columns",
						p_int_to_str( list_count( st->actions ) ), TRUE,
					GEN_WILD_PREFIX "state-number",
						p_int_to_str( st->state_id ), TRUE,
					(uchar*)NULL ), TRUE );

		if( l->next )
			goto_table_row = p_str_append( goto_table_row,
				gen->gotab.row_sep, FALSE );

		goto_table = p_str_append( goto_table, goto_table_row, TRUE );

		/* Only in context-sensitive model */
		if( parser->p_model == MODEL_CONTEXT_SENSITIVE )
		{
			/* dfa machine selection */
			dfa_select = p_str_append( dfa_select,
				p_tpl_insert( gen->dfa_select.col,
					GEN_WILD_PREFIX "machine",
						p_int_to_str( list_find( parser->kw, st->dfa ) ), TRUE,
							(char*)NULL ), TRUE );

			if( l->next )
				dfa_select = p_str_append( dfa_select,
								gen->dfa_select.col_sep, FALSE );
		}
		
		/* Default production table */
		def_prod = p_str_append( def_prod,
				p_tpl_insert( gen->defprod.col,
					GEN_WILD_PREFIX "state-number",
						p_int_to_str( st->state_id ), TRUE,
					GEN_WILD_PREFIX "production-number",
						p_int_to_str(
							( ( st->def_prod ) ? st->def_prod->id : -1 ) ),
								TRUE, (uchar*)NULL ), TRUE );

		if( l->next )
			def_prod = p_str_append( def_prod, gen->defprod.col_sep, FALSE );
	}

	/* Production length and production left-hand side tables */
	MSG( "Tables for production length and left-hand side association" );
	for( l = parser->productions, row = 0; l; l = l->next, row++ )
	{
		p = (PROD*)l->pptr;

		prod_rhs_count = p_str_append( prod_rhs_count,
							p_tpl_insert( gen->prodlen.col,
								GEN_WILD_PREFIX "length-of-rhs",
									p_int_to_str( list_count( p->rhs ) ), TRUE,
								GEN_WILD_PREFIX "production-number",
									p_int_to_str( row ), TRUE,
								(uchar*)NULL ), TRUE );

		prod_lhs = p_str_append( prod_lhs,
							p_tpl_insert( gen->prodlhs.col,
								GEN_WILD_PREFIX "lhs",
									p_int_to_str( p->lhs->id ), TRUE,
								GEN_WILD_PREFIX "production-number",
									p_int_to_str( row ), TRUE,
								(uchar*)NULL ), TRUE );

		if( l->next )
		{
			prod_rhs_count = p_str_append( prod_rhs_count,
									gen->prodlen.col_sep, FALSE );
			prod_lhs = p_str_append( prod_lhs,
									gen->prodlhs.col_sep, FALSE );
		}
	}

	/* Lexical recognition machine table composition */
	MSG( "Lexical recognition machine" );
	for( l = parser->kw, row = 0, column = 0; l; l = list_next( l ), row++ )
	{
		dfa = (pregex_dfa*)list_access( l );

		/* Row start */
		dfa_idx_row = p_tpl_insert( gen->dfa_idx.row_start,
				GEN_WILD_PREFIX "number-of-columns",
					p_int_to_str( list_count( dfa->states ) ), TRUE,
				GEN_WILD_PREFIX "row",
					p_int_to_str( row ), TRUE,
				(uchar*)NULL );

		dfa_accept_row = p_tpl_insert( gen->dfa_accept.row_start,
				GEN_WILD_PREFIX "number-of-columns",
					p_int_to_str( list_count( dfa->states ) ), TRUE,
				GEN_WILD_PREFIX "row",
					p_int_to_str( row ), TRUE,
				(uchar*)NULL );

		if( max_dfa_idx < list_count( dfa->states ) )
			max_dfa_accept = max_dfa_idx = list_count( dfa->states );

		/* Building row entries */
		LISTFOR( dfa->states, m )
		{
			dfa_st = (pregex_dfa_st*)list_access( m );
			VARS( "dfa_st", "%p", dfa_st );

			if( dfa_char && dfa_trans )
			{
				dfa_char = p_str_append( dfa_char,
						gen->dfa_char.col_sep, FALSE );
				dfa_trans = p_str_append( dfa_trans,
						gen->dfa_trans.col_sep, FALSE );
			}

			dfa_idx_row = p_str_append( dfa_idx_row,
				p_tpl_insert( gen->dfa_idx.col,
					GEN_WILD_PREFIX "index",
						p_int_to_str( column ), TRUE,
					(uchar*)NULL ), TRUE );

			dfa_accept_row = p_str_append( dfa_accept_row,
				p_tpl_insert( gen->dfa_accept.col,
					GEN_WILD_PREFIX "accept",
						p_int_to_str( dfa_st->accept ), TRUE,
					(uchar*)NULL ), TRUE );

			/* Iterate trough all transitions */
			MSG( "Iterating to transitions of DFA" );
			LISTFOR( dfa_st->trans, n )
			{
				dfa_ent = (pregex_dfa_ent*)list_access( n );

				for( c = dfa_ent->ccl; c && c->begin != CCL_MAX; c++ )
				{
					dfa_char = p_str_append( dfa_char,
								p_tpl_insert( gen->dfa_char.col,
								GEN_WILD_PREFIX "from",
									p_int_to_str( c->begin ), TRUE,
								GEN_WILD_PREFIX "to",
									p_int_to_str( c->end ), TRUE,
								GEN_WILD_PREFIX "goto",
									p_int_to_str( dfa_st->accept ), TRUE,
								(uchar*)NULL ), TRUE );

					dfa_trans = p_str_append( dfa_trans,
								p_tpl_insert( gen->dfa_trans.col,
									GEN_WILD_PREFIX "goto",
									p_int_to_str( dfa_ent->go_to ), TRUE,
								(uchar*)NULL ), TRUE );


					dfa_char = p_str_append( dfa_char,
									gen->dfa_char.col_sep, FALSE );
					dfa_trans = p_str_append( dfa_trans,
									gen->dfa_trans.col_sep, FALSE );
									
					column++;
				}
			}

			/* DFA transition end marker */
			dfa_char = p_str_append( dfa_char,
					p_tpl_insert( gen->dfa_char.col,
						GEN_WILD_PREFIX "from",
							p_int_to_str( -1 ), TRUE,
						GEN_WILD_PREFIX "to",
							p_int_to_str( -1 ), TRUE,
						(uchar*)NULL ), TRUE );

			/* DFA transition */
			dfa_trans = p_str_append( dfa_trans,
					p_tpl_insert( gen->dfa_trans.col,
						GEN_WILD_PREFIX "goto",
							p_int_to_str( -1 ),
					TRUE, (uchar*)NULL ), TRUE );

			column++;

			if( list_next( m ) )
			{
				dfa_idx_row = p_str_append( dfa_idx_row,
						gen->dfa_idx.col_sep, FALSE );
				dfa_accept_row = p_str_append( dfa_accept_row,
						gen->dfa_accept.col_sep, FALSE );
			}
		}

		/* Row end */
		dfa_idx_row = p_str_append( dfa_idx_row,
				p_tpl_insert( gen->dfa_idx.row_end,
					GEN_WILD_PREFIX "number-of-columns",
						p_int_to_str( list_count( dfa->states ) ), TRUE,
					GEN_WILD_PREFIX "row",
						p_int_to_str( row ), TRUE,
					(uchar*)NULL ), TRUE );

		dfa_accept_row = p_str_append( dfa_accept_row,
				p_tpl_insert( gen->dfa_accept.row_end,
					GEN_WILD_PREFIX "number-of-columns",
						p_int_to_str( list_count( dfa->states ) ), TRUE,
					GEN_WILD_PREFIX "row", p_int_to_str( row ), TRUE,
					(uchar*)NULL ), TRUE );

		if( list_next( l ) )
		{
			dfa_idx_row = p_str_append( dfa_idx_row,
				gen->dfa_idx.row_sep, FALSE );
			dfa_accept_row = p_str_append( dfa_accept_row,
				gen->dfa_accept.row_sep, FALSE );
		}

		dfa_idx = p_str_append( dfa_idx, dfa_idx_row, TRUE );
		dfa_accept = p_str_append( dfa_accept, dfa_accept_row, TRUE );
	}
	
	MSG( "Construct map of invalid characters (keyword recognition)" );
	
	/* Map of invalid keyword suffix characters */
	for( c = parser->p_invalid_suf; c && c->begin != CCL_MAX; c++ )
	{
		VARS( "c->begin", "%d", c->begin );
		VARS( "c->end", "%d", c->end );
		
		if( kw_invalid_suffix )
			kw_invalid_suffix = p_str_append(
				kw_invalid_suffix, gen->kw_invalid_suffix.col_sep,
					FALSE );
		
		kw_invalid_suffix = p_str_append( kw_invalid_suffix,
			p_tpl_insert( gen->kw_invalid_suffix.col,
				GEN_WILD_PREFIX "from",
					p_int_to_str( c->begin ), TRUE,
				GEN_WILD_PREFIX "to",
					p_int_to_str( c->end ), TRUE,
				(uchar*)NULL ), TRUE );
	}

	MSG( "Construct character map" );

	/* Character map */
	LISTFOR( parser->symbols, l )
	{
		sym = (SYMBOL*)list_access( l );

		if( IS_TERMINAL( sym ) && !( sym->keyword ) )
		{
			for( c = sym->ccl; c && c->begin != CCL_MAX; c++ )
			{
				if( char_map )
				{
					char_map = p_str_append(
						char_map, gen->charmap.col_sep, FALSE );
					char_map_sym = p_str_append(
						char_map_sym, gen->charmap_sym.col_sep, FALSE );
				}

				char_map = p_str_append( char_map,
					p_tpl_insert( gen->charmap.col,
						GEN_WILD_PREFIX "from",
							p_int_to_str( c->begin ), TRUE,
						GEN_WILD_PREFIX "to",
							p_int_to_str( c->end ), TRUE,
						GEN_WILD_PREFIX "symbol",
							p_int_to_str( sym->id ), TRUE,
						(uchar*)NULL ), TRUE );
						
				char_map_sym = p_str_append( char_map_sym,
					p_tpl_insert( gen->charmap_sym.col,
						GEN_WILD_PREFIX "from",
							p_int_to_str( c->begin ), TRUE,
						GEN_WILD_PREFIX "to",
							p_int_to_str( c->end ), TRUE,
						GEN_WILD_PREFIX "symbol",
							p_int_to_str( sym->id ), TRUE,
						(uchar*)NULL ), TRUE );
			}
			
			charmap_count += ccl_size( sym->ccl );
		}
	}

	/* Whitespace identification table and symbol-name-table */
	LISTFOR( parser->symbols, l ) /* Okidoki, now do the generation */
	{
		sym = (SYMBOL*)list_access( l );

		/* printf( "sym->name = >%s< at %d\n", sym->name, sym->id ); */
		if( sym->whitespace )
			whitespaces = p_str_append( whitespaces,
				gen->whitespace.col_true, FALSE );
		else
			whitespaces = p_str_append( whitespaces,
				gen->whitespace.col_false, FALSE );

		symbols = p_str_append( symbols, p_tpl_insert( gen->symbols.col,
				GEN_WILD_PREFIX "symbol-name",
						p_escape_for_target( gen, sym->name, FALSE ), TRUE,
					GEN_WILD_PREFIX "symbol", p_int_to_str( sym->id ), TRUE,
						GEN_WILD_PREFIX "type", p_int_to_str( sym->type ), TRUE,
							(char*)NULL ), TRUE );

		if( max_symbol_name < (int)strlen( sym->name ) )
			max_symbol_name = (int)strlen( sym->name );

		if( l->next )
		{
			whitespaces = p_str_append( whitespaces,
				gen->whitespace.col_sep, FALSE );
			symbols = p_str_append( symbols,
				gen->symbols.col_sep, FALSE );
		}
	}

	/* Type definition union */
	if( list_count( parser->vtypes ) == 1 )
	{
		vt = (VTYPE*)( parser->vtypes->pptr );
		type_def = p_tpl_insert( gen->vstack_single,
				GEN_WILD_PREFIX "value-type", vt->real_def, FALSE,
					(uchar*)NULL );
	}
	else
	{
		type_def = p_tpl_insert( gen->vstack_union_start,
				GEN_WILD_PREFIX "number-of-value-types",
					p_int_to_str( list_count( parser->vtypes ) ),
						TRUE, (uchar*)NULL );

		for( l = parser->vtypes; l; l = l->next )
		{
			vt = (VTYPE*)(l->pptr);

			type_def = p_str_append( type_def,
				p_tpl_insert( gen->vstack_union_def,
					GEN_WILD_PREFIX "value-type", vt->real_def, FALSE,
						GEN_WILD_PREFIX "attribute",
								p_tpl_insert( gen->vstack_union_att,
									GEN_WILD_PREFIX "value-type-id",
											p_int_to_str( vt->id ), TRUE,
												(uchar*)NULL ), TRUE,
						GEN_WILD_PREFIX "value-type-id",
							p_int_to_str( vt->id ), TRUE,
						(uchar*)NULL ), TRUE );
		}

		type_def = p_str_append( type_def,
					p_tpl_insert( gen->vstack_union_end,
						GEN_WILD_PREFIX "number-of-value-types",
							p_int_to_str( list_count( parser->vtypes ) ),
								TRUE, (uchar*)NULL ), TRUE );
	}

	/* Reduction action code and production definition table */
	for( l = parser->productions, row = 0; l; l = l->next, row++ )
	{
		p = (PROD*)( l->pptr );

		actions = p_str_append( actions, p_tpl_insert( gen->action_start, 
			GEN_WILD_PREFIX "production-number", p_int_to_str( p->id ), TRUE,
				(uchar*)NULL ), TRUE );

		/* Select the semantic code to be processed! */
		act = (uchar*)NULL;

		if( TRUE ) /* !( p->lhs->keyword )  */
		{
			is_default_code = FALSE;

			if( p->code )
				act = p->code;
			else if( list_count( p->rhs ) == 0 )
			{
				act = parser->p_def_action_e;
				is_default_code = TRUE;
			}
			else
			{
				act = parser->p_def_action;
				is_default_code = TRUE;
			}
			
			if( is_default_code &&
				( p->lhs->whitespace ||
					list_find( p->rhs, parser->error ) > -1 ) )
			{
				act = (uchar*)NULL;
			}

			if( act )
			{
				if( gen->code_localization )
				{
					actions = p_str_append( actions,
						p_tpl_insert( gen->code_localization,
							GEN_WILD_PREFIX "line",
								p_int_to_str( p->code_at ), TRUE,	
							(uchar*)NULL ),
						TRUE );
				}

				act = p_build_action( parser, gen, p, act, is_default_code );
				actions = p_str_append( actions, act, TRUE );
			}
		}

		actions = p_str_append( actions, p_tpl_insert( gen->action_end, 
			GEN_WILD_PREFIX "production-number", p_int_to_str( p->id ), TRUE,
				(uchar*)NULL ), TRUE );

		/* Generate production name */
		productions = p_str_append( productions, p_tpl_insert(
			gen->productions.col,
				GEN_WILD_PREFIX "production", p_escape_for_target(
						gen, p_mkproduction_str( p ), TRUE ), TRUE,
			(uchar*)NULL ), TRUE );
			
		if( l->next )
			productions = p_str_append( productions,
				gen->productions.col_sep, FALSE );
	}

	/* Scanner action code */
	for( l = parser->symbols, row = 0; l; l = l->next, row++ )
	{
		sym = (SYMBOL*)( l->pptr );
		if( sym->type != SYM_REGEX_TERMINAL )
			continue;

		scan_actions = p_str_append( scan_actions,
			p_tpl_insert(gen->scan_action_start,
				GEN_WILD_PREFIX "symbol-number", p_int_to_str( sym->id ), TRUE,
					(uchar*)NULL ), TRUE );

		/* Select the semantic code to be processed! */
		act = (uchar*)NULL;

		if( sym->code )
			act = sym->code;

		if( act )
		{
			act = p_build_scan_action( parser, gen, sym, act );
			scan_actions = p_str_append( scan_actions, act, TRUE );
		}

		scan_actions = p_str_append( scan_actions,
			p_tpl_insert( gen->scan_action_end, 
			GEN_WILD_PREFIX "symbol-number", p_int_to_str( sym->id ), TRUE,
					(uchar*)NULL ), TRUE );
	}

	/* Get the goal production */
	goalprod = (PROD*)( parser->goal->productions->pptr );
	
	for( file = xml_child( gen->xml, "file" );
			file; file = xml_next( file ) )
	{
		if( ( filename = (uchar*)xml_attr( file, "filename" ) ) )
			filename = p_tpl_insert( filename,
				GEN_WILD_PREFIX "basename", parser->p_basename, FALSE,
				GEN_WILD_PREFIX "prefix", parser->p_prefix, FALSE,
					(char*)NULL );
		
		/* Assembling all together - Warning, this is
			ONE single function call! */

		all = p_tpl_insert( xml_txt( file ),
		
			/* Lengths of names and Prologue/Epilogue codes */
			GEN_WILD_PREFIX "name" LEN_EXT,
				p_long_to_str( (long)p_strlen( parser->p_name ) ), TRUE,
			GEN_WILD_PREFIX "copyright" LEN_EXT,
				p_long_to_str( (long)p_strlen( parser->p_copyright ) ), TRUE,
			GEN_WILD_PREFIX "version" LEN_EXT,
				p_long_to_str( (long)p_strlen( parser->p_version ) ), TRUE,
			GEN_WILD_PREFIX "description" LEN_EXT,
				p_long_to_str( (long)p_strlen( parser->p_desc ) ), TRUE,
			GEN_WILD_PREFIX "prologue" LEN_EXT,
				p_long_to_str( (long)p_strlen( parser->p_header ) ), TRUE,
			GEN_WILD_PREFIX "epilogue" LEN_EXT,
				p_long_to_str( (long)p_strlen( parser->p_footer ) ), TRUE,
			GEN_WILD_PREFIX "pcb" LEN_EXT,
				p_long_to_str( (long)p_strlen( parser->p_pcb ) ), TRUE,
	
			/* Names and Prologue/Epilogue codes */
			GEN_WILD_PREFIX "name", parser->p_name, FALSE,
			GEN_WILD_PREFIX "copyright", parser->p_copyright, FALSE,
			GEN_WILD_PREFIX "version", parser->p_version, FALSE,
			GEN_WILD_PREFIX "description", parser->p_desc, FALSE,
			GEN_WILD_PREFIX "prologue", parser->p_header, FALSE,
			GEN_WILD_PREFIX "epilogue", parser->p_footer, FALSE,
			GEN_WILD_PREFIX "pcb", parser->p_pcb, FALSE,
	
			/* Limits and sizes, parse tables */
			GEN_WILD_PREFIX "number-of-symbols",
				p_int_to_str( list_count( parser->symbols ) ), TRUE,
			GEN_WILD_PREFIX "number-of-states",
				p_int_to_str( list_count( parser->lalr_states ) ), TRUE,
			GEN_WILD_PREFIX "number-of-productions",
				p_int_to_str( list_count( parser->productions ) ), TRUE,
			GEN_WILD_PREFIX "number-of-dfa-machines",
				p_int_to_str( list_count( parser->kw ) ), TRUE, 
			GEN_WILD_PREFIX "deepest-action-row",
				p_int_to_str( max_action ), TRUE,
			GEN_WILD_PREFIX "deepest-goto-row",
				p_int_to_str( max_goto ), TRUE,
			GEN_WILD_PREFIX "deepest-dfa-index-row",
				p_int_to_str( max_dfa_idx ), TRUE,
			GEN_WILD_PREFIX "deepest-dfa-accept-row",
				p_int_to_str( max_dfa_accept ), TRUE,
			GEN_WILD_PREFIX "size-of-dfa-characters",
				p_int_to_str( column ), TRUE, 
			GEN_WILD_PREFIX "number-of-keyword-invalid-suffixes",
				p_int_to_str( ccl_size( parser->p_invalid_suf ) ), TRUE,
			GEN_WILD_PREFIX "number-of-character-map",
				p_int_to_str( charmap_count ), TRUE,
			GEN_WILD_PREFIX "action-table", action_table, FALSE,
			GEN_WILD_PREFIX "goto-table", goto_table, FALSE,
			GEN_WILD_PREFIX "production-lengths", prod_rhs_count, FALSE,
			GEN_WILD_PREFIX "production-lhs", prod_lhs, FALSE,
			GEN_WILD_PREFIX "default-productions", def_prod, FALSE,
			GEN_WILD_PREFIX "character-map-symbols", char_map_sym, FALSE,
			GEN_WILD_PREFIX "character-map", char_map, FALSE,
			GEN_WILD_PREFIX "character-universe",
				p_int_to_str( parser->p_universe ), TRUE,
			GEN_WILD_PREFIX "whitespaces", whitespaces, FALSE,
			GEN_WILD_PREFIX "symbols", symbols, FALSE,
			GEN_WILD_PREFIX "productions", productions, FALSE,
			GEN_WILD_PREFIX "max-symbol-name-length",
				p_int_to_str( max_symbol_name ), TRUE,
			GEN_WILD_PREFIX "dfa-select", dfa_select, FALSE,
			GEN_WILD_PREFIX "dfa-index", dfa_idx, FALSE,
			GEN_WILD_PREFIX "dfa-char", dfa_char, FALSE,
			GEN_WILD_PREFIX "dfa-trans", dfa_trans, FALSE,
			GEN_WILD_PREFIX "dfa-accept", dfa_accept, FALSE,
			GEN_WILD_PREFIX "keyword-invalid-suffixes",
				kw_invalid_suffix, FALSE,
			GEN_WILD_PREFIX "value-type-definition", type_def, FALSE,
			GEN_WILD_PREFIX "actions", actions, FALSE,
			GEN_WILD_PREFIX "scan_actions", scan_actions, FALSE,
			GEN_WILD_PREFIX "top-value", top_value, FALSE,
			GEN_WILD_PREFIX "model", p_int_to_str( parser->p_model ), TRUE,
			GEN_WILD_PREFIX "error",
				( parser->error ? p_int_to_str( parser->error->id ) :
							p_int_to_str( -1 ) ), TRUE,
			GEN_WILD_PREFIX "eof",
				( parser->end_of_input ? 
					p_int_to_str( parser->end_of_input->id ) :
						p_int_to_str( -1 ) ), TRUE,
			GEN_WILD_PREFIX "goal-production",
				p_int_to_str( goalprod->id ), TRUE,
			GEN_WILD_PREFIX "goal",
				p_int_to_str( parser->goal->id ), TRUE,
	
			(uchar*)NULL
		);
		
		/* Now replace all prefixes */
		complete = p_tpl_insert( all,
					GEN_WILD_PREFIX "prefix",
						parser->p_prefix, FALSE,
					GEN_WILD_PREFIX "basename",
						parser->p_basename, FALSE,
					GEN_WILD_PREFIX "filename" LEN_EXT,
						p_long_to_str(
							(long)p_strlen( parser->filename ) ), TRUE,
					GEN_WILD_PREFIX "filename", parser->filename, FALSE,

					(uchar*)NULL );

		p_free( all );
		
		/* Open output file */
		if( filename )
		{
			if( !( stream = fopen( filename, "wt" ) ) )
			{
				p_error( ERR_OPEN_OUTPUT_FILE, ERRSTYLE_WARNING, filename );
				p_free( filename );
				filename = (char*)NULL;
			}
		}
		
		/* No 'else' here! */
		if( !filename )
			stream = stdout;

		fprintf( stream, "%s", complete );
		p_free( complete );
		
		if( filename )
		{
			fclose( stream );
			p_free( filename );
		}
	}
	
	MSG( "Freeing used memory" );

	/* Freeing generated content */
	p_free( action_table );
	p_free( goto_table );
	p_free( prod_rhs_count );
	p_free( prod_lhs );
	p_free( def_prod );
	p_free( char_map );
	p_free( char_map_sym );
	p_free( whitespaces );
	p_free( symbols );
	p_free( productions );
	p_free( dfa_select );
	p_free( dfa_idx );
	p_free( dfa_char );
	p_free( dfa_trans );
	p_free( dfa_accept );
	p_free( kw_invalid_suffix );
	p_free( type_def );
	p_free( actions );
	p_free( scan_actions );
	p_free( top_value );

	/* Freeing the generator's structure */
	p_free( gen->for_sequences );
	p_free( gen->do_sequences );
	xml_free( gen->xml );

	VOIDRET;
}

/* -FUNCTION--------------------------------------------------------------------
	Function:		p_escape_for_target()
	
	Author:			Jan Max Meyer
	
	Usage:			Escapes the input-string according to the parser template's
					escaping-sequence definitions. This function is used to
					print identifiers and character-class definitions to the
					target parser without getting trouble with the target
					language's escape characters (e.g. as in C - I love C!!!).
					
	Parameters:		GENERATOR*	g					Generator template structure			
					uchar*		str					Source string
					BOOLEAN		clear				If TRUE, str will be free'd,
													else not
	
	Returns:		uchar*							The final (escaped) string
  
	~~~ CHANGES & NOTES ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	Date:		Author:			Note:
----------------------------------------------------------------------------- */
uchar* p_escape_for_target( GENERATOR* g, uchar* str, BOOLEAN clear )
{
	int		i;
	uchar*	ret;
	uchar*	tmp;

	if( !( ret = p_strdup( str ) ) )
		OUT_OF_MEMORY;

	if( clear )
		p_free( str );

	for( i = 0; i < g->sequences_count; i++ )
	{
		if( !( tmp = p_tpl_insert( ret, g->for_sequences[ i ],
				g->do_sequences[ i ], FALSE, (uchar*)NULL ) ) )
			OUT_OF_MEMORY;

		p_free( ret );
		ret = tmp;
	}

	return ret;
}

/* -FUNCTION--------------------------------------------------------------------
	Function:		p_build_action()
	
	Author:			Jan Max Meyer
	
	Usage:			Constructs target language code for production reduction
					code blocks.
					
	Parameters:		PARSER*		parser				Parser information structure
					GENERATOR*	g					Generator template structure
					PROD*		p					Production
					uchar*		base				Code-base template for the
													reduction action
					BOOLEAN		def_code			Defines if the base-pointer
													is a default-code block or
													an individually coded one.
	
	Returns:		uchar*							Pointer to the generated
													code - must be freed by
													caller.
  
	~~~ CHANGES & NOTES ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	Date:		Author:			Note:
----------------------------------------------------------------------------- */
uchar* p_build_action( PARSER* parser, GENERATOR* g, PROD* p,
			uchar* base, BOOLEAN def_code )
{
	pregex			replacer;
	pregex_result*	result;
	int				result_cnt;
	int				i;
	int				off;
	uchar*			last;
	uchar*			ret		= (uchar*)NULL;
	uchar*			chk;
	uchar*			tmp;
	uchar*			att;
	LIST*			l;
	LIST*			m;
	LIST*			rhs			= p->rhs;
	LIST*			rhs_idents	= p->rhs_idents;
	BOOLEAN			on_error	= FALSE;
	SYMBOL*			sym;
	
	PROC( "p_build_action" );
	PARMS( "parser", "%p", parser );
	PARMS( "g", "%p", g );
	PARMS( "p", "%p", p );
	PARMS( "base", "%s", base );
	PARMS( "def_code", "%s", BOOLEAN_STR( def_code ) );
	
	/* Prepare regular expression engine */
	pregex_comp_init( &replacer, REGEX_MOD_GLOBAL );
	
	if( pregex_comp_compile( &replacer, "@[A-Za-z_][A-Za-z0-9_]*", 0 )
			!= ERR_OK )
		RETURN( (uchar*)NULL );

	if( pregex_comp_compile( &replacer, "@[0-9]+", 1 )
			!= ERR_OK )
		RETURN( (uchar*)NULL );

	if( pregex_comp_compile( &replacer, "@@", 2 )
			!= ERR_OK )
		RETURN( (uchar*)NULL );
		
	/* Run regular expression */
	if( ( result_cnt = pregex_comp_match( &replacer, base, &result ) ) < 0 )
	{
		MSG( "Error occured" );
		VARS( "result_cnt", "%d", result_cnt );
		RETURN( (uchar*)NULL );
	}
	else if( !result_cnt )
	{
		MSG( "Nothing to do at all" );
		RETURN( pstrdup( base ) );
	}
	
	VARS( "result_cnt", "%d", result_cnt );
	
	/* Free the regular expression facilities - we have everything we 
		need from here! */
	pregex_comp_free( &replacer );
	
	VARS( "p->sem_rhs", "%p", p->sem_rhs  );
	/* Ok, perform replacement operations */
	if( p->sem_rhs )
	{
		MSG( "Replacing semantic right-hand side" );
		rhs = p->sem_rhs;
		rhs_idents = p->sem_rhs_idents;
	}
	
	MSG( "Iterating trough result array" );	
	for( i = 0, last = base; i < result_cnt && !on_error; i++ )
	{
		VARS( "i", "%d", i );
		off = 0;

		if( last < result[i].begin )
		{
			if( !( ret = pstr_append_nchar(
					ret, last, result[i].begin - last ) ) )
				OUTOFMEM;
				
			VARS( "ret", "%s", ret );

			last = result[i].end;
		}
		
		VARS( "result[i].accept", "%d", result[i].accept );
		switch( result[i].accept )
		{
			case 0:
				MSG( "Identifier" );				
				for( l = rhs_idents, m = rhs, off = 1; l && m;
						l = list_next( l ), m = list_next( m ), off++ )
				{
					chk = (uchar*)list_access( l );
					VARS( "chk", "%s", chk ? chk : "(NULL)" );

					if( chk && !pstrncmp( chk, result[i].begin + 1,
									pstrlen( chk ) * sizeof( uchar ) ) )
					{
						break;
					}
				}
				
				if( !l )
					off = 0;
				
				VARS( "off", "%d", off );
				break;

			case 1:
				MSG( "Offset" );
				off = patoi( result[i].begin + 1 );
				break;

			case 2:
				MSG( "Left-hand side" );
				if( p->lhs->vtype && list_count( parser->vtypes ) > 1 )
					ret = pstr_append_str( ret,
							p_tpl_insert( g->action_lhs_union,
								GEN_WILD_PREFIX "attribute",
									p_tpl_insert( g->vstack_union_att,
										GEN_WILD_PREFIX "value-type-id",
											p_int_to_str( p->lhs->vtype->id ),
												TRUE,
										(uchar*)NULL ), TRUE,
								(uchar*)NULL ), TRUE );
				else
					ret = pstr_append_str( ret, g->action_lhs_single, FALSE );					
					
				VARS( "ret", "%s", ret );
				break;
				
			default:
				MSG( "Uncaught regular expression match!" );
				break;
		}
		
		VARS( "off", "%d", off );
		if( off > 0 )
		{
			tmp = (uchar*)NULL;

			MSG( "Handing offset" );
			sym = (SYMBOL*)list_getptr( rhs, off - 1 );

			if( !( sym->type == SYM_KW_TERMINAL ) )
			{				
				if( list_count( parser->vtypes ) > 1 )
				{
					if( sym->vtype )
					{
						att  = p_tpl_insert( g->vstack_union_att,
							GEN_WILD_PREFIX "value-type-id",
								p_int_to_str( sym->vtype->id ), TRUE,
							(uchar*)NULL );
					}
					else
					{
						p_error( ERR_NO_VALUE_TYPE, ERRSTYLE_FATAL,
								sym->name, p->id, result[i].len + 1,
									result[i].begin );
					
						att = (uchar*)NULL;
						on_error = TRUE;
					}

					tmp = p_tpl_insert( g->action_union,
						GEN_WILD_PREFIX "offset",
							p_int_to_str( list_count( rhs ) - off ), TRUE,
						GEN_WILD_PREFIX "attribute", att, TRUE,
						(uchar*)NULL );
				}
				else
					tmp = p_tpl_insert( g->action_single,
						GEN_WILD_PREFIX "offset",
							p_int_to_str( list_count( rhs ) - off ), TRUE,
						(uchar*)NULL );
			}
			else
			{
				if( !def_code )
				{
					p_error( ERR_NO_VALUE_TYPE, ERRSTYLE_FATAL,
							p_find_base_symbol( sym )->name,
								p->id, result[i].len + 1, result[i].begin );
				}

				on_error = TRUE;
			}
			
			if( tmp )
				ret = pstr_append_str( ret, tmp, TRUE );
		}
	}
		
	if( last && *last )
		ret = pstr_append_str( ret, last, FALSE );
	
	MSG( "Free result array" );
	pfree( result );
		
	VARS( "ret", "%s", ret );
	VARS( "on_error", "%s", BOOLEAN_STR( on_error ) );
		
	if( on_error && ret )
	{
		MSG( "Okay, on error, everything will be deleted!" );
		p_free( ret );
		ret = (uchar*)NULL;
	}

	RETURN( ret );
}

/* -FUNCTION--------------------------------------------------------------------
	Function:		p_build_scan_action()
	
	Author:			Jan Max Meyer
	
	Usage:			<usage>
					
	Parameters:		<type>		<identifier>		<description>
	
	Returns:		<type>							<description>
  
	~~~ CHANGES & NOTES ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	Date:		Author:			Note:
----------------------------------------------------------------------------- */
uchar* p_build_scan_action( PARSER* parser, GENERATOR* g, SYMBOL* s,
			uchar* base )
{
	pregex			replacer;
	pregex_result*	result;
	int				result_cnt;
	uchar*			ret			= (uchar*)NULL;
	uchar*			last;
	int				i;
	BOOLEAN			on_error	= FALSE;
	
	PROC( "p_build_scan_action" );
	PARMS( "parser", "%p", parser );
	PARMS( "g", "%p", g );
	PARMS( "s", "%p", s );
	PARMS( "base", "%s", base );
	
	/* Prepare regular expression engine */
	pregex_comp_init( &replacer, REGEX_MOD_GLOBAL | REGEX_MOD_NO_ANCHORS );
	
	if( pregex_comp_compile( &replacer, "@>", 0 )
			!= ERR_OK )
		RETURN( (uchar*)NULL );

	if( pregex_comp_compile( &replacer, "@<", 1 )
			!= ERR_OK )
		RETURN( (uchar*)NULL );

	if( pregex_comp_compile( &replacer, "@@", 2 )
			!= ERR_OK )
		RETURN( (uchar*)NULL );
		
	/* Run regular expression */
	if( ( result_cnt = pregex_comp_match( &replacer, base, &result ) ) < 0 )
	{
		MSG( "Error occured" );
		VARS( "result_cnt", "%d", result_cnt );
		RETURN( (uchar*)NULL );
	}
	else if( !result_cnt )
	{
		MSG( "Nothing to do at all" );
		RETURN( pstrdup( base ) );
	}
	
	VARS( "result_cnt", "%d", result_cnt );
	
	/* Free the regular expression facilities - we have everything we 
		need from here! */
	pregex_comp_free( &replacer );
	
	MSG( "Iterating trough result array" );	
	for( i = 0, last = base; i < result_cnt && !on_error; i++ )
	{
		VARS( "i", "%d", i );

		if( last < result[i].begin )
		{
			if( !( ret = pstr_append_nchar(
					ret, last, result[i].begin - last ) ) )
				OUTOFMEM;
				
			VARS( "ret", "%s", ret );
			last = result[i].end;
		}
		
		VARS( "result[i].accept", "%d", result[i].accept );
		switch( result[i].accept )
		{
			case 0:
				MSG( "@>" );
				ret = pstr_append_str( ret,
					g->scan_action_begin_offset, FALSE );
				break;

			case 1:
				MSG( "@<" );
				ret = pstr_append_str( ret,
					g->scan_action_end_offset, FALSE );

				break;

			case 2:
				MSG( "@@" );
				if( s->vtype && list_count( parser->vtypes ) > 1 )
					ret = pstr_append_str( ret,
							p_tpl_insert( g->scan_action_ret_union,
								GEN_WILD_PREFIX "attribute",
									p_tpl_insert( g->vstack_union_att,
										GEN_WILD_PREFIX "value-type-id",
											p_int_to_str( s->vtype->id ), TRUE,
										(uchar*)NULL ), TRUE,
								(uchar*)NULL ), TRUE );
				else
					ret = pstr_append_str( ret,
							g->scan_action_ret_single, FALSE );
				break;
				
			default:
				MSG( "Uncaught regular expression match!" );
				break;
		}
		
		VARS( "ret", "%s", ret );
	}
		
	if( last && *last )
		ret = pstr_append_str( ret, last, FALSE );
	
	MSG( "Free result array" );
	pfree( result );
		
	VARS( "ret", "%s", ret );
	RETURN( ret );
}

/* -FUNCTION--------------------------------------------------------------------
	Function:		p_mkproduction_str()
	
	Author:			Jan Max Meyer
	
	Usage:			Converts a production into a dynamic string.
					
	Parameters:		PROD*		p					Production pointer
	
	Returns:		uchar*							Generated string
  
	~~~ CHANGES & NOTES ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	Date:		Author:			Note:
----------------------------------------------------------------------------- */
uchar* p_mkproduction_str( PROD* p )
{
	uchar*		ret;
	uchar		wtf		[ 512 ];
	LIST*		l;
	SYMBOL*		sym;
	
	sprintf( wtf, "%s -> ", p->lhs->name );
	ret = p_strdup( wtf );
	
	for( l = p->rhs; l; l = l->next )
	{
		sym = (SYMBOL*)( l->pptr );
		
		switch( sym->type )
		{
			case SYM_CCL_TERMINAL:
				sprintf( wtf, "\'%s\'", sym->name );
				break;
			case SYM_KW_TERMINAL:
				sprintf( wtf, "\"%s\"", sym->name );
				break;
			case SYM_REGEX_TERMINAL:
				sprintf( wtf, "@%s", sym->name );
				break;
			case SYM_EXTERN_TERMINAL:
				sprintf( wtf, "*%s", sym->name );
				break;
			case SYM_ERROR_RESYNC:
				strcpy( wtf, P_ERROR_RESYNC );
				break;
				
			default:
				strcpy( wtf, sym->name );
				break;			
		}
		
		if( l->next )
			strcat( wtf, " " );
		
		ret = p_str_append( ret, wtf, FALSE );
	}
	
	return ret;
}

/* -FUNCTION--------------------------------------------------------------------
	Function:		p_load_generator()
	
	Author:			Jan Max Meyer
	
	Usage:			Loads a XML-defined code generator into an adequate
					GENERATOR structure. Pointers are only set to the
					values mapped to the XML-structure, so no memory is
					wasted.
					
	Parameters:		GENERATOR*		g				The target generator
					uchar*			genfile			Path to generator file
	
	Returns:		BOOLEAN			TRUE			on success
									FALSE			on error.
  
	~~~ CHANGES & NOTES ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	Date:		Author:			Note:
----------------------------------------------------------------------------- */
BOOLEAN p_load_generator( GENERATOR* g, uchar* genfile )
{
	XML_T	tmp;
	uchar*	att_for;
	uchar*	att_do;
	int		i;

#define GET_XML_DEF( source, target, tagname ) \
	if( xml_child( (source), (tagname) ) ) \
		(target) = (uchar*)( xml_txt( xml_child( (source), (tagname) ) ) ); \
	else \
		p_error( ERR_TAG_NOT_FOUND, ERRSTYLE_WARNING, (tagname), genfile );

#define GET_XML_TAB_1D( target, tagname ) \
	if( ( tmp = xml_child( g->xml, (tagname) ) ) ) \
	{ \
		GET_XML_DEF( tmp, (target).col, "col" ) \
		GET_XML_DEF( tmp, (target).col_sep, "col_sep" ) \
	} \
	else \
		p_error( ERR_TAG_NOT_FOUND, ERRSTYLE_WARNING, (tagname), genfile );

#define GET_XML_BOOLTAB_1D( target, tagname ) \
	if( ( tmp = xml_child( g->xml, (tagname) ) ) ) \
	{ \
		GET_XML_DEF( tmp, (target).col_true, "col_true" ) \
		GET_XML_DEF( tmp, (target).col_false, "col_false" ) \
		GET_XML_DEF( tmp, (target).col_sep, "col_sep" ) \
	} \
	else \
		p_error( ERR_TAG_NOT_FOUND, ERRSTYLE_WARNING, (tagname), genfile );

#define GET_XML_TAB_2D( target, tagname ) \
	if( ( tmp = xml_child( g->xml, (tagname) ) ) ) \
	{ \
		GET_XML_DEF( tmp, (target).row_start, "row_start" ) \
		GET_XML_DEF( tmp, (target).row_end, "row_end" ) \
		GET_XML_DEF( tmp, (target).row_sep, "row_sep" ) \
		GET_XML_DEF( tmp, (target).col, "col" ) \
		GET_XML_DEF( tmp, (target).col_sep, "col_sep" ) \
	} \
	else \
		p_error( ERR_TAG_NOT_FOUND, ERRSTYLE_WARNING, (tagname), genfile ); 

	if( !( g->xml = xml_parse_file( genfile ) ) )
	{
		p_error( ERR_NO_GENERATOR_FILE, ERRSTYLE_FATAL, genfile );
		return FALSE;
	}

	if( *xml_error( g->xml ) )
	{
		p_error( ERR_XML_ERROR, ERRSTYLE_FATAL, genfile, xml_error( g->xml ) );
		return FALSE;
	}

	/* GET_XML_DEF( g->xml, g->driver, "driver" ); */
	GET_XML_DEF( g->xml, g->vstack_def_type, "vstack_def_type" );
	GET_XML_DEF( g->xml, g->vstack_term_type, "vstack_term_type" );

	GET_XML_DEF( g->xml, g->action_start, "action_start" );
	GET_XML_DEF( g->xml, g->action_end, "action_end" );
	GET_XML_DEF( g->xml, g->action_single, "action_single" );
	GET_XML_DEF( g->xml, g->action_union, "action_union" );
	GET_XML_DEF( g->xml, g->action_lhs_single, "action_lhs_single" );
	GET_XML_DEF( g->xml, g->action_lhs_union, "action_lhs_union" );

	GET_XML_DEF( g->xml, g->scan_action_start, "scan_action_start" );
	GET_XML_DEF( g->xml, g->scan_action_end, "scan_action_end" );
	GET_XML_DEF( g->xml, g->scan_action_begin_offset, "scan_action_begin_offset" );
	GET_XML_DEF( g->xml, g->scan_action_end_offset, "scan_action_end_offset" );
	GET_XML_DEF( g->xml, g->scan_action_ret_single, "scan_action_ret_single" );
	GET_XML_DEF( g->xml, g->scan_action_ret_union, "scan_action_ret_union" );

	GET_XML_DEF( g->xml, g->vstack_single, "vstack_single" );
	GET_XML_DEF( g->xml, g->vstack_union_start, "vstack_union_start" );
	GET_XML_DEF( g->xml, g->vstack_union_end, "vstack_union_end" );
	GET_XML_DEF( g->xml, g->vstack_union_def, "vstack_union_def" );
	GET_XML_DEF( g->xml, g->vstack_union_att, "vstack_union_att" );

	GET_XML_TAB_1D( g->prodlen, "prodlen" )
	GET_XML_TAB_1D( g->prodlhs, "prodlhs" )
	GET_XML_TAB_1D( g->defprod, "defprod" )
	GET_XML_TAB_1D( g->charmap, "charmap" )
	GET_XML_TAB_1D( g->charmap_sym, "charmap_sym" )
	GET_XML_TAB_1D( g->dfa_select, "dfa_select" )
	GET_XML_TAB_1D( g->dfa_char, "dfa_char" )
	GET_XML_TAB_1D( g->dfa_trans, "dfa_trans" )
	GET_XML_TAB_1D( g->kw_invalid_suffix, "kw_invalid_suffix" )
	GET_XML_BOOLTAB_1D( g->whitespace, "whitespace" )

	GET_XML_TAB_2D( g->acttab, "acttab" )
	GET_XML_TAB_2D( g->gotab, "gotab" )
	GET_XML_TAB_2D( g->dfa_idx, "dfa_idx" )
	GET_XML_TAB_2D( g->dfa_accept, "dfa_accept" )

	GET_XML_TAB_1D( g->symbols, "symbols" )
	GET_XML_TAB_1D( g->productions, "productions" )
	
	GET_XML_DEF( g->xml, g->code_localization, "code_localization" );

	/* Escape sequence definitions */
	for( tmp = xml_child( g->xml, "escape-sequence" ); tmp; tmp = xml_next( tmp ) )
	{
		att_for = (uchar*)xml_attr( tmp, "for" );
		att_do = (uchar*)xml_attr( tmp, "do" );
		
		if( att_for && att_do )
		{
			for( i = 0; i < g->sequences_count; i++ )
			{
				if( !strcmp( g->for_sequences[ i ], att_for ) )
				{
					p_error( ERR_DUPLICATE_ESCAPE_SEQ, ERRSTYLE_WARNING, att_for, genfile );
					break;
				}
			}

			if( i < g->sequences_count )
				continue;

			g->for_sequences = (uchar**)p_realloc( (uchar**)g->for_sequences,
					( g->sequences_count + 1 ) * sizeof( uchar* ) );
			g->do_sequences = (uchar**)p_realloc( (uchar**)g->do_sequences,
					( g->sequences_count + 1 ) * sizeof( uchar* ) );

			if( !( g->for_sequences && g->do_sequences ) )
				OUT_OF_MEMORY;

			g->for_sequences[ g->sequences_count ] = (uchar*)( att_for );
			g->do_sequences[ g->sequences_count ] = (uchar*)( att_do );

			if( !( g->for_sequences[ g->sequences_count ]
				&& g->do_sequences[ g->sequences_count ] ) )
				OUT_OF_MEMORY;

			g->sequences_count++;
		}
		else
		{
			if( !att_for )
				p_error( ERR_XML_INCOMPLETE, ERRSTYLE_FATAL, genfile, xml_name( tmp ), "for" );
			if( !att_do )
				p_error( ERR_XML_INCOMPLETE, ERRSTYLE_FATAL, genfile, xml_name( tmp ), "do" );
		}
	}

	return TRUE;
}
