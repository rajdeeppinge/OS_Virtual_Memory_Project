/* 
	Author - Rajdeep Pinge, Aditya Joglekar
	Time period - March-April, 2017
*/


/* Code Reference:
	Basic code given by University of Notre Dame for project on virtual memory in their cse30341 operating systems course.

	link to the project description: http://www3.nd.edu/~dthain/courses/cse30341/spring2017/project5/
	link to the source code: http://www3.nd.edu/~dthain/courses/cse30341/spring2017/project5/source/
*/


/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/


#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


int nframes; // stores total number of frames
int *is_frame_occup_struc = NULL; // pointer to free frame data struc
int *frame_holds_what = NULL;		// array to maintain which frame holds which physical page
char *PRAlgoToUse;				// store which page replacement algorithm to use 

char *virtmem = NULL;
char *physmem = NULL;
struct disk *disk = NULL; // global pointer to disk



// data struct for fifo
int oldest_page = 0;
int newest_page = 0;
int *fifo_page_queue = NULL;



//used to track statistics to print at the end
int pageFaults = 0;
int diskReads = 0;
int diskWrites = 0;

// function definitions
int findnset_free_frame(int *is_frame_occup_struc,int nframes);
void random_pra( struct page_table *pt, int page );
void fifo_pra( struct page_table *pt, int page);
void custom_pra( struct page_table *pt, int page);



/* Some definitions
	pt: pointer to page table
	page: virtual page requested
*/


/**************** Version 1 of Page Fault Handler ***********************/
/* This function handles the page faults generated while accessing the memory

	In this version We do nothing and simply terminate program when page fault happens
*/
/*
void page_fault_handler( struct page_table *pt, int page )
{
        printf("page fault on page #%d\n",page);
        exit(1);
}
*/



/**************** Version 2 of Page Fault Handler ***********************/
/* This function handles the page faults generated while accessing the memory

	In this version Basic Page fault handling is done assuming number of pages = number of page-frames
*/
/*
void page_fault_handler( struct page_table *pt, int page )
{
	printf("page fault on page #%d\n",page);
	
	// map virtual page to physical page same as its own number
	page_table_set_entry(pt, page, page, PROT_READ|PROT_WRITE);

}
*/



/**************** Version 3 of Page Fault Handler ***********************/
/* This function handles the page faults generated while accessing the memory

	In this version the entry is first given only the read access and then is required write access is given
*/
/*
void page_fault_handler( struct page_table *pt, int page )
{
	printf("page fault on page #%d\n",page);

	
	int bits, mapframe;

	// get entry in the page table	
	page_table_get_entry( pt, page, &mapframe, &bits );


	// FAULT TYPE 1 - page not in virtual memory i.e. no protection bits set i.e. entry in page table is free
	if( ((bits & PROT_READ) == 0) && ((bits & PROT_WRITE) == 0) && ((bits & PROT_EXEC) == 0) )
	{
		bits = bits|PROT_READ;
		page_table_set_entry(pt, page, page, bits);
	}
	
	else if( ((bits & PROT_READ) == 1) && ((bits & PROT_WRITE) == 0) ) // FAULT TYPE 2 - page in virtual memory but does not have write permission
	{
		bits = bits|PROT_WRITE;
		page_table_set_entry( pt, page, mapframe, bits );
	}


// code for testing
//	page_table_print(pt);
//	printf("------------------------------------------------\n");

	
//	exit(1);
}
*/



