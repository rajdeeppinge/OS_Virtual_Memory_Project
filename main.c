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


void page_fault_handler( struct page_table *pt, int page )
{
	printf("page fault on page #%d\n",page);

	
	int bits, mapframe;

	// get entry in the page table	
	page_table_get_entry( pt, page, &mapframe, &bits );


	// FAULT TYPE 1 - page not in virtual memory i.e. no protection bits set i.e. entry in page table is free and 
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

	else	// FAULT 3
	{
	
	}

// code for testing
/*	page_table_print(pt);
	printf("------------------------------------------------\n");
*/
	
//	exit(1);
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
	int nframes = atoi(argv[2]);
	const char *program = argv[4];

	// try to create a disk
	struct disk *disk = disk_open("myvirtualdisk",npages);
	
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

	printf("Page table created\n");

	// get pointer to virtual memory from the page table
	char *virtmem = page_table_get_virtmem(pt);

	// get pointer to physical memory from the page table
	char *physmem = page_table_get_physmem(pt);


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

	// clean used resources
	page_table_delete(pt);
	disk_close(disk);

	return 0;
}
