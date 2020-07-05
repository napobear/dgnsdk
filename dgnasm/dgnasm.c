#include "dgnasm.h"

unsigned int flags; // Store misc booleans
unsigned int curfno; // Current file number
unsigned int entrypos; // Starting address offset within the text segment
unsigned int stksize; // Additional stack size

struct asmsym * symtbl; // Symbol table
unsigned int sympos = ASM_SIZE; // Number of symbols in the table

// Output an octal number
void octwrite( int nfd, unsigned int val )
{
    write( nfd, "0", 1 );

    if ( !val ) return;

    char tmpbuf[6];
    int tmppos = 6;

    while ( val )
    {
        tmpbuf[--tmppos] = (val & 7) + '0';
        val >>= 3;
    }

    write( nfd, tmpbuf + tmppos, 6 - tmppos );
}

void symwrite( int nfd, struct asmsym * cursym )
{
    int k = 0;
    while ( cursym->name[k++] );
    write( nfd, "NAME: ", 6 );
    write( nfd, cursym->name, k );

    write( nfd, " | ", 3 );
    octwrite( nfd, cursym->len );

    write( nfd, "\r\n", 2 );


    write( nfd, "TYPE: ", 6);
    octwrite( nfd, cursym->type );
    write( nfd, "\r\n", 2 );

    write( nfd, "VAL: ", 5 );
    octwrite( nfd, cursym->val );
    write( nfd, "\r\n", 2 );
}

#include "help.c"
#include "segments.c"
#include "tokenizer.c"
#include "assembler.c"

void asmfail( char * msg )
{
    int i = 0;

    if ( fp )
    {
        unsigned int tmppos;
        char tmpchr;

        // Output current file
        while ( fp[i++] );
        write( 2, fp, i );
        write( 2, ":", 1 );

        if ( p )
        {
            // Output current line in octal
            octwrite( 2, curline );
            write( 2, ":", 1 );

            // Output current position in octal
            octwrite( 2, pp - lp );
            write( 2, ":", 1 );
        }

        // Close current file (good practice)
        close( fd );
    }

    // Output error message
    i = 0;
    while ( msg[i++] );
    write( 2, msg, i );
    write( 2, "\r\n", 2 );

    // Output current line
    i = 0;
    while ( lp[i++] );
    write( 2, lp, i );
//    write( 2, "\r\n", 2 );

/*    // Output curpos indicator
    i = 0;
    while ( i < pp - lp )
    {
        write( 2, lp[i++] == '\t' ? "\t" : " ", 1 );
    }

    write( 2, "^\r\n", 3 );
*/
    // Output curpos indicator
    i = 0;
    while ( i < p - lp )
    {
        write( 2, pp[i++] == '\t' ? "\t" : " ", 1 );
    }

    write( 2, "^\r\n", 3 );

    exit(1); // Quit program
}

