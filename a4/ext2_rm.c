#include "ext2.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>

#define MAX_DIR_NAME 256
#define MAX_PATH_LEN 1024

unsigned char * disk;
struct ext2_super_block sb;
struct ext2_group_desc *gd;
char *inode_tbl;



struct ext2_inode * get_inode(unsigned int index);
struct ext2_dir_entry * search_entry_in_inode(const struct ext2_inode * inode, const char * entry_name);
int remove_inode(struct ext2_inode * parent, struct ext2_inode * target, unsigned int tfile_index, char* name);


unsigned char* get_block(unsigned int i) {return (disk+(i)*EXT2_BLOCK_SIZE);}
struct ext2_inode *get_inode(unsigned int i){
  if (i == 0) { return NULL; }
  inode_tbl = (char *)(disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
  return (struct ext2_inode *)(inode_tbl + sizeof(struct ext2_inode) * (i - 1));
}

// search enrty in block given name
struct ext2_dir_entry * find_entry_in_block(const unsigned char * block_ptr, const char * entry_name) {
	const unsigned char * ptr = block_ptr;
	while(ptr < block_ptr + EXT2_BLOCK_SIZE){
		struct ext2_dir_entry *entry = (struct ext2_dir_entry *)ptr;
		int name_len = entry->name_len;
		int same_len = strlen(entry_name) == name_len;
		int str_cmp = strncmp(entry->name, entry_name, name_len) == 0;
		if (same_len && str_cmp){ return entry; }
		ptr += entry->rec_len;
	} return NULL;
}

//get entry given inode and entry name
struct ext2_dir_entry * search_entry_in_inode(const struct ext2_inode * inode, const char * query_entry) {
	unsigned int block_len = (EXT2_BLOCK_SIZE / sizeof (unsigned int));
	int i=0;
	while (i<14) {
		unsigned int ib = inode->i_block[i];
		if (ib) {
			unsigned char * block = get_block(ib);
			if (i<12) {
				struct ext2_dir_entry * entry = find_entry_in_block(block, query_entry);
				if (entry != NULL) {
					int name_len = entry->name_len;
					int name_cmp = strncmp(entry->name,query_entry, name_len) == 0;
					if (name_cmp){ return entry; }}
			} else if(i==12){
				unsigned int * block_ptrs = (unsigned int *)(block);
				for(int j = 0; j < block_len; j++ ){
					unsigned char * block = get_block(block_ptrs[j]);
					struct ext2_dir_entry * entry = find_entry_in_block(block, query_entry);
					if (entry != NULL) {
						int name_len = entry->name_len;
						int name_cmp = strncmp(entry->name,query_entry, name_len) == 0;
						if (name_cmp) { return entry; }}}
			}
    }
		i++;
	}
	return NULL;
}

// given path get inode index
unsigned int find_inode_index_by_path(const char *disk_path, unsigned short mode){
  unsigned inode_index = EXT2_ROOT_INO;
  char token[256];
  memset(token, '\0', 256);
  int len = strlen(disk_path);
  int path_index = 0;
  while (path_index < len){
    int dn_index = 0;
    while (path_index < len){
      if (disk_path[path_index] == '/') { break; }
      token[dn_index] = disk_path[path_index];
      path_index += 1; dn_index += 1;
    }
    if (strlen(token) > 0){
      struct ext2_inode *inode = get_inode(inode_index);
      struct ext2_dir_entry *dir_entry = search_entry_in_inode(inode, token);
			if ((inode->i_mode & EXT2_S_IFDIR) == 0) {return 0;}
      if (dir_entry){ inode_index = dir_entry->inode; }
      else {return 0;}
    }
		memset(token, '\0', 256);
    path_index++;
  }
	struct ext2_inode *inode = get_inode(inode_index);
	if ((inode->i_mode & mode) == 0) {return 0;}
  return inode_index;
}


void split_path(const char * path, char * new_dir, char * parent_dir) {
	int start = 0; int index = 0;
	int new_dir_name_len = 0;
	while (index < strlen(path)) {
		if (path[index] != '/') {
			start = index;
			new_dir_name_len = 0;
			while(path[index] != '/' && index < strlen(path)) {
				new_dir_name_len += 1;
				index += 1;
			}
		} else { index += 1; }
	}
	strncpy(new_dir, path + start, new_dir_name_len);
	strncpy(parent_dir, path, start);
	new_dir[new_dir_name_len] = '\0';
	parent_dir[start] = '\0';
}


void inode_block_bitmap_undo(unsigned int * inode_bitmap, unsigned int * block_bitmap, int inode_index,int block_index) {
  inode_bitmap += (inode_index / 32);
  *inode_bitmap &= (unsigned int)( ~(1 << (inode_index % 32)) );

	block_bitmap += (block_index / 32);
  *block_bitmap &= (unsigned int)( ~(1 << (block_index % 32)) );
}

int update_target(struct ext2_inode * target, unsigned int target_fn_index, int i) {
	target->i_links_count -= 1;
	int c = 0;
	if (target->i_links_count == 0) {
		unsigned int * inode_bitmap = (unsigned int *)get_block(gd->bg_inode_bitmap);
		unsigned int * block_bitmap = (unsigned int *)get_block(gd->bg_block_bitmap);
		inode_block_bitmap_undo(inode_bitmap,block_bitmap,target_fn_index - 1,target->i_block[i] - 1);
		//target->i_blocks = 0;
		//target->i_size = 0;
		for (int i = 0; i < 12 && target->i_block[i]; i++) { c++; }
		//memset(target->i_block, 0, 12);
	}
	return c;
}



int remove_inode(struct ext2_inode* parent_inode, struct ext2_inode* target_inode, unsigned int target_fn_index, char* name){
    int i, ptr, free_block = 0;
    for (i = 0; i < 12; i++) {
			unsigned int ib = parent_inode->i_block[i];
			struct ext2_dir_entry* prev_entry;
			if (ib) {
        struct ext2_dir_entry* entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * ib);
        while (ptr < EXT2_BLOCK_SIZE) {
						entry = (struct ext2_dir_entry*)((char *)entry + entry->rec_len);
            //printf("entry name: %s\n", entry->name);
						//entry = (struct ext2_dir_entry*)(disk + EXT2_BLOCK_SIZE*parent_inode->i_block[i] + ptr);
            if ((entry->inode == target_fn_index) && (strncmp(name, entry->name, entry->name_len) == 0) && (strlen(name)==entry->name_len)) {
								if (((unsigned int) entry->file_type != EXT2_FT_SYMLINK) && ((unsigned int) entry->file_type != EXT2_FT_REG_FILE)) {return EISDIR;}
                else {
                  int count = update_target(target_inode, target_fn_index,i);
									target_inode->i_dtime = (unsigned int)time(NULL);
                  free_block += count;
                  gd->bg_free_blocks_count += free_block;
                  gd->bg_free_inodes_count += 1;
                  prev_entry->rec_len = entry->rec_len + prev_entry->rec_len;
                  return 0;
                }
            }
						ptr += entry->rec_len;
            prev_entry = entry;
        } ptr = 0;
			}
    }
    return 0;
}
/***************************************************************************/
int main(int argc, char **argv) {
	char * usage = "Incorrect Format. Usage: ext2_rm <image file name> <target path>\n";
	if(argc != 3) { fprintf(stderr, "%s", usage); exit(1); }

	int fd = open(argv[1], O_RDWR);
	disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(disk == MAP_FAILED) { perror("mmap"); return -1; }
	sb = *(struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
	gd = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);

	int begin_with_slash = strncmp(argv[2], "/", 1) == 0;
	int empty_name = strlen(argv[2]) < 2;
	if (!begin_with_slash) { printf("Invalid Path: Path not begin with '/'.\n"); return ENOENT; }
  if (empty_name) { printf("Invalid directory name: Home directory.\n"); return ENOENT; }

	char *path_on_disk = (char *) malloc(strlen(argv[2]));
  strcpy(path_on_disk, argv[2]);

	char path_to_target[MAX_PATH_LEN];
  char target_fn_to_rm[MAX_DIR_NAME];
  split_path(path_on_disk,target_fn_to_rm, path_to_target);

	unsigned int parent_dir_path_index, target_fn_index;
	parent_dir_path_index = find_inode_index_by_path(path_to_target, EXT2_S_IFDIR);
	if (!parent_dir_path_index) {printf("rm: %s: Parent directory does not exist.\n", path_to_target); return ENOENT;}
	struct ext2_inode* parent_inode = get_inode(parent_dir_path_index);
	int parent_is_dir = (parent_inode->i_mode & EXT2_S_IFDIR);
	if (!parent_is_dir) {printf("mkdir: %s: Parent directory does not exist.\n", path_to_target); return ENOENT;}
  target_fn_index = find_inode_index_by_path(path_on_disk, EXT2_S_IFREG);
	if (target_fn_index == 0) {printf("rm: %s: No regular file with this name found\n", target_fn_to_rm); return ENOENT;}
	struct ext2_inode* target_inode = get_inode(target_fn_index);
	free(path_on_disk);

  //printf("%s\n", target_fn_to_rm);
  remove_inode(parent_inode, target_inode, target_fn_index, target_fn_to_rm);

  return 0;
}
