/* Parser Control Block */
typedef struct
{
    /* Is this PCB allocated by parser? */
    char				is_internal;

    /* Stack */
    @@prefix_tok*		stack;
    @@prefix_tok*		tos;

    /* Stack size */
    unsigned int		stacksize;

    /* Values */
    @@prefix_vtype		ret;
    @@prefix_vtype		test;

    /* State */
    int					act;
    int					idx;
    int					lhs;

    /* Lookahead */
    int					sym;
    int					old_sym;
    unsigned int		len;

    /* Input buffering */
    UNICC_SCHAR*		lexem;
    UNICC_CHAR*			buf;
    UNICC_CHAR*			bufend;
    UNICC_CHAR*			bufsize;

    /* Lexical analysis */
    UNICC_CHAR			next;
    UNICC_CHAR			eof;
    UNICC_BOOLEAN		is_eof;

    /* Error handling */
    int					error_delay;
    int					error_count;

    unsigned int		line;
    unsigned int		column;

    /* Abstract Syntax Tree */
    @@prefix_ast*		ast;

    /* User-defined components */
    @@pcb

} @@prefix_pcb;