/**************** Version 4 of Page Fault Handler ***********************/
/* This function handles the page faults generated while accessing the memory

	In this version the entry is first given only the read access and then is required write access is given
*/
void page_fault_handler( struct page_table *pt, int page )
{
    printf("page fault on page #%d\n",page); // print this virtual page is needed

	pageFaults++;							//increment page faults

	// variables to information about the page on which page fault has occured
    int curr_bits;
    int curr_frame;
    
	// get the details of the page table entry corresponding to the page i.e. which frame does it hold and what are the permission bits
    page_table_get_entry( pt, page, &curr_frame, &curr_bits ); 
       

    // FAULT TYPE 1 - page not in virtual memory i.e. no protection bits set i.e. entry in page table is free 
    if ( ( (curr_bits & PROT_READ)==0 ) && ( (curr_bits & PROT_WRITE)==0 ) && ( (curr_bits & PROT_EXEC)==0 ) )
    {
		// find a free frame
		int free_loc = findnset_free_frame(is_frame_occup_struc, nframes);

		if (free_loc != -1) // have found a free frame. Bring page in that free frame
		{
			// set an entry of page in page table to free_loc frame location and give read access to it
			page_table_set_entry(pt, page, free_loc, 0|PROT_READ);

			// if fifo then need to store this frame in queue 
			if ( !strcmp(PRAlgoToUse, "fifo") )
			{
				fifo_page_queue[newest_page] = free_loc;
				newest_page = (newest_page + 1) % nframes;		// make it a circular queue
			}


			// Read data from disk at virtual address given by 'page' to physical memory frame
			disk_read(disk, page, &physmem[free_loc*PAGE_SIZE]);
			diskReads++;

			// Store info that this page is held in which page frame.
			// this frame holds this page, inverse of page table.
			frame_holds_what[free_loc] = page; 
		}

		else		// all frames all full. Need to kick out some page from some frame. Will need page replacement algorithm
		{ 
			 
			if (!strcmp(PRAlgoToUse, "rand"))
			{
				random_pra(pt, page);
			}

			else if (!strcmp(PRAlgoToUse, "fifo"))
			{
				fifo_pra(pt, page);
			}
			
			else if (!strcmp(PRAlgoToUse, "custom"))
			{
				custom_pra(pt, page);
			}

			else //check for incorrect policy name
			{
				printf("use: virtmem <npages> <nframes> <rand|fifo|custom> <sort|scan|focus>\n");
				exit(1);
			}
			
		}
		

    }

    else // FAULT TYPE 2 - page is in virtual memory but does not have necessary permissions
    {
		// dont have write permission but has read
		if ( ( (curr_bits & PROT_WRITE)==0 ) && ( ( curr_bits & PROT_READ ) ==1 ) )
		{
			//OR curr_bits with PROC masks to get 1's at req positions.		   
			page_table_set_entry( pt, page,curr_frame,curr_bits|PROT_WRITE);  
		}

		else // has write but not read, may happen though unlikely
		{
			// OR with write permission
			page_table_set_entry( pt, page,curr_frame,(curr_bits)| PROT_READ);
		}
    }

	page_table_print(pt);
} 



int main( int argc, char *argv[] )
{
	// check if all command line arguments are given
	if(argc!=5) {
		printf("use: virtmem <npages> <nframes> <rand|fifo|custom> <sort|scan|focus>\n");
		return 1;
	}

	// derive details of the program to run from command line arguments
	int npages = atoi(argv[1]);
	nframes = atoi(argv[2]);		//made global
	PRAlgoToUse = argv[3];			// store which page replacement algorithm to use 
	const char *program = argv[4];

	
	is_frame_occup_struc = malloc(nframes * sizeof(int)); // allocate memory for structure storing frame occupation info

	if(is_frame_occup_struc == NULL) {
		printf("Error allocating space for structure storing frame occupation information!\n");
		exit(1);
	}

	frame_holds_what = malloc(nframes * sizeof(int));		// allocate memory for array storing which frame holds which physical page
    
	if(frame_holds_what == NULL) {
		printf("Error allocating space for array storing info about which frame holds what page!\n");
		exit(1);
	}


    // initialize the arrays
    for(int i=0; i < nframes; i++)
    {
       is_frame_occup_struc[i] = 0;  // initially no frame is occupied
       frame_holds_what[i] = 0;  		// initially no frame holds any physical page
    }


	// for fifo
	if ( !strcmp(PRAlgoToUse, "fifo") )
	{
		fifo_page_queue = malloc(nframes * sizeof(int));
		if(fifo_page_queue == NULL) {
			printf("Error allocating space for fifo page queue!\n");
			exit(1);
		}
	}



	// try to create a disk
	disk = disk_open("myvirtualdisk", npages);
	
	// if 0 is returned then disk is not created. Therefore show error
	if(!disk) {
		fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
		return 1;
	}

	// try to cretae page table
	struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
	
	// if 0 is returned then page table is not created. Therefore show error
	if(!pt) {
		fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
		return 1;
	}

	// get pointer to virtual memory from the page table
	virtmem = page_table_get_virtmem(pt);

	// get pointer to physical memory from the page table
	physmem = page_table_get_physmem(pt);


	// run appropriate program base on the command given by the user.
	if(!strcmp(program,"sort")) {
		sort_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"scan")) {
		scan_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"focus")) {
		focus_program(virtmem,npages*PAGE_SIZE);

	} else {
		fprintf(stderr,"unknown program: %s\n",argv[3]);

	}

	page_table_print(pt);

	//print results to user
	printf("Disk Reads: %d\n", diskReads);
	printf("Disk Writes: %d\n", diskWrites);
	printf("Page Faults: %d\n", pageFaults);


	// free the allocated resources
	free(is_frame_occup_struc);
    free(frame_holds_what);

	// clean used resources
	page_table_delete(pt);
	disk_close(disk);

	return 0;
}


