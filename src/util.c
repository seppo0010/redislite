#define SLOT_2_0     0x001fc07f
#define SLOT_4_2_0   0xf01fc07f

/*
** This function is taken from sqlite. Original name: sqlite3PutVarint
** Write a 64-bit variable-length integer to memory starting at p[0].
** The length of data write will be between 1 and 9 bytes.  The number
** of bytes written is returned.
**
** A variable-length integer consists of the lower 7 bits of each byte
** for all bytes that have the 8th bit set and one byte with the 8th
** bit clear.  Except, if we get to the 9th byte, it stores the full
** 8 bits and is the last byte.
*/
int redislitePutVarint(unsigned char *p, long long v){
  int i, j, n;
  int buf[10];
  if( v & (((long long)0xff000000)<<32) ){
    p[8] = (int)v;
    v >>= 8;
    for(i=7; i>=0; i--){
      p[i] = (int)((v & 0x7f) | 0x80);
      v >>= 7;
    }
    return 9;
  }    
  n = 0;
  do{
    buf[n++] = (int)((v & 0x7f) | 0x80);
    v >>= 7;
  }while( v!=0 );
  buf[0] &= 0x7f;
  for(i=0, j=n-1; j>=0; j--, i++){
    p[i] = buf[j];
  }
  return n;
}

/*
** This function is taken from sqlite. Original name: sqlite3PutVarint32
** This routine is a faster version of sqlite3PutVarint() that only
** works for 32-bit positive integers and which is optimized for
** the common case of small integers.  A MACRO version, putVarint32,
** is provided which inlines the single-byte case.  All code should use
** the MACRO version as this function assumes the single-byte case has
** already been handled.
*/
int redislitePutVarint32(unsigned char *p, int v){
#ifndef putVarint32
  if( (v & ~0x7f)==0 ){
    p[0] = v;
    return 1;
  }
#endif
  if( (v & ~0x3fff)==0 ){
    p[0] = (int)((v>>7) | 0x80);
    p[1] = (int)(v & 0x7f);
    return 2;
  }
  return redislitePutVarint(p, v);
}

