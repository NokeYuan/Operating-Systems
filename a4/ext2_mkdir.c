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

#define MAX_DIR_NAME 256
#define MAX_PATH_LEN 1024

unsigned char *disk;
struct ext2_super_block *sb;
struct ext2_group_desc *gd;
char *inode_tbl;

unsigned int find_inode_index_by_path(const char *disk_path, unsigned short mode);
struct ext2_inode *get_inode(unsigned int inode_index);
struct ext2_dir_entry *search_entry_in_inode(const struct ext2_inode *inode, const char *dir_entry_name);
struct ext2_dir_entry *find_entry_in_block(const unsigned char *data_block, const char *dir_entry_name);
unsigned int initial_inode();
unsigned int initial_block();
void split_path(const char * path, char * new_dir, char * parent_dir);
int setup_inode(struct ext2_inode *new_dir_inode, unsigned short mode, unsigned int size);

unsigned char* get_block(unsigned int i) {return (disk+(i)*EXT2_BLOCK_SIZE);}
struct ext2_inode *get_inode(unsigned int i){
  if (i == 0) { return NULL; }
  inode_tbl = (char *)(disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
  return (struct ext2_inode *)(inode_tbl + sizeof(struct ext2_inode) * (i - 1));
}

/***********************************************************************************/
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

/***********************************************************************************/
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

/***********************************************************************************/
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
	unsigned char* byte = block_bitmap+division;
	*byte |= 1 << modulus;
  gd->bg_free_blocks_count -= 1;
  sb->s_free_blocks_count -= 1;
}

/***********************************************************************************/
unsigned int initial_inode(){
  unsigned char *inode_bitmap = (unsigned char*)(disk + EXT2_BLOCK_SIZE * gd->bg_inode_bitmap);
  unsigned char *inode_ptr = inode_bitmap;
	int free_inode=0;
	int inode_index=1;
  //printf("free inode count: %d\n", sb->s_free_inodes_count);
  while (free_inode < sb->s_free_inodes_count){
    unsigned char bitmap_section = *inode_ptr;
    int j=0;
    while (j<8) {
      if ((inode_index >= 11) & (!((bitmap_section>>j) & 1))){
        inode_bitmap_alter(inode_index-1, inode_bitmap);
				return inode_index;
			} j++;
			inode_index += 1;
    } free_inode += 1; inode_ptr += 1;
  }
  return 0;
}

/***********************************************************************************/
unsigned int initial_block(){
  unsigned char *block_bitmap = (unsigned char*)(disk + EXT2_BLOCK_SIZE * gd->bg_block_bitmap);
	unsigned char *block_ptr = block_bitmap;
	int free_block=0;
	int block_index=1;
	int section_num = (sb->s_blocks_count)/sizeof(char*);
	while (free_block < section_num ){
		unsigned char bitmap_section = *block_ptr;
    int j=0;
		while (j<8) {
			if(!((bitmap_section>>j) & 1)){
        block_bitmap_alter(block_index -1, block_bitmap);
				return block_index;
			} j++;
			block_index += 1;
		} free_block +=1 ; block_ptr+=1;
	}
	return 0;
}

/***********************************************************************************/
int entry_size(int l) {if ((8+l)%4) {return ((8+l)/4+1)*4;} else {return 8+l;}}
unsigned char * append_dir_entry(struct ext2_dir_entry* new_dir_entry, struct ext2_inode *inode, char *new_dir_name){
    unsigned int i=0;
    unsigned char *ptr;
    int ENTRY_SIZE = sizeof(struct ext2_dir_entry);
    for (i=0; i<12; i++) {
      if (inode->i_block[i]){
        ptr = get_block(inode->i_block[i]);
        unsigned char *block_end = get_block(inode->i_block[i]) + EXT2_BLOCK_SIZE;
        struct ext2_dir_entry *entry;

        while (ptr < block_end) {
          entry = (struct ext2_dir_entry *)ptr;
          int real_size = entry_size(entry->name_len);
          int exceed = ptr + entry->rec_len >= block_end;
          int inrange = ptr + real_size + ENTRY_SIZE < block_end;
          if (exceed && inrange) {
            entry->rec_len = real_size;
            ptr += entry->rec_len;
            break;
          } ptr += entry->rec_len;
        }
        if (ptr == block_end) {continue;}
        else if (ptr != block_end){
          new_dir_entry->rec_len = block_end - ptr;
          //printf("rec len entry in add entry:%hu\n", new_dir_entry.rec_len);
          unsigned char * curr_pos = ptr + ENTRY_SIZE;
          memcpy(curr_pos, new_dir_name, new_dir_entry->name_len);
          struct ext2_dir_entry * allocate_pos = (struct ext2_dir_entry *) ptr;
          *allocate_pos = *new_dir_entry;
        }
        break;
      } else {
        inode->i_block[i] = initial_block();
        new_dir_entry->rec_len = EXT2_BLOCK_SIZE;
        //printf("rec len entry in add entry:%hu\n", new_dir_entry.rec_len);
        unsigned char *ptr = disk + (inode->i_block[i] * EXT2_BLOCK_SIZE);
        memcpy(ptr + ENTRY_SIZE, new_dir_name, new_dir_entry->name_len);
        struct ext2_dir_entry * allocate_pos = (struct ext2_dir_entry *) ptr;
        *allocate_pos = *new_dir_entry;
        break;
      }
    }
    //inode->i_links_count++;
    return ptr;
}