int main( int argc, char ** argv )
{
    int i;

    // Drop first argument (program name)
    char * progname = *argv;
    argc--; argv++;

    // Set all flags
    while ( argc && **argv == '-' )
    {
        (*argv)++;

        // All symbols are global
        if      ( **argv == 'g' ) flags |= FLG_GLOB;
        // Stack size follows
        else if ( **argv == 't' )
        {
            argc--; argv++;
            while ( **argv >= '0' && **argv <= '9' ) stksize = stksize * 10 + *(*argv)++ - '0';

            if ( stksize > 32 ) asmfail( "Number of stack pages specified exceeds maximum of 32" );
        }
        // Output mode
        else if ( **argv == 'm' )
        {
            (*argv)++;
            if      ( **argv == 'h' ) flags |= FLG_SMH;
            else if ( **argv == 'a' ) flags |= FLG_SMHA;
            else if ( **argv == 'v' ) flags |= FLG_TERM;
        }

        argc--; argv++;
    }

    if ( !argc ) showhelp( progname );

    write( 1, " *** Loading symbols ***\r\n", 26 );

    // *** Load Assembler Defined Symbols ***
    fd = open( "symbols.dat", 0 );
    if ( fd < 0 ) asmfail( "failed to open symbols.dat" );

    symtbl = (struct asmsym *) sbrk( ASM_SIZE * sizeof(struct asmsym) );
    if ( symtbl == SBRKFAIL ) { close( fd ); asmfail( "failed to allocate space for assembler defiend symbols" ); }

    i = read( fd, symtbl, ASM_SIZE * sizeof(struct asmsym) );
    close( fd );

    if ( i < ASM_SIZE * sizeof(struct asmsym) ) asmfail( "couldn't load all symbols, symbols.dat might be out of date" );

    write( 1, " *** Starting first pass ***\r\n", 30 );

    // *** Run first pass for each file ***
    while ( curfno < argc )
    {
        // Output the current file
        write( 1, "Labeling file: ", 15 );

        i = 0;
        while ( argv[curfno][i++] );
        write( 1, argv[curfno], i );
        write( 1, "\r\n", 2 );

        // Add file seperator symbol
        struct asmsym * sepsym = symtbl + sympos++;

        *sepsym->name = 0;
        sepsym->len = i;
        sepsym->type = SYM_FILE;
        sepsym->val = text.dataPos;

        assemble( argv[curfno] );

        curfno++;
    }

    // Make sure there are no undefined local labels
    i = ASM_SIZE;
    while ( i < sympos )
    {
        if ( (symtbl[i].type & SYM_MASK) == SYM_DEF && ~symtbl[i].type & SYM_GLOB )
        {
            symwrite( 2, symtbl + i );
            asmfail("found undefined local label");
        }
        i++;
    }

//    write( 1, "Allocating required memory for segment data\r\n", 45 );

    // *** Reset for next pass ***
    // Record how much data space is needed and reset
    text.dataSize = text.dataPos; text.dataPos = 0; text.rlocPos = 0;
    data.dataSize = data.dataPos; data.dataPos = 0; data.rlocPos = 0;
     bss.dataSize =  bss.dataPos;  bss.dataPos = 0;
    zero.dataSize = zero.dataPos; zero.dataPos = 0; zero.rlocPos = 0;

    // Did zero page overflow?
    if ( zero.dataSize > 0xFF ) asmfail("zero page overflow");

    // Allocate memory for each segment's data, except for BSS
    text.data = (unsigned int *) sbrk( text.dataSize * sizeof(unsigned int) );
    if ( text.data == SBRKFAIL ) asmfail( "failed to allocate memory for text segment's data" );

    data.data = (unsigned int *) sbrk( data.dataSize * sizeof(unsigned int) );
    if ( data.data == SBRKFAIL ) asmfail( "failed to allocate memory for data segment's data" );

    zero.data = (unsigned int *) sbrk( zero.dataSize * sizeof(unsigned int) );
    if ( zero.data == SBRKFAIL ) asmfail( "failed to allocate memory for zero segment's data" );

    write( 1, " *** Running relocation pass ***\r\n", 34 );

    // Reset current file number
    curfno = 0;
    flags |= FLG_RLOC;

    // *** Run relocation pass for each file ***
    while ( curfno < argc )
    {
        // Output the current file
        write( 1, "Relocating file: ", 17 );
        i = 0;
        while ( argv[curfno][i++] );
        write( 1, argv[curfno], i );
        write( 1, "\r\n", 2 );

        assemble( argv[curfno] );
        curfno++;
    }
    // Record how many relocation entries are needed and reset
    text.rlocSize = text.rlocPos; text.rlocPos = 0; text.dataPos = 0;
    data.rlocSize = data.rlocPos; data.rlocPos = 0; data.dataPos = 0;
    zero.rlocSize = zero.rlocPos; zero.rlocPos = 0; zero.dataPos = 0;
                                                     bss.dataPos = 0;

    // Allocate memory for each segment's relocation bits
    text.rloc = (struct relocate *) sbrk( text.rlocSize * sizeof(struct relocate) );
    if ( text.rloc == SBRKFAIL ) asmfail( "failed to allocate memory for text segment's relocation info" );

    data.rloc = (struct relocate *) sbrk( data.rlocSize * sizeof(struct relocate) );
    if ( data.rloc == SBRKFAIL ) asmfail( "failed to allocate memory for data segment's relocation info" );

    zero.rloc = (struct relocate *) sbrk( zero.rlocSize * sizeof(struct relocate) );
    if ( zero.rloc == SBRKFAIL ) asmfail( "failed to allocate memory for zero segment's relocation info" );

    write( 1, " *** Running assembly pass ***\r\n", 32 );

    // Reset current file number
    curfno = 0;
    flags |= FLG_DATA;

    // *** Run data pass for each file ***
    while ( curfno < argc )
    {
        // Output the current file
        write( 1, "Assembling file: ", 17 );
        i = 0;
        while ( argv[curfno][i++] );
        write( 1, argv[curfno], i );
        write( 1, "\r\n", 2 );

        assemble( argv[curfno] );
        curfno++;
    }

    // *** Output final result ***

    write( 1, "*** Generating output file ***\r\n", 32 );

    // Open a.out for writing
    int ofd = creat( "a.out", 0755 );
    if ( ofd < 0 ) asmfail( "failed to open a.out" );

    if ( flags & FLG_SMH ) // SimH output
    {
        struct segment * curseg = &zero;
        unsigned int org = 0;
        int header[3];

        // Output Zero, Text, and Data segments
        while ( curseg )
        {
            unsigned int bs = 0, be = 0;
            while ( bs < curseg->dataSize )
            {
                be += 16;
                if ( be > curseg->dataSize ) be = curseg->dataSize;

                header[0] = bs - be; // Negative block length
                header[1] = org + bs; // Location to put block in memory

                // Compute checksum
                header[2] = -header[0] - header[1];
                i = bs;
                while ( i < be ) { header[2] -= curseg->data[i]; i++; }

                write( ofd, header, 6 ); // Output header
                write( ofd, curseg->data + bs, (be - bs) * 2 ); // Output block

                bs = be;
            }

            if      ( curseg == &zero ) { org += stksize ? stksize << 10 : 256; curseg = &text; }
            else if ( curseg == &text ) { org += text.dataSize; curseg = &data; }
            else curseg = NULL;
        }

        org += data.dataSize;
        i = 0;

        // Output small BSS segment
        if ( bss.dataSize <= 16 )
        {
            if ( bss.dataSize )
            {
                header[0] = -bss.dataSize;
                header[1] = org;
                header[2] = bss.dataSize - org;

                write( ofd, header, 6 );

                while ( header[0] < 0 )
                {
                    write( ofd, &i, 2 );
                    header[0]++;
                }
            }
        }
        else
        {
            // Prime SimH with a single 0 byte block
            header[0] = -1;
            header[1] = org;
            header[2] = 1 - org;

            write( ofd, header, 6 );
            write( ofd, &i, 2 );

            // Send SimH a repeat block to fill out the rest of BSS
            header[0] = -bss.dataSize + 1;
            header[1] = org + 1;
            header[2] = -header[0] - header[2];

            write( ofd, header, 6 );
        }

        // Output start block
        header[0] = 1;
        header[1] = (stksize ? stksize << 10 : 256) + entrypos;
        header[2] = 0;

        // Should we enable auto-start
        if ( flags & FLG_SMHA ) header[1] |= 0x8000;

        write( ofd, header, 6 );
    }
    else if ( flags & FLG_TERM ) // Virtual console output
    {
        struct segment * curseg = &zero;
        unsigned int org = 0;

        write( ofd, "K", 1 ); // Make sure the current cell is closed

        while ( curseg )
        {
            if ( curseg->dataSize )
            {
                // Open the first cell
                octwrite( ofd, org );
                write( ofd, "/", 1 );

                i = 0;
                while ( i < curseg->dataSize )
                {
                    octwrite( ofd, curseg->data[i] );
                    write( ofd, "\n", 1 );
                    i++;
                }

                write( ofd, "K", 1 ); // Close the last cell
            }

            if      ( curseg == &zero ) { org += stksize ? stksize << 10 : 256; curseg = &text; }
            else if ( curseg == &text ) { org += text.dataSize; curseg = &data; }
            else curseg = NULL;
        }

        // Output BSS segment
        if ( bss.dataSize )
        {
            org += data.dataSize;

            // Open first cell
            octwrite( ofd, org );
            write( ofd, "/", 1 );

            i = 0;
            while ( i < curseg->dataSize )
            {
                write( ofd, "0\n", 2 );
                i++;
            }

            write( ofd, "K", 1 ); // Close last cell
        }

        // Initialize the stack pointer
        if ( stksize ) write( ofd, "5A400\nK", 7 );
    }
    else // Binary executable output
    {
        // Output header
	struct exec header;
        header.magic = 0; // Magic number (program load method)
        header.stack = stksize; // Stack segment length
        header.zero = zero.dataSize; // Zero segment length
        header.zrsize = zero.rlocSize; // Number of relocation entries
        header.text = text.dataSize; // Text segment length
        header.trsize = text.rlocSize; // Text segment relocation length
        header.data = data.dataSize; // Data segment length
        header.drsize = data.rlocSize; // Data segment relocation length
        header.bss = bss.dataSize; // Bss segment length
        header.syms = sympos - ASM_SIZE; // Number of symbols in table
        header.entry = (stksize ? stksize << 10 : 256) + entrypos; // Text segment entry offset

        write( ofd, &header, sizeof(struct exec) );

        // Output each segment's data
        write( ofd, zero.data, zero.dataSize * sizeof(unsigned int) );
        write( ofd, text.data, text.dataSize * sizeof(unsigned int) );
        write( ofd, data.data, data.dataSize * sizeof(unsigned int) );

        // Output symbol table
        unsigned int k, nameoff = 0;
        struct symbol outsym;
        i = ASM_SIZE;
        while ( i < sympos )
        {
            if ( (symtbl[i].type & SYM_MASK) == SYM_FILE ||
            symtbl[i].type & SYM_GLOB || flags & FLG_LOCL )
            {
                // Build output symbol
                outsym.stroff = nameoff;
                outsym.type = symtbl[i].type;
                outsym.val  = symtbl[i].val;

                write( ofd, &outsym, sizeof(struct symbol) );

                // Compute offset (+1 for null terminate)
                nameoff += symtbl[i].len + 1;
            }

            i++;
        }

        // Output string table
        i = ASM_SIZE;
        k = 0;
        while ( i < sympos )
        {
            if ( (symtbl[i].type & SYM_MASK) == SYM_FILE )
            {
                write( ofd, argv[k], symtbl[i].len );
                write( ofd, "0", 1 ); // Null terminate
            }
            else if ( symtbl[i].type & SYM_GLOB || flags & FLG_LOCL )
            {
                write( ofd, symtbl[i].name, symtbl[i].len );
                write( ofd, "0", 1 ); // Null terminate
            }

            i++;
        }

        // Output relocation data
        write( ofd, zero.rloc, zero.rlocSize * sizeof(struct relocate) );
        write( ofd, text.rloc, text.rlocSize * sizeof(struct relocate) );
        write( ofd, data.rloc, data.rlocSize * sizeof(struct relocate) );
    }

    // Close a.out
    close( ofd );

    return 0;
}
