/* Grammar integrity checking functions */

#include "unicc.h"

/** Checks for undefined but called, and/or defined but uncalled symbols.

//parser// is the pointer to the parser information structure.

Returns a TRUE if undefined or unused symbols are found, else FALSE. */
BOOLEAN find_undef_or_unused( PARSER* parser )
{
    plistel*	e;
    SYMBOL*		sym;
    BOOLEAN		ret		= FALSE;

    plist_for( parser->symbols, e )
    {
        sym = (SYMBOL*)plist_access( e );

        if( sym->generated == FALSE && sym->defined == FALSE )
        {
            print_error( parser, (sym->type == SYM_NON_TERMINAL ) ?
                ERR_UNDEFINED_NONTERM : ERR_UNDEFINED_TERM,
                    ERRSTYLE_FATAL | ERRSTYLE_FILEINFO,
                        parser->filename, sym->line, sym->name );

            ret = TRUE;
        }

        if( sym->generated == FALSE && sym->used == FALSE )
        {
            print_error( parser, (sym->type == SYM_NON_TERMINAL ) ?
                ERR_UNUSED_NONTERM : ERR_UNUSED_TERM,
                    ERRSTYLE_WARNING | ERRSTYLE_FILEINFO,
                        parser->filename, sym->line, sym->name );
        }
    }

    return ret;
}

/** This function is required to test if a given character class will be
consumed by a NFA machine state.

//nfa// is the NFA to work on.

//res// defines the closure set of NFA states required for the next transition.
(plist*)NULL causes the NFA to be startet from initial state.

//accept// is the return pointer for the accepting ID of the NFA.

//check_with// is the character-class to test transitions on.

Returns the result of the transition on the given character class,
0 if there is no transition. */
static int nfa_transition_on_ccl( pregex_nfa* nfa, plist* res,
                                    unsigned int* accept, pccl* check_with )
{
    int				i;
    wchar_t			beg;
    wchar_t			end;
    wchar_t			ch;
    plist*			tr;
    plist*			ret_res;
    plistel*		e;

    if( !plist_count( res ) )
        plist_push( res, plist_access( plist_first( nfa->states ) ) );

    pregex_nfa_epsilon_closure( nfa, res, accept, (int*)NULL );
    ret_res = plist_create( 0, PLIST_MOD_PTR );

    for( i = 0; pccl_get( &beg, &end, check_with, i ); i++ )
    {
        /*
            This may be the source for large run time latency.
            The way chosen by pregex/dfa.c should be used if necessary
            somewhere in future.
        */
        for( ch = beg; ch <= end; ch++ )
        {
            tr = plist_dup( res );

            if( pregex_nfa_move( nfa, tr, ch, ch ) > 0 )
                plist_union( ret_res, tr );

            tr = plist_free( tr );
        }
    }

    plist_erase( res );

    plist_for( ret_res, e )
        plist_push( res, plist_access( e ) );

    plist_free( ret_res );

    return plist_count( res );
}


