// Memory segments
struct segment text = {NULL, 0, 0, NULL, 0, 0, SYM_TEXT };
struct segment data = {NULL, 0, 0, NULL, 0, 0, SYM_DATA };
struct segment  bss = {NULL, 0, 0, NULL, 0, 0, SYM_BSS  };
struct segment zero = {NULL, 0, 0, NULL, 0, 0, SYM_ZERO };
// Segment currently being worked on
struct segment * curseg;

// Set a word in a segment
void segset( struct segment * seg, unsigned int reloc, unsigned int val )
{
    struct relocate * curRloc = NULL;

    // Can't output to a BSS segment
    if ( seg->sym == SYM_BSS ) asmfail( "tried to output to BSS segment" );

    // Not on relocation pass yet
    if ( ~flags & FLG_RLOC ) return;

    // Compute relocation address and increment
    if ( reloc ) curRloc = seg->rloc + seg->rlocPos++;
//    {
//        write( 1, "RELOC: ", 7 );
//        octwrite( 1, reloc );
//        write( 1, "\r\n", 2 );
//    }

    // Not on data pass yet
    if ( ~flags & FLG_DATA ) return;

    // Beyond end of memory
    if ( seg->dataPos > seg->dataSize ) asmfail( "tried to output beyond end of segment's data" );
    if ( seg->rlocPos > seg->rlocSize ) asmfail( "tried to output beyond end of segment's relocation info" );

    // Add symbol offsets
    if      ( seg->sym == SYM_TEXT ) val += 1 + stksize << 10;
    else if ( seg->sym == SYM_DATA ) val += text.dataSize + (1 + stksize << 10);

    // Store actual data value
    seg->data[seg->dataPos] = val;

    // Store relocation bits if needed
    if ( reloc )
    {
        curRloc->head = reloc;
        curRloc->addr = seg->dataPos;
    }
}
