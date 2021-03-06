//memory.c

#define USE_DL_MALLOC 1

#ifndef USE_DL_MALLOC

#include<stdio.h>

#define MEM_U8 unsigned char 
#define MEM_ULONG unsigned int 

#define TRUE		1
#define FALSE		0

#define FREE		0
#define RESERVED	1

/* Memory block header = 9 bytes = 4 previous + 4 next + status */
#define SIZE_HEADER	16

#define prev(i)			(*((MEM_ULONG *) (i)))
#define next(i)			(*((MEM_ULONG *) (i+4)))

#define status(i)		(*((MEM_U8 *) (i+8)))
#define setalign(i,y)	(*((MEM_U8 *)(i-1))) = y
#define getalign(i)		(*((MEM_U8 *)(i-1)))

#define size(i)			(next(i)-i-SIZE_HEADER)

/* if going to split free block, need at least 8 bytes in new free part */
#define MIN_FREE_BYTES   4
//static MEM_U8 memory[MEM_LENGHT] __attribute__ ((aligned (MIN_FREE_BYTES)));

static MEM_ULONG first;				/*stores address of first byte of heap*/
static MEM_ULONG last;				/*store address of last byte of heap + 1*/

void heapInit(unsigned int start, unsigned int end)
{
//	first = (MEM_ULONG)&memory[0];
//	last = (MEM_ULONG)&memory[MEM_LENGHT - 1];

	first = (start+3) & (~3);
	last = end & (~3);

	prev(first)=0;
	next(first)=0;
	status(first)=FREE;
}

static int currentNodeAlloc(MEM_ULONG i,MEM_ULONG nbytes)
{
	MEM_ULONG free_size;

	/*handle case of current block being the last*/
 	if(next(i) == 0){
		free_size = last - i - SIZE_HEADER;
	}else{
	    free_size = size(i);
	}
	
	/*either split current block, use entire current block, or fail*/
	if(free_size >= nbytes + SIZE_HEADER + MIN_FREE_BYTES)
	{
		MEM_ULONG old_next;
		MEM_ULONG old_block;
		MEM_ULONG new_block;

		old_next = next(i);
		old_block = i;

		/*fix up original block*/
		next(i) = i+SIZE_HEADER+nbytes;
		new_block = next(i);
		status(i)=RESERVED;

		/*set up new free block*/
		i = next(i);

		next(i)=old_next;
		prev(i)=old_block;
		status(i)=FREE;

		/*right nieghbor must point to new free block*/
		if(next(i)!=0)			
		{
			i = next(i);
			prev(i)=new_block;
		}
		
		return(TRUE);
	}
	else if(free_size >= nbytes)
	{
		status(i)=RESERVED;

		return(TRUE);
	}

	return(FALSE);
}

static MEM_ULONG loc_alloc(MEM_ULONG nbytes)
{
	int ret;
	MEM_ULONG i;

	nbytes = ((nbytes  + MIN_FREE_BYTES - 1)/ MIN_FREE_BYTES )  * MIN_FREE_BYTES;
	i=first;

	if(status(i)==FREE)
	{
		ret = currentNodeAlloc(i,nbytes);
		if(ret==TRUE) {
			return(i+SIZE_HEADER);
		}
	}

	while(next(i)!=0)
	{
		i=next(i);
		if(status(i)==FREE)
		{
			ret = currentNodeAlloc(i,nbytes);
			if(ret==TRUE) {
				return(i+SIZE_HEADER);
			}
		}
	}

    return 0;
}

void* Drv_alloc(MEM_ULONG nbytes)
{
	MEM_ULONG i = loc_alloc(nbytes);
	if(i != 0) setalign(i,0);

	return ((void*)i);
}

void* Drv_memalign(MEM_ULONG align, MEM_ULONG size)
{
	return Drv_alloc(size);
}

#if 0
MEM_ULONG alignAlloc(MEM_ULONG align, MEM_ULONG size)
{
	MEM_ULONG i = loc_alloc(size + align);
	if(i != 0)
	{
		align = i - (i / align) * align;
		i += align;
		setalign(i,align);    
	}

	return i;
}
#endif

static void loc_free(MEM_ULONG address)
{
	MEM_ULONG block;
	MEM_ULONG lblock;
	MEM_ULONG rblock;
  
	block = address-SIZE_HEADER;
	lblock = prev(block);
	rblock = next(block);

	/*
	4 cases: FFF->F, FFR->FR, RFF->RF, RFR 
	always want to merge free blocks 
	*/

	if((lblock!=0)&&(rblock!=0)&&(status(lblock)==FREE)&&(status(rblock)==FREE))
	{
		next(lblock)=next(rblock);
		status(lblock)=FREE;
		if(next(rblock)!=0)	prev(next(rblock))=lblock;
	}
	else if((lblock!=0)&&(status(lblock)==FREE))
	{
		next(lblock)=next(block);
		status(lblock)=FREE;
		if(next(block)!=0)	prev(next(block))=lblock;
	}
	else if((rblock!=0)&&(status(rblock)==FREE))
	{
		next(block)=next(rblock);
		status(block)=FREE;
		if(next(rblock)!=0)	prev(next(rblock))=block;
	}
	else
	{
		status(block)=FREE;
	}
}