/** Tries to parse along an NFA state machine using character class terminals
from a given LALR(1) state.

It should prove if the grammar is capable to match the string the regular
expression matches. It is used for the regex anomaly detection.

//parser// is the pointer to the parser information structure.

//nfa// is the NFA to be recognized.

//start// is the start state; This is the state where parsing is immediatelly
started.

Returns a TRUE if the parse succeeded (when str was completely absorbed),
FALSE else.
*/
static BOOLEAN check_nfa_matches_parser(
    PARSER* parser, pregex_nfa* nfa, plist* start_res, int start )
{
    int				stack[ 1024 ];
    int				act;
    unsigned int	accept;
    int				idx;
    int				tos 			= 0;
    PROD*			rprod;
    STATE*			st;
    TABCOL*			col;
    plist*			res;
    plistel*		e;
    BOOLEAN			ret				= TRUE;
    LIST*			l;

    /*
        06.03.2008	Jan Max Meyer
        Changes on the algorithm because of new SHIFT_REDUCE-transitions.
        They are either reduces.

        30.01.2011	Jan Max Meyer
        Renamed to check_nfa_matches_parser(), the function now tries to parse
        along an NFA state machine which must not have its origin in a keyword
        terminal.

        16.01.2014	Jan Max Meyer
        Fixed sources to run with libphorward v0.18 and newer.

        TODO:
        This part of UniCC is programmed very rudely, and should be changed
        somewhere in the future. But for now, it does its job.

        Nelson: "Haaahaaaa" ... never! I think... ;)
    */
    if( ( st = (STATE*)parray_get( parser->states, start ) ) )
        stack[ tos++ ] = st->derived_from;
    else
        stack[ tos++ ] = start;

    stack[ tos ] = start;

    res = plist_create( 0, PLIST_MOD_PTR | PLIST_MOD_RECYCLE );

    do
    {
        act = 0;
        idx = 0;
        st = (STATE*)parray_get( parser->states, stack[ tos ] );

        for( l = st->actions; l; l = l->next )
        {
            col = (TABCOL*)l->pptr;
            if( col->symbol->type == SYM_CCL_TERMINAL )
            {
                plist_erase( res );
                plist_for( start_res, e )
                    plist_push( res, plist_access( e ) );


                if( nfa_transition_on_ccl( nfa, res,
                        &accept, col->symbol->ccl ) > 0 )
                {
                    act = col->action;
                    idx = col->index;
                }

                if( act || accept )
                    break;
            }
        }
        /*
        fprintf( stderr, "state = %d, act = %d idx = %d accept = %d res = %p\n",
            st->state_id, act, idx, accept, res );
        */

        plist_erase( start_res );
        plist_for( res, e )
            plist_push( start_res, plist_access( e ) );

        if( accept )
            break;

        /* Error */
        if( act == ERROR )
        {
            ret = FALSE;
            break;
        }

        /* Shift */
        if( act & SHIFT )
            stack[ ++tos ] = idx;

        /* Reduce */
        while( act & REDUCE )
        {
            rprod = (PROD*)plist_access(
                        plist_get( parser->productions, idx ) );
            /*
            fprintf( stderr, "tos = %d, reducing production %d, %d\n",
                tos, idx, list_count( rprod->rhs ) );
            dump_production( stderr, rprod, FALSE, FALSE );
            */

            tos -= plist_count( rprod->rhs );

            /*
            fprintf( stderr, "tos %d\n", tos );
            */
            tos++;

            st = (STATE*)parray_get( parser->states, stack[ tos - 1 ] );

            for( l = st->gotos; l; l = l->next )
            {
                col = (TABCOL*)l->pptr;

                if( col->symbol == rprod->lhs )
                {
                    act = col->action;
                    stack[ tos ] = idx = col->index;
                    break;
                }
            }

            if( stack[ tos ] == -1 )
            {
                ret = FALSE;
                break;
            }
        }
    }
    while( ret && !accept );

    plist_free( res );
    return ret;
}


