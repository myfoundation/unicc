/* Typedef for symbol information table */
typedef struct
{
    char*			name;
    char*			emit;
    short			type;
    UNICC_BOOLEAN	lexem;
    UNICC_BOOLEAN	whitespace;
    UNICC_BOOLEAN	greedy;
} @@prefix_syminfo;
