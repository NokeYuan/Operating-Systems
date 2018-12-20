#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include "ext2.h"



unsigned char* disk;
struct ext2_super_block *sb;
struct ext2_group_desc *gd;
struct ext2_inode *inode_tbl;


/***********************************************************************************/
int bitmap_unused(unsigned char *bitmap, int i) {return (bitmap[i/8] & (1 << (i%8))) >> (i%8);}
int entry_size(int l) {if ((8+l)%4) {return ((8+l)/4+1)*4;} else {return 8+l;}}
unsigned char* get_block(unsigned int i) {return (disk+(i)*EXT2_BLOCK_SIZE);}

struct ext2_inode *get_inode(unsigned int i){
  if (i == 0) { return NULL; }
  char *inode_tbl = (char *)(disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
  return (struct ext2_inode *)(inode_tbl + sizeof(struct ext2_inode) * (i - 1));
}


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
      if ((inode->i_mode & EXT2_S_IFDIR) == 0) {return 0;}
      struct ext2_dir_entry *dir_entry = search_entry_in_inode(inode, token);
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


/***********************************************************************************/

void inode_bitmap_alter(unsigned int i, unsigned char *inode_bitmap){
	int division = i/(sizeof(char*));
	int modulus = i%(sizeof(char*));
	unsigned char* byte = inode_bitmap+division;
	*byte |= 1 << modulus;
  gd->bg_free_inodes_count -= 1;
  sb->s_free_inodes_count -= 1;
}

void block_bitmap_alter(unsigned int i, unsigned char *block_bitmap){
	int division = i/(sizeof(char*));
	int modulus = i%(sizeof(char*));
	unsigned char* byte = block_bitmap + division;
	*byte |= 1 << modulus;
  gd->bg_free_blocks_count -= 1;
  sb->s_free_blocks_count -= 1;
}

void restore_update(int target,unsigned char * bitmap, char flag) {
  //change_bitmap(bitmap, target);
  if (flag == 'b') {block_bitmap_alter(target-1, bitmap);}
  if (flag == 'i') {inode_bitmap_alter(target-1, bitmap);}
}

struct curr_prev_entry {
  struct ext2_dir_entry* curr_entry;
  struct ext2_dir_entry* prev_entry;
};

struct curr_prev_entry* find_pair(struct ext2_inode* curr_inode, const char* fn_rstr) {
  for(int i=0; i<12; i++) {
    unsigned int ib=curr_inode->i_block[i];
    if (ib) {
      struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(get_block(ib));
      int ptr = 0;
      while (ptr < EXT2_BLOCK_SIZE) {
        entry = (struct ext2_dir_entry *)((char *)entry + entry->rec_len);
        struct ext2_dir_entry * del_entry = entry;
        int sum_entry_len = entry_size(del_entry->name_len);
        ptr += entry->rec_len;
        while (sum_entry_len < entry->rec_len) {
          del_entry = (struct ext2_dir_entry *)((char *)del_entry + entry_size(del_entry->name_len));
          if (del_entry->name_len==strlen(fn_rstr) && strncmp(del_entry->name,fn_rstr,strlen(fn_rstr))==0) {
            if (del_entry->file_type == EXT2_FT_DIR) {continue;}
            struct ext2_dir_entry* prev_entry = entry;
            //struct ext2_dir_entry* deleted_entry = deleted_dir;
            del_entry->rec_len = prev_entry->rec_len - sum_entry_len;
            prev_entry->rec_len = sum_entry_len;
            struct curr_prev_entry* pair= malloc(sizeof(struct curr_prev_entry*));
            pair->curr_entry = del_entry; pair->prev_entry = prev_entry;
            return pair;
          }
          sum_entry_len += entry_size(del_entry->name_len);
        }
      } ptr = 0;
  }} return NULL;
}

int check_block(unsigned int* block,unsigned char *bitmap,int i) {return block[i] && bitmap_unused(bitmap, block[i]-1);}
int main(int argc, char **argv) {
  int fd = open(argv[1], O_RDWR);
  disk = mmap(NULL, 128 * EXT2_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(disk == MAP_FAILED) {perror("mmap");exit(1);}
  sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
  gd = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);

  unsigned char *block_bitmap = (unsigned char *)(get_block(gd->bg_block_bitmap));
  unsigned char *inode_bitmap = (unsigned char *)(get_block(gd->bg_inode_bitmap));
  inode_tbl = (struct ext2_inode *)(get_block(gd->bg_inode_table));

  char *path_on_disk = (char *) malloc(strlen(argv[2]));
  strcpy(path_on_disk, argv[2]);

  char parent_path[1024];
  char *fn_to_restored = (char *) malloc(1024);
  split_path(path_on_disk, fn_to_restored, parent_path);

  int parent_inode_id = find_inode_index_by_path(parent_path, EXT2_S_IFDIR);
  //printf("fn_to_restored: %s\n", fn_to_restored);
  if (!parent_inode_id) {printf("restore: %s: Parent directory does not exist.\n", parent_path); return ENOENT;}
  int parent_is_dir = (get_inode(parent_inode_id)->i_mode & EXT2_S_IFDIR);
  if (!parent_is_dir) {printf("restore: %s: Parent directory does not exist.\n", parent_path); return ENOENT;}

  struct ext2_inode* curr_inode = (struct ext2_inode*)(get_inode(parent_inode_id));
  char * inode_taken = "The inode of the file has been used.\n";
  char * block_taken = "The blocks of the file has been used.\n";

  struct curr_prev_entry* entry_pair = find_pair(curr_inode, fn_to_restored);
  if (entry_pair == NULL) {fprintf(stderr, "Fail to restore due to entry no found\n"); return ENOENT;}
  //printf("not here\n");
  struct ext2_dir_entry* del_entry=entry_pair->curr_entry;
  //struct ext2_dir_entry* prev_entry=entry_pair->prev_entry;

  if (!entry_pair) {fprintf(stderr, "Fail to restore due to entry no found\n"); return ENOENT;}
  int del_inode_index = del_entry->inode;
  //printf("%d\n", del_inode_index);
  if (bitmap_unused(inode_bitmap, del_inode_index-1)) {fprintf(stderr,"%s",inode_taken);return ENOENT;}
  struct ext2_inode* rm_inode= get_inode(del_inode_index);

  for (int i=0; i<12; i++) {if(check_block(rm_inode->i_block, block_bitmap, i)) {fprintf(stderr,"%s",block_taken);return ENOENT;}}
  if (rm_inode->i_block[12] != 0) {
    if(bitmap_unused(block_bitmap, rm_inode->i_block[12]-1)) {fprintf(stderr,"%s",block_taken);return ENOENT;}
    unsigned int * indirect_block = (unsigned int *)get_block(rm_inode->i_block[12]);
    for(int i=0;i<sb->s_blocks_count;i++) {if(check_block(indirect_block, block_bitmap, i)) {fprintf(stderr,"%s",block_taken);return ENOENT;}}
  }

  restore_update(del_inode_index, inode_bitmap, 'i');
  (get_inode(del_inode_index))->i_dtime = 0;
  (get_inode(del_inode_index))->i_links_count++;

  for (int i=0;i<12;i++) {if (rm_inode->i_block[i]) {restore_update(rm_inode->i_block[i], block_bitmap,'b');}}
  if (rm_inode->i_block[12] != 0) {
    restore_update(rm_inode->i_block[12], block_bitmap, 'b');
    int * indirect_block = (int *)get_block(rm_inode->i_block[12]);
    for (int i=0; i<sb->s_blocks_count; i++) {if (indirect_block[i]) {restore_update(indirect_block[i], block_bitmap, 'b');}}
  }
  return 0;
}