/** Checks for regex anomalies in the resulting parse tables.

Such regex anomalies occur, when the parser can reduce both by a regular
expression based terminal and by a character, so it decides for the regular
expression.

Under some circumstances, the token to be shifted is NOT the regex it was
reduced for, and the parse fails.

A demonstation grammar for a keyword anomaly is the following:

    start$ -> a;
    a -> b "PRINT";
    b -> c | '[' b ']';
    c -> 'A-Z' | c 'A-Z';

At the input "[HALLOPRINT]", which is valid, the parser will fail after
successfully parsing "[HALLO", expecting a "]".

//parser// is the pointer to the parser information structure.

Returns TRUE if regex anomalies where found, FALSE otherwise. */
BOOLEAN check_regex_anomalies( PARSER* parser )
{
    STATE*			st;
    LIST*			m;
    LIST*			n;
    plistel*		e;
    plistel*		f;
    PROD*			p;
    SYMBOL*			lhs;
    SYMBOL*			sym;
    TABCOL*			col;
    TABCOL*			ccol;
    int				cnt;
    BOOLEAN			found;

    plist*			res;
    pregex_nfa*		nfa;
    unsigned int	accept;

    /*
    06.03.2008	Jan Max Meyer
    Changes on the algorithm because of new SHIFT_REDUCE-transitions.
    They are either reduces.

    12.06.2010	Jan Max Meyer
    Changed to new regular expression library functions, which can handle
    entire sets of characters from 0x0 - 0xFFFF.

    31.01.2011	Jan Max Meyer
    Renamed the function to check_regex_anomalies() because not only keywords
    are tested now, also entire regular expressions.

    29.11.2011	Jan Max Meyer
    Changed to new regular expression handling with the pregex_ptn-structure.
    */

    res = plist_create( 0, PLIST_MOD_PTR | PLIST_MOD_RECYCLE );

    /*
        For every keyword, try to find a character class beginning with the
        same character as the keyword. Then, try to recognize the keyword
        beginning from the state that forms the keyword, by running the
        parser on its existing tables.
    */
    parray_for( parser->states, st )
    {
        /* First of all, count all possible reduces in the current state. */
        for( m = st->actions, cnt = 0; m; m = m->next )
        {
            col = (TABCOL*)m->pptr;
            if( col->action & REDUCE )
                cnt++;
        }

        for( m = st->actions; m; m = m->next )
        {
            col = (TABCOL*)m->pptr;

            /* Regular expression to be reduced? */
            if( col->symbol->type == SYM_REGEX_TERMINAL
                    && col->action & REDUCE )
            {
                /*
                    Table columns not derived from the kernel set
                    of the state are ignored
                */
                if( col->derived_from && list_find( st->epsilon,
                        col->derived_from ) >= 0 )
                    continue;

                /*
                    Generate NFA from pattern
                */
                nfa = pregex_nfa_create();

                col->symbol->ptn->accept = col->symbol->id + 1;

                pregex_ptn_to_nfa( nfa, col->symbol->ptn );

                /*
                    check_nfa_matches_parser() can either be called here;
                    But to be sure, we have a try if there are shifts on
                    the same character, and then we try to parse the using
                    the existing parse tables. This will even be more
                    faster, I think.
                */
                for( n = st->actions; n; n = n->next )
                {
                    ccol = (TABCOL*)n->pptr;

                    /* Character class to be shifted? */
                    if( ccol->symbol->type == SYM_CCL_TERMINAL
                            && ccol->action & SHIFT )
                    {
                        plist_erase( res );

                        /*
                        fprintf( stderr, "col = >%s< ccol = >%s<\n",
                            col->symbol->name,
                                pccl_to_str( ccol->symbol->ccl, TRUE ) );
                        */

                        /*
                            If this is a match with the grammar and the keyword,
                            a keyword anomaly exists between the shift by a
                            character and the reduce by a keyword which can be
                            derived from the build-up of the characters of the
                            keyword. This is not the problem if there is only
                            one reduce, but if there are more, output a warning!
                        */
                        if( nfa_transition_on_ccl(
                                    nfa, res, &accept,
                                        ccol->symbol->ccl ) )
                        {
                            /*
                            printf( "state %d\n", st->state_id );
                            dump_item_set( stderr, (char*)NULL, st->kernel );
                            dump_item_set( stderr, (char*)NULL, st->epsilon );
                            getchar();
                            */

                            if( check_nfa_matches_parser( parser, nfa, res,
                                    st->state_id ) && cnt > 1 )
                            {
                                /*
                                    At this point, we have a candidate for a
                                    regex anomaly.. now we check out all
                                    positions where the left-hand side of the
                                    reduced production appears in...
                                    if there is no keyword to shift in the
                                    FIRST-sets of following symbols, report
                                    this anomaly!
                                */
                                p = (PROD*)plist_access(
                                            plist_get( parser->productions,
                                                            col->index ) );
                                lhs = p->lhs;

                                /* Go trough all productions */
                                found = FALSE;

                                plist_for( parser->productions, e )
                                {
                                    p = (PROD*)plist_access( e );

                                    plist_for( p->rhs, f )
                                    {
                                        sym = (SYMBOL*)plist_access( f );

                                        if( sym == lhs && plist_next( f ) )
                                        {
                                            do
                                            {
                                                f = plist_next( f );
                                                if( !f )
                                                    break;

                                                sym = (SYMBOL*)
                                                        plist_access( f );
                                                /*
                                                fprintf( stderr, "sym = " );
                                                print_symbol( stderr, sym );
                                                fprintf( stderr, " %d %d\n",
                                                    list_find( sym->first,
                                                        col->symbol ),
                                                            sym->nullable );
                                                */
                                                if( !plist_get_by_ptr(
                                                        sym->first,
                                                        col->symbol )
                                                    && !sym->nullable )
                                                {
                                                    print_error( parser,
                                                        ERR_KEYWORD_ANOMALY,
                                                        ERRSTYLE_WARNING |
                                                        ERRSTYLE_STATEINFO,
                                                        st, ccol->symbol->name,
                                                            col->symbol->name );

                                                    found = TRUE;
                                                    break;
                                                }
                                            }
                                            while( sym && sym->nullable );
                                        }

                                        if( found )
                                            break;
                                    }

                                    if( found )
                                        break;
                                }
                            }
                        }
                    }
                }

                nfa = pregex_nfa_free( nfa );
            }
        }
    } /* This is stupid... */

    plist_free( res );

    return FALSE;
}