/*Note: disaster will strike if fed wrong address*/
void Drv_deAlloc(void* address)
{
	address -= getalign(address);
	loc_free((unsigned int)address);
}

void* Drv_realloc(void* address,MEM_ULONG nbytes)
{
	MEM_ULONG addr,oldsize;
	MEM_ULONG block,rblock,rrblock;
	MEM_ULONG bsize,rbsize,align;
	void* rr;

	oldsize = nbytes;
	nbytes = ((nbytes  + MIN_FREE_BYTES - 1)/ MIN_FREE_BYTES )  * MIN_FREE_BYTES;

	rr = address;
	if (nbytes == 0) {
		Drv_deAlloc(rr);

		return ((void*)0);
	}
	if (address == 0)
	{
		addr = loc_alloc(nbytes);
	    if(addr != 0)	setalign(addr,0);

	    return ((void*)addr);
	}

	align = getalign(address);
	address -= getalign(address);
	address -= SIZE_HEADER;    
	bsize = size((unsigned int)address);

	if(nbytes <= bsize-align) {
		return ((void*)rr);
	}

	rblock = next((unsigned int)address);
	if((rblock != 0) &&(status(rblock) == FREE) )
	{
		bsize += size(rblock);
		if(bsize >= nbytes + align)
		{
			rrblock = next(rblock);
			next((unsigned int)address) = ((unsigned int)address + nbytes + align + SIZE_HEADER);
			block = next((unsigned int)address);
			prev(block) = (unsigned int)address;
			next(block) = rrblock; 
			status(block) = FREE;
			return ((void*)rr);
		}
	}
	addr = loc_alloc(nbytes+align);

	if(addr == 0) {
		return ((void*)0);
	}

	addr += align;		
	setalign(addr,align);	
	memcpy(addr,rr,bsize);
	Drv_deAlloc(rr);

	return ((void*)addr);
}

void* Drv_calloc(MEM_ULONG nmem,MEM_ULONG size)
{
	void* rr;

	rr = Drv_alloc(size * nmem);
	memset(rr,0,size * nmem);

	return ((void*)rr);
}

#if 0
static const char *statStr[] = {"FREE","RESERVED"};

void printMemory()
{
	MEM_ULONG i;
	i=first;
	printf("[0x%08x,0x%08x,0x%08x,%s]\n",prev(i),i,next(i),statStr[status(i)]);
	while(next(i)!=0)
	{
		i=next(i);
		printf("[0x%08x,0x%08x,0x%08x,%s]\n",prev(i),i,next(i),statStr[status(i)]);
	}
	return;
}
#endif

#else
#define HAVE_MMAP 0
#define HAVE_MORECORE 0
#define ONLY_MSPACES 1
#define DEFAULT_GRANULARITY ((size_t)64U * (size_t)1024U)
#define ABORT_ON_ASSERT_FAILURE 0
#define ABORT 
#define USE_LOCKS 0
#define USE_SPIN_LOCKS 0
#define MALLOC_ALIGNMENT 8
#define NO_SYS_ALLOC 1
#include "malloc.c.h"

static mspace _mspace;

void heapInit(unsigned int start, unsigned int end)
{
	start = (start + 3) & (~3);
	end = end & (~3);

	_mspace = create_mspace_with_base((void*)start, end - start, 0);
}

void* Drv_alloc(unsigned int nbytes)
{
	return mspace_malloc(_mspace, nbytes);
}

void* Drv_memalign(unsigned int align, unsigned int nbytes)
{
	return mspace_memalign(_mspace, align, nbytes);
}

void Drv_deAlloc(void* address)
{
	mspace_free(_mspace, address);
}

void* Drv_realloc(void* address, unsigned int nbytes)
{
	return mspace_realloc(_mspace, address, nbytes);
}

void* Drv_calloc(unsigned int nmem, unsigned int size)
{
	return mspace_calloc(_mspace, nmem, size);
}

void* Drv_valloc(unsigned int size)
{
	return mspace_valloc(size);
}

void* Drv_pvalloc(unsigned int size)
{
	return mspace_pvalloc(size);
}



# define strong_alias(name, aliasname) _strong_alias(name, aliasname)
# define _strong_alias(name, aliasname) \
  extern __typeof (name) aliasname __attribute__ ((alias (#name)));
  
# define weak_alias(name, aliasname) _weak_alias (name, aliasname)
# define _weak_alias(name, aliasname) \
  extern __typeof (name) aliasname __attribute__ ((weak, alias (#name)));

weak_alias (Drv_calloc, __calloc) weak_alias (Drv_calloc, calloc)
strong_alias (Drv_deAlloc, __cfree) weak_alias (Drv_deAlloc, cfree)
strong_alias (Drv_deAlloc, __free) strong_alias (Drv_deAlloc, free)
strong_alias (Drv_alloc, __malloc) strong_alias (Drv_alloc, malloc)
strong_alias (Drv_memalign, __memalign)
weak_alias (Drv_memalign, memalign)

weak_alias (Drv_realloc, __realloc) weak_alias (Drv_realloc, realloc)
weak_alias (Drv_valloc, __valloc) weak_alias (Drv_valloc, valloc)
weak_alias (Drv_pvalloc, __pvalloc) weak_alias (Drv_pvalloc, pvalloc)

#endif
