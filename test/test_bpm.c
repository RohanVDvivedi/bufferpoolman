#include<stdio.h>
#include<stdlib.h>

#include<string.h>

#include<buffer_pool_manager.h>

#define BLOCKS_PER_PAGE 1

int main()
{
	printf("\n\ntest started\n\n");

	bufferpool* bpm = get_bufferpool("./test.db", 1, BLOCKS_PER_PAGE);

	printf("Bufferpool built\n\n");

	void* page_mem = NULL;

	page_mem = get_page_to_write(bpm, 0);
	printf("page 0 locked for write\n");
	char* string_temp = "Hello World, This is page number 0";
	memcpy(page_mem, string_temp, strlen(string_temp) + 1);
	printf("page 0 write done\n");
	release_page_write(bpm, 0);
	printf("page 0 released from write lock\n\n");

	page_mem = get_page_to_read(bpm, 0);
	printf("page 0 locked for read\n");
	printf("Data : \t <%s>\n", (char*)page_mem);
	printf("page 0 read done\n");
	release_page_read(bpm, 0);
	printf("page 0 released from read lock\n\n");

	delete_bufferpool(bpm);

	printf("Buffer pool deleted\n\n");

	printf("test completed\n\n\n");

	return 0;
}