/*
	This function implements random page replacement algorithm when a page fault occurs
	Algorithm: A random frame is chosen from available frames for replacement and the page that it holds is replaced
*/

void random_pra( struct page_table *pt, int page )
{
	int frame_no_toremove= (int)lrand48()%nframes;

	printf("

	int pageno_to_remove= frame_holds_what[frame_no_toremove]; // what page does the frame hold?

	// get page table entry of that page
	int frame_toremove; 
	int frame_toremove_bits;

	page_table_get_entry(pt, pageno_to_remove, &frame_toremove, &frame_toremove_bits ); // info from page table 


	// if dirty i.e if it has write access, then have to write this page back in disk and then replace the page
	if ( (frame_toremove_bits&PROT_WRITE)!=0 )
	{
		disk_write( disk,pageno_to_remove, &physmem[(frame_toremove)*PAGE_SIZE] ); // write back page to disk
		diskWrites++;	
	}

	page_table_set_entry( pt, pageno_to_remove, 0, 0); // 0's invalidate frame entry of previous page

	page_table_set_entry( pt, page, (frame_toremove), 0|PROT_READ ); // set new page table entry with read permission

	disk_read( disk, page, &physmem[(frame_toremove)*PAGE_SIZE] ); // Read data from disk at virtual address given by 'page' to physical memory frame
	diskReads++;


	// Store info that this page is held in which age frame.
	// this frame holds this page, inverse of page table.
	frame_holds_what[(frame_toremove)] = page; // the frame now contains this page.

}



/*
	This function implements first in first out (fifo) page replacement algorithm when a page fault occurs
	Algorithm: A page which came first is chosen for replacement.
*/ 
void fifo_pra( struct page_table *pt, int page )
{
	int frame_no_toremove= fifo_page_queue[oldest_page];


	// NOTE here that page will be replaced only if all frames are full
	// i.e. pointer to newest page location point actually at oldest page which is to be deleted	

	// shift oldest_page to next oldest_page so that this page is removed from the queue
	oldest_page = (oldest_page + 1) % nframes;
	

	int pageno_to_remove= frame_holds_what[frame_no_toremove]; // what page does the frame hold?

	// get page table entry of that page
	int frame_toremove; 
	int frame_toremove_bits;

	page_table_get_entry(pt, pageno_to_remove, &frame_toremove, &frame_toremove_bits ); // info from page table 


	// if dirty i.e if it has write access, then have to write this page back in disk and then replace the page
	if ( (frame_toremove_bits&PROT_WRITE)!=0 )
	{
		disk_write( disk,pageno_to_remove, &physmem[(frame_toremove)*PAGE_SIZE] ); // write back page to disk
		diskWrites++;	
	}

	page_table_set_entry( pt, pageno_to_remove, 0, 0); // 0's invalidate frame entry of previous page

	page_table_set_entry( pt, page, (frame_toremove), 0|PROT_READ ); // set new page table entry with read permission

	disk_read( disk, page, &physmem[(frame_toremove)*PAGE_SIZE] ); // Read data from disk at virtual address given by 'page' to physical memory frame
	diskReads++;


	// Store info that this page is held in which age frame.
	// this frame holds this page, inverse of page table.
	frame_holds_what[(frame_toremove)] = page; // the frame now contains this page.
}


/*
	This function implements     ////////////////////  when a page fault occurs
	Algorithm: 
*/ 
void custom_pra( struct page_table *pt, int page )
{

}



/*
	This function tries to find a free frame from the available frames

	INPUT: Structure storing the occupation information of all frames, total frames
	OUTPUT: Position of free frame if a free frame is found. Otherwise -1.
*/
int findnset_free_frame(int *is_frame_occup_struc, int nframes)
{
    int i;
    
	for (i=0;i<nframes;i++)
	{
		if (is_frame_occup_struc[i] == 0) 	// if a free frame is found
		{
			is_frame_occup_struc[i] = 1; // mark that frame as occupied for the new incoming page 
			return i;					// return the position of frame
		}
	}

	// if no free frame is found, the whole page table is full, return -1
	return -1;
}
