/* Virtual production construction functions */

#include "unicc.h"

/** Creates a positive closure for a symbol.

//parser// is the Parser information structure.
//base// is the base symbol.

Returns a SYMBOL* sointer to the symbol representing the closure nonterminal. */
SYMBOL* positive_closure( PARSER* parser, SYMBOL* base )
{
    char*	deriv_str;
    PROD*	p;
    SYMBOL*	s			= (SYMBOL*)NULL;

    /*
    30.09.2010	Jan Max Meyer:
    Inherit defined_at information
    */

    if( base )
    {
        deriv_str = derive_name( base->name, P_POSITIVE_CLOSURE );

        if( !( s = get_symbol( parser, deriv_str,
                        SYM_NON_TERMINAL, FALSE ) ) )
        {
            s = get_symbol( parser, deriv_str,
                    SYM_NON_TERMINAL, TRUE );
            s->generated = TRUE;
            s->used = TRUE;
            s->defined = TRUE;
            s->derived_from = base;
            s->line = base->line;

            p = create_production( parser, s );
            append_to_production( p, s, (char*)NULL );
            append_to_production( p, base, (char*)NULL );

            p = create_production( parser, s );
            append_to_production( p, base, (char*)NULL );
        }

        pfree( deriv_str );
    }

    return s;
}

/** Creates a kleene closure for a symbol.

//parser// is the parser information structure.
//base// is the base symbol.

Returns a SYMBOL* Pointer to symbol representing the closure nonterminal. */
SYMBOL* kleene_closure( PARSER* parser, SYMBOL* base )
{
    char*	deriv_str;
    PROD*	p;
    SYMBOL*	s			= (SYMBOL*)NULL;
    SYMBOL*	pos_s		= (SYMBOL*)NULL;

    /*
    14.05.2008	Jan Max Meyer
    Modified rework. Instead of

        s* -> s* base | ;

    this will now create

        s+ -> s+ base | base;
        s* -> s+ | ;


    30.09.2010	Jan Max Meyer
    Inherit defined_at information
    */

    if( base )
    {
        pos_s = positive_closure( parser, base );
        if( !pos_s )
            return s;

        deriv_str = derive_name( base->name, P_KLEENE_CLOSURE );

        if( !( s = get_symbol( parser, deriv_str,
                    SYM_NON_TERMINAL, FALSE ) ) )
        {
            s = get_symbol( parser, deriv_str,
                    SYM_NON_TERMINAL, TRUE );
            s->generated = TRUE;
            s->used = TRUE;
            s->defined = TRUE;
            s->derived_from = base;
            s->line = base->line;

            p = create_production( parser, s );
            /*append_to_production( p, s, (char*)NULL );
            append_to_production( p, base, (char*)NULL );*/
            append_to_production( p, pos_s, (char*)NULL );

            p = create_production( parser, s );
        }

        pfree( deriv_str );
    }

    return s;
}

/** Creates an optional closure for a symbol.

//parser// is the parser information structure.
//base// is the base symbol.

Returns a SYMBOL* Pointer to symbol representing the closure nonterminal.
*/
SYMBOL* optional_closure( PARSER* parser, SYMBOL* base )
{
    char*	deriv_str;
    PROD*	p;
    SYMBOL*	s			= (SYMBOL*)NULL;

    /*
    30.09.2010	Jan Max Meyer:
    Inherit defined_at information
    */

    if( base )
    {
        deriv_str = derive_name( base->name, P_OPTIONAL_CLOSURE );

        if( !(s = get_symbol( parser, deriv_str,
                    SYM_NON_TERMINAL, FALSE ) ) )
        {
            s = get_symbol( parser, deriv_str,
                    SYM_NON_TERMINAL, TRUE );
            s->generated = TRUE;
            s->used = TRUE;
            s->defined = TRUE;
            s->derived_from = base;
            s->line = base->line;

            p = create_production( parser, s );
            append_to_production( p, base, (char*)NULL );

            p = create_production( parser, s );
        }

        pfree( deriv_str );
    }

    return s;
}
