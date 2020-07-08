// Memory segments
struct segment text = { { -1, 0, 0 }, { -1, 0, 0 }, SYM_TEXT };
struct segment data = { { -1, 0, 0 }, { -1, 0, 0 }, SYM_DATA };
struct segment bss  = { { -1, 0, 0 }, { -1, 0, 0 }, SYM_BSS  };
struct segment zero = { { -1, 0, 0 }, { -1, 0, 0 }, SYM_ZERO };
// Segment currently being worked on
struct segment * curseg;

// Set a word in a segment
void segset( struct segment * seg, unsigned int reloc, unsigned int val )
{
    // Can't output to a BSS segment
    if ( seg->sym == SYM_BSS ) asmfail( "tried to output to BSS segment" );

    // Not on data pass yet
    if ( ~flags & FLG_DATA ) return;

    // Store relocation bits if needed
    if ( reloc )
    {
        // Add symbol offsets
        if      ( (reloc & SYM_MASK) == SYM_TEXT ) val +=  stksize ? stksize << 10 : 256;
        else if ( (reloc & SYM_MASK) == SYM_DATA ) val += (stksize ? stksize << 10 : 256) + text.data.size;
        else if ( (reloc & SYM_MASK) == SYM_BSS  ) val += (stksize ? stksize << 10 : 256) + text.data.size + data.data.size;

        int cpos = seg->data.pos;

        write( seg->rloc.fd, &reloc, sizeof(int) );
        write( seg->rloc.fd, &cpos, sizeof(int) );

        seg->rloc.size++;
    }

    write( seg->data.fd, &val, sizeof(int) );
}