/** Checks for productions resulting in circular definitions, empty recursions
or with wrong FIRST-sets.

//parser// is the pointer to the parser information structure.

Returns TRUE In case if stupid productions where detected,
FALSE if all is fine :D.
*/
BOOLEAN check_stupid_productions( PARSER* parser )
{
    plistel*	e;
    plistel*	f;
    PROD*		p;
    SYMBOL*		sym;
    BOOLEAN		stupid		= FALSE;
    BOOLEAN		possible	= FALSE;
    plist*		first_check	= (plist*)NULL;

    plist_for( parser->productions, e )
    {
        p = (PROD*)plist_access( e );

        if( plist_count( p->rhs ) == 1
                && plist_access( plist_first( p->rhs ) ) == p->lhs )
        {
            print_error( parser, ERR_CIRCULAR_DEFINITION,
                ERRSTYLE_WARNING | ERRSTYLE_PRODUCTION | ERRSTYLE_FILEINFO,
                    parser->filename, p->line, p );
            stupid = TRUE;
        }
        else if( p->lhs->nullable )
        {
            possible = FALSE;
            plist_for( p->rhs, f )
            {
                sym = (SYMBOL*)plist_access( f );

                if( !( sym->nullable ) )
                {
                    possible = FALSE;
                    break;
                }
                else if( sym == p->lhs )
                    possible = TRUE;
            }

            if( possible )
            {
                print_error( parser, ERR_EMPTY_RECURSION,
                    ERRSTYLE_WARNING | ERRSTYLE_FILEINFO | ERRSTYLE_PRODUCTION,
                        parser->filename, p->line, p );
                stupid = TRUE;
            }
        }

        /* Get all FIRST-sets of the right-hand side; If there are none,
            this can't be possible */
        if( plist_count( p->rhs ) > 0 )
        {
            if( !first_check )
                first_check = plist_create( 0,
                                    PLIST_MOD_PTR | PLIST_MOD_RECYCLE );
            else
                plist_erase( first_check );

            seek_rhs_first( first_check, plist_first( p->rhs ) );

            if( plist_count( first_check ) == 0 )
            {
                print_error( parser, ERR_USELESS_RULE,
                    ERRSTYLE_WARNING | ERRSTYLE_PRODUCTION | ERRSTYLE_FILEINFO,
                        parser->filename, p->line, p );
            }
        }
    }

    plist_free( first_check );

    return stupid;
}