/*
** This function is taken from sqlite. Original name: sqlite3GetVarint
** Read a 64-bit variable-length integer from memory starting at p[0].
** Return the number of bytes read.  The value is stored in *v.
*/
int redisliteGetVarint(const unsigned char *p, long long *v){
  int a,b,s;

  a = *p;
  /* a: p0 (unmasked) */
  if (!(a&0x80))
  {
    *v = a;
    return 1;
  }

  p++;
  b = *p;
  /* b: p1 (unmasked) */
  if (!(b&0x80))
  {
    a &= 0x7f;
    a = a<<7;
    a |= b;
    *v = a;
    return 2;
  }

  /* Verify that constants are precomputed correctly */
  p++;
  a = a<<14;
  a |= *p;
  /* a: p0<<14 | p2 (unmasked) */
  if (!(a&0x80))
  {
    a &= SLOT_2_0;
    b &= 0x7f;
    b = b<<7;
    a |= b;
    *v = a;
    return 3;
  }

  /* CSE1 from below */
  a &= SLOT_2_0;
  p++;
  b = b<<14;
  b |= *p;
  /* b: p1<<14 | p3 (unmasked) */
  if (!(b&0x80))
  {
    b &= SLOT_2_0;
    /* moved CSE1 up */
    /* a &= (0x7f<<14)|(0x7f); */
    a = a<<7;
    a |= b;
    *v = a;
    return 4;
  }

  /* a: p0<<14 | p2 (masked) */
  /* b: p1<<14 | p3 (unmasked) */
  /* 1:save off p0<<21 | p1<<14 | p2<<7 | p3 (masked) */
  /* moved CSE1 up */
  /* a &= (0x7f<<14)|(0x7f); */
  b &= SLOT_2_0;
  s = a;
  /* s: p0<<14 | p2 (masked) */

  p++;
  a = a<<14;
  a |= *p;
  /* a: p0<<28 | p2<<14 | p4 (unmasked) */
  if (!(a&0x80))
  {
    /* we can skip these cause they were (effectively) done above in calc'ing s */
    /* a &= (0x7f<<28)|(0x7f<<14)|(0x7f); */
    /* b &= (0x7f<<14)|(0x7f); */
    b = b<<7;
    a |= b;
    s = s>>18;
    *v = ((long long)s)<<32 | a;
    return 5;
  }

  /* 2:save off p0<<21 | p1<<14 | p2<<7 | p3 (masked) */
  s = s<<7;
  s |= b;
  /* s: p0<<21 | p1<<14 | p2<<7 | p3 (masked) */

  p++;
  b = b<<14;
  b |= *p;
  /* b: p1<<28 | p3<<14 | p5 (unmasked) */
  if (!(b&0x80))
  {
    /* we can skip this cause it was (effectively) done above in calc'ing s */
    /* b &= (0x7f<<28)|(0x7f<<14)|(0x7f); */
    a &= SLOT_2_0;
    a = a<<7;
    a |= b;
    s = s>>18;
    *v = ((long long)s)<<32 | a;
    return 6;
  }

  p++;
  a = a<<14;
  a |= *p;
  /* a: p2<<28 | p4<<14 | p6 (unmasked) */
  if (!(a&0x80))
  {
    a &= SLOT_4_2_0;
    b &= SLOT_2_0;
    b = b<<7;
    a |= b;
    s = s>>11;
    *v = ((long long)s)<<32 | a;
    return 7;
  }

  /* CSE2 from below */
  a &= SLOT_2_0;
  p++;
  b = b<<14;
  b |= *p;
  /* b: p3<<28 | p5<<14 | p7 (unmasked) */
  if (!(b&0x80))
  {
    b &= SLOT_4_2_0;
    /* moved CSE2 up */
    /* a &= (0x7f<<14)|(0x7f); */
    a = a<<7;
    a |= b;
    s = s>>4;
    *v = ((long long)s)<<32 | a;
    return 8;
  }

  p++;
  a = a<<15;
  a |= *p;
  /* a: p4<<29 | p6<<15 | p8 (unmasked) */

  /* moved CSE2 up */
  /* a &= (0x7f<<29)|(0x7f<<15)|(0xff); */
  b &= SLOT_2_0;
  b = b<<8;
  a |= b;

  s = s<<4;
  b = p[-4];
  b &= 0x7f;
  b = b>>3;
  s |= b;

  *v = ((long long)s)<<32 | a;

  return 9;
}
/*
** This function is taken from sqlite. Original name: sqlite3GetVarint32
** Read a 32-bit variable-length integer from memory starting at p[0].
** Return the number of bytes read.  The value is stored in *v.
**
** If the varint stored in p[0] is larger than can fit in a 32-bit unsigned
** integer, then set *v to 0xffffffff.
**
** A MACRO version, getVarint32, is provided which inlines the 
** single-byte case.  All code should use the MACRO version as 
** this function assumes the single-byte case has already been handled.
*/
int redisliteGetVarint32(const unsigned char *p, int *v){
  int a,b;

  /* The 1-byte case.  Overwhelmingly the most common.  Handled inline
  ** by the getVarin32() macro */
  a = *p;
  /* a: p0 (unmasked) */
#ifndef getVarint32
  if (!(a&0x80))
  {
    /* Values between 0 and 127 */
    *v = a;
    return 1;
  }
#endif

  /* The 2-byte case */
  p++;
  b = *p;
  /* b: p1 (unmasked) */
  if (!(b&0x80))
  {
    /* Values between 128 and 16383 */
    a &= 0x7f;
    a = a<<7;
    *v = a | b;
    return 2;
  }

  /* The 3-byte case */
  p++;
  a = a<<14;
  a |= *p;
  /* a: p0<<14 | p2 (unmasked) */
  if (!(a&0x80))
  {
    /* Values between 16384 and 2097151 */
    a &= (0x7f<<14)|(0x7f);
    b &= 0x7f;
    b = b<<7;
    *v = a | b;
    return 3;
  }

  /* A 32-bit varint is used to store size information in btrees.
  ** Objects are rarely larger than 2MiB limit of a 3-byte varint.
  ** A 3-byte varint is sufficient, for example, to record the size
  ** of a 1048569-byte BLOB or string.
  **
  ** We only unroll the first 1-, 2-, and 3- byte cases.  The very
  ** rare larger cases can be handled by the slower 64-bit varint
  ** routine.
  */
#if 1
  {
    long long v64;
    int n;

    p -= 2;
    n = redisliteGetVarint(p, &v64);
    if( (v64 & ((((long long)1)<<32)-1))!=v64 ){
      *v = 0xffffffff;
    }else{
      *v = (int)v64;
    }
    return n;
  }

#else
  /* For following code (kept for historical record only) shows an
  ** unrolling for the 3- and 4-byte varint cases.  This code is
  ** slightly faster, but it is also larger and much harder to test.
  */
  p++;
  b = b<<14;
  b |= *p;
  /* b: p1<<14 | p3 (unmasked) */
  if (!(b&0x80))
  {
    /* Values between 2097152 and 268435455 */
    b &= (0x7f<<14)|(0x7f);
    a &= (0x7f<<14)|(0x7f);
    a = a<<7;
    *v = a | b;
    return 4;
  }

  p++;
  a = a<<14;
  a |= *p;
  /* a: p0<<28 | p2<<14 | p4 (unmasked) */
  if (!(a&0x80))
  {
    /* Values  between 268435456 and 34359738367 */
    a &= SLOT_4_2_0;
    b &= SLOT_4_2_0;
    b = b<<7;
    *v = a | b;
    return 5;
  }

  /* We can only reach this point when reading a corrupt database
  ** file.  In that case we are not in any hurry.  Use the (relatively
  ** slow) general-purpose sqlite3GetVarint() routine to extract the
  ** value. */
  {
    long long v64;
    int n;

    p -= 4;
    n = redisliteGetVarint(p, &v64);
    *v = (int)v64;
    return n;
  }
#endif
}

void redislite_put_4bytes(unsigned char *p, int v){
  p[0] = (unsigned char)(v>>24);
  p[1] = (unsigned char)(v>>16);
  p[2] = (unsigned char)(v>>8);
  p[3] = (unsigned char)v;
}

int redislite_get_4bytes(const char *p){ 
  return (p[0]<<24) | (p[1]<<16) | (p[2]<<8) | p[3];
}