/***********************************************************************************/

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

int setup_inode(struct ext2_inode *new_dir_inode, unsigned short mode, unsigned int size){
	new_dir_inode->i_mode = mode;
  new_dir_inode->i_size = size;
  new_dir_inode->i_ctime = 0;
  new_dir_inode->i_mtime = 0;
	new_dir_inode->i_uid = 0;
	new_dir_inode->i_gid = 0;
	new_dir_inode->i_dtime = 0;
	new_dir_inode->osd1 = 0;
	new_dir_inode->i_links_count = 0;
	new_dir_inode->i_blocks = 2;
	new_dir_inode->i_generation = 0;
	new_dir_inode->i_file_acl = 0;
	new_dir_inode->i_dir_acl = 0;
	return 0;
}
/***********************************************************************************/

int main(int argc, char **argv) {
  char * usage = "Incorrect Format. Usage: ext2_mkdir <image file name> <file path>\n";
  if (argc != 3) { fprintf(stderr, "%s", usage); exit(1); }
  int fd = open(argv[1], O_RDWR);
	disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(disk == MAP_FAILED) { perror("mmap"); exit(1); }
  sb = (struct ext2_super_block *)(disk + 1024);
  gd = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);

  int begin_with_slash = strncmp(argv[2], "/", 1) == 0;
  int empty_name = strlen(argv[2]) < 2;
  if (!begin_with_slash) { printf("Invalid Path: Path not begin with '/'.\n"); return ENOENT; }
  if (empty_name) { printf("Invalid directory name: Home directory.\n"); return ENOENT; }

  char *path_on_disk = (char *) malloc(strlen(argv[2]));
  strcpy(path_on_disk, argv[2]);

  char parent_dir_path[MAX_PATH_LEN];
  char new_dir_name[MAX_DIR_NAME];
  split_path(path_on_disk,new_dir_name,parent_dir_path);
  free(path_on_disk);

  unsigned int parent_index = find_inode_index_by_path(parent_dir_path, EXT2_S_IFDIR);
  //printf("parent index: %d\n", parent_index);
  if (!parent_index) {printf("mkdir: %s: Parent directory does not exist.\n", parent_dir_path); return ENOENT;}
  int parent_is_dir = (get_inode(parent_index)->i_mode & EXT2_S_IFDIR);
  if (!parent_is_dir) {printf("mkdir: %s: Parent directory does not exist.\n", parent_dir_path); return ENOENT;}
  struct ext2_inode *parent_inode = get_inode(parent_index);
  if (search_entry_in_inode(parent_inode, new_dir_name)) {printf("mkdir: Directory or file name '%s' already exists.\n", new_dir_name); return EEXIST;}
  parent_inode->i_links_count++;
  unsigned int inode_index = initial_inode();
  if (inode_index<2) { printf("Inode table run out of space\n"); exit(1);}
  gd->bg_used_dirs_count ++;

  //printf("new inode index: %d\n", inode_index);
  struct ext2_inode *new_dir_inode = get_inode(inode_index);
  setup_inode(new_dir_inode, EXT2_S_IFDIR, EXT2_BLOCK_SIZE);

  new_dir_inode->i_size = EXT2_BLOCK_SIZE;
  new_dir_inode->i_mode = EXT2_S_IFDIR;
  new_dir_inode->i_links_count = 2;
  new_dir_inode->i_blocks = 2;

  memset(new_dir_inode->i_block, 0, 15);

  struct ext2_dir_entry* new_dir_entry = malloc(sizeof(struct ext2_dir_entry));
  struct ext2_dir_entry* current_level_dir = malloc(sizeof(struct ext2_dir_entry));
  struct ext2_dir_entry* parent_level_dir = malloc(sizeof(struct ext2_dir_entry));
  //unsigned char * ptr = NULL;

  new_dir_entry->name_len = strlen(new_dir_name);
  new_dir_entry->inode = inode_index;
  new_dir_entry->file_type = EXT2_FT_DIR;
  append_dir_entry(new_dir_entry, parent_inode, new_dir_name);
  //printf("rec len entry :%hu\n", ((struct ext2_dir_entry *)ptr)->rec_len);

  current_level_dir->name_len = 1;
  current_level_dir->inode = inode_index;
  current_level_dir->file_type = EXT2_FT_DIR;
  append_dir_entry(current_level_dir, new_dir_inode, ".");

  parent_level_dir->name_len = 2;
  parent_level_dir->inode = parent_index;
  //printf("(*parent_level_dir).name_len: %d\n", (*parent_level_dir).name_len);
  parent_level_dir->file_type = EXT2_FT_DIR;
  append_dir_entry(parent_level_dir, new_dir_inode, "..");

  return 0;
}
