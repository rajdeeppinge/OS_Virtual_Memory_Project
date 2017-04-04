
/*
Do not modify this file.
Make all of your changes to main.c instead.
*/

#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>
#include <limits.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ucontext.h>

#include "page_table.h"



// structure holding the meta-data for page table
struct page_table {
	int fd;			// points to page table emulated by a file
	char *virtmem;		// pointer to start of virtual memory
	int npages;		// no of pages in virtual memory
	char *physmem;		// pointer to start of physical memory
	int nframes;		// no of frames in physical memory
	int *page_mapping;	// pointer to start of mapping between physical and virtual memory
	int *page_bits;		// pointer to start of permission bits list 
	page_fault_handler_t handler;	//
};



// create a placeholder pointer for page table
struct page_table *the_page_table = 0;




static void internal_fault_handler( int signum, siginfo_t *info, void *context )
{

#ifdef i386
	char *addr = (char*)(((struct ucontext *)context)->uc_mcontext.cr2);
#else
	char *addr = info->si_addr;
#endif

	struct page_table *pt = the_page_table;

	if(pt) {
		int page = (addr-pt->virtmem) / PAGE_SIZE;

		if(page>=0 && page<pt->npages) {
			pt->handler(pt,page);
			return;
		}
	}

	fprintf(stderr,"segmentation fault at address %p\n",addr);
	abort();
}



/* Create a new page table, along with a corresponding virtual memory
that is "npages" big and a physical memory that is "nframes" bit
 When a page fault occurs, the routine pointed to by "handler" will be called. */
struct page_table * page_table_create( int npages, int nframes, page_fault_handler_t handler )
{
	int i;
	struct sigaction sa;
	struct page_table *pt;
	char filename[256];

	pt = malloc(sizeof(struct page_table));
	if(!pt) return 0;			// if malloc fails return 0 i.e. page table not created

	the_page_table = pt;		// assign the created page to be pointed to by the placeholder pointer created earlier		

	sprintf(filename,"/tmp/pmem.%d.%d",getpid(),getuid());	// generate a unique file name

	// create a new file which emulates page table
	pt->fd = open(filename,O_CREAT|O_TRUNC|O_RDWR,0777);
	if(!pt->fd) return 0;		// if file creation fails, then return 0

	ftruncate(pt->fd,PAGE_SIZE*npages);

	unlink(filename);

	pt->physmem = mmap(0,nframes*PAGE_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,pt->fd,0);
	pt->nframes = nframes;

	pt->virtmem = mmap(0,npages*PAGE_SIZE,PROT_NONE,MAP_SHARED|MAP_NORESERVE,pt->fd,0);
	pt->npages = npages;

	pt->page_bits = malloc(sizeof(int)*npages);
	pt->page_mapping = malloc(sizeof(int)*npages);

	pt->handler = handler;

	for(i=0;i<pt->npages;i++) pt->page_bits[i] = 0;

	sa.sa_sigaction = internal_fault_handler;
	sa.sa_flags = SA_SIGINFO;

	sigfillset( &sa.sa_mask );
	sigaction( SIGSEGV, &sa, 0 );

	return pt;
}



/* Delete a page table and the corresponding virtual and physical memories. */
// This does not delete the disk
void page_table_delete( struct page_table *pt )
{
	munmap(pt->virtmem,pt->npages*PAGE_SIZE);
	munmap(pt->physmem,pt->nframes*PAGE_SIZE);

	// free the list of page bits
	free(pt->page_bits);

	// free the list of page mappings
	free(pt->page_mapping);

	// close the file descriptor which points to the page table	
	close(pt->fd);

	// free the page table structure which contains information about page table
	free(pt);
}



/*
Set the frame number and access bits associated with a page.
The bits may be any of PROT_READ, PROT_WRITE, or PROT_EXEC logical-ored together.
*/
void page_table_set_entry( struct page_table *pt, int page, int frame, int bits )
{
	// if page out of bounds
	if( page<0 || page>=pt->npages ) {
		fprintf(stderr,"page_table_set_entry: illegal page #%d\n",page);
		abort();
	}

	// if frame out of bounds
	if( frame<0 || frame>=pt->nframes ) {
		fprintf(stderr,"page_table_set_entry: illegal frame #%d\n",frame);
		abort();
	}

	// otherwise map frame to page.
	pt->page_mapping[page] = frame;

	// Assign page bits received as parameters
	pt->page_bits[page] = bits;

	//
	remap_file_pages(pt->virtmem+page*PAGE_SIZE,PAGE_SIZE,0,frame,0);
	mprotect(pt->virtmem+page*PAGE_SIZE,PAGE_SIZE,bits);
}



/*
Get the frame number and access bits associated with a page.
"frame" and "bits" must be pointers to integers which will be filled with the current values.
The bits may be any of PROT_READ, PROT_WRITE, or PROT_EXEC logical-ored together.
*/
void page_table_get_entry( struct page_table *pt, int page, int *frame, int *bits )
{
	// if page out of bounds. error
	if( page<0 || page>=pt->npages ) {
		fprintf(stderr,"page_table_get_entry: illegal page #%d\n",page);
		abort();
	}

	// otherwise get frame mapped to page and get page bits
	*frame = pt->page_mapping[page];
	*bits = pt->page_bits[page];
}



/* Print out the page table entry for a single page. */
void page_table_print_entry( struct page_table *pt, int page )
{
	// if page not in bounds of page table
	if( page<0 || page>=pt->npages ) {
		fprintf(stderr,"page_table_print_entry: illegal page #%d\n",page);
		abort();
	}

	// take out permission bits of the page
	int b = pt->page_bits[page];

	// print out the entry with page no., frame no. and permission bits
	printf("page %06d: frame %06d bits %c%c%c\n",
		page,
		pt->page_mapping[page],
		b&PROT_READ  ? 'r' : '-',
		b&PROT_WRITE ? 'w' : '-',
		b&PROT_EXEC  ? 'x' : '-'
	);

}



/* Print out the state of every page in a page table. */
void page_table_print( struct page_table *pt )
{
	int i;
	for(i=0;i<pt->npages;i++) {
		page_table_print_entry(pt,i);
	}
}



/* Return the total number of frames in the physical memory. */
int page_table_get_nframes( struct page_table *pt )
{
	return pt->nframes;
}



/* Return the total number of pages in the virtual memory. */
int page_table_get_npages( struct page_table *pt )
{
	return pt->npages;
}



/* Return a pointer to the start of the virtual memory associated with a page table. */
char * page_table_get_virtmem( struct page_table *pt )
{
	return pt->virtmem;
}



/* Return a pointer to the start of the physical memory associated with a page table. */
char * page_table_get_physmem( struct page_table *pt )
{
	return pt->physmem;
}
