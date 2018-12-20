#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include <errno.h>
#include <string.h>

struct ext2_super_block *super_b;
struct ext2_group_desc *group_d;
unsigned char *b_map;
struct ext2_inode *i_start;

unsigned char *disk;

struct ext2_inode * get_inode(int index){
    i_start = (struct ext2_inode *)(disk + (group_d->bg_inode_table * EXT2_BLOCK_SIZE));
    struct ext2_inode *this_inode = i_start + index - 1;
    return this_inode;
}


int allocate_inode(void) {
    int free_inodes = group_d->bg_free_inodes_count;
    int i_count = super_b->s_inodes_count;
    if (free_inodes != 0){
        int i = 0;
        int j = 0;
        int i_index = 0;
        int byte = i_count/8;
        while(i<byte){
            unsigned char * i_map = (unsigned char *)(disk + (group_d->bg_inode_bitmap * EXT2_BLOCK_SIZE)) + i;
            while (j < 8){
                i_index++;
                if (!((*i_map)>>j & 1)){
                    group_d->bg_free_inodes_count--;
                    super_b->s_free_inodes_count--;
                    *i_map = *i_map | (1 << i);
                    return i_index;
                }
                j++;
            }
            i++;
        }
        return -1;
    }
    return -1;
}


int allocate_block(void) {
    int free_blocks = group_d->bg_free_blocks_count;
    int b_count = super_b->s_blocks_count;
    if (free_blocks != 0){
        int i = 0;
        int j = 0;
        int b_index = 0;
        int byte = b_count/8;
        while(i<byte){
            unsigned char * b_map = b_map = (unsigned char *)(disk + (group_d->bg_block_bitmap * EXT2_BLOCK_SIZE))+ i;
            while (j < 8){
                b_index++;
                if (!((*b_map)>>j & 1)){
                    group_d->bg_free_inodes_count--;
                    super_b->s_free_inodes_count--;
                    *b_map = *b_map | (1 << i);
                    return b_index;
                }
                j++;
            }
            i++;
        }

        return -1;
    }
    return -1;
}

void build_entry (struct ext2_dir_entry * entry ,unsigned int i_num, int name_len, int rec_len, unsigned char type,char *name){
    struct ext2_inode *self_inode = get_inode(i_num);
    self_inode->i_links_count++;
    entry->name_len     = name_len;
    memcpy(entry->name, name, entry->name_len);
    int padding_len = 4 - (entry->name_len % 4);
    memset(entry->name + entry->name_len, '\0', padding_len);
    entry->rec_len      = rec_len;
    entry->file_type    = type;
    entry->inode        = i_num;
    return;
}

void update_rec (struct ext2_dir_entry * entry){
    
    int last_len = sizeof(struct ext2_dir_entry) + entry->name_len;
    if (last_len % 4) {last_len += 4 - (last_len % 4);}
    entry->rec_len = last_len;
    return;

}


int last_entry (struct ext2_inode *p_inode,struct ext2_dir_entry * entry,int e_offset, char *name , int last_block) {
    
    int ent_rem = entry->rec_len - sizeof(struct ext2_dir_entry) - entry->name_len;
    int name_rem = sizeof(struct ext2_dir_entry) + strlen(name);
    if (ent_rem < name_rem) {
        p_inode->i_blocks += 2;
        int new_block_number = allocate_block();
        if (new_block_number < 0) {exit(ENOSPC);}
        p_inode->i_block[last_block + 1] = new_block_number;
        return new_block_number;
    }
    return -1;
}


void add_entry(struct ext2_inode *p_inode, unsigned int i_num, char *t_name){
    
    int rec_len = 0;
    int e_offset = 0;
    int last_block = (p_inode->i_blocks / 2) - 1;
    int name_len = strlen(t_name);
    struct ext2_dir_entry *entry;
    
    for (;;) {
        entry = (struct ext2_dir_entry *)((disk + EXT2_BLOCK_SIZE * p_inode->i_block[last_block]) + e_offset);
        int mem_total = entry->rec_len + e_offset;
        if (mem_total == EXT2_BLOCK_SIZE) {
            int last_e = last_entry(p_inode,entry,e_offset,t_name,last_block);
            if (last_e != -1){
                entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * last_e);
                build_entry(entry,i_num, name_len,EXT2_BLOCK_SIZE,EXT2_FT_REG_FILE,t_name);
            }
            break;
        }
        e_offset += entry->rec_len;
    }
    
    update_rec(entry);
    e_offset += entry->rec_len;
    entry = (struct ext2_dir_entry *)((disk + EXT2_BLOCK_SIZE * p_inode->i_block[last_block]) + e_offset);
    rec_len = EXT2_BLOCK_SIZE - e_offset;
    build_entry(entry,i_num, name_len,rec_len,EXT2_FT_REG_FILE,t_name);
}



int *build_block_array(int s_size){

    int a_size = s_size/EXT2_BLOCK_SIZE;
    if (s_size%EXT2_BLOCK_SIZE) {a_size++;}
    int *block_array = malloc(sizeof(int) * a_size);
    return block_array;
    
}

void copy_data(unsigned char *s_file, int *b_array,int b_array_size){
    int i = 0;
    while (i < b_array_size) {
        unsigned char *src = s_file + EXT2_BLOCK_SIZE * i;
        unsigned char *dst = disk + EXT2_BLOCK_SIZE * b_array[i];
        memcpy(dst, src, EXT2_BLOCK_SIZE);
        i++;
    }
}

void array_to_block(struct ext2_inode *f_inode, int *f_array, int f_array_size){
    
    int i=0;
    int j=0;
    while (i < 12) {
        f_inode->i_block[i] = f_array[i];
        j++;
        if (j== f_array_size) {return;}
        i++;
    }
    int indirect_block_num = allocate_block();
    if (indirect_block_num < 0) {exit(ENOSPC);}
    unsigned int *indirect_block = (unsigned int *)(disk + EXT2_BLOCK_SIZE * f_inode->i_block[12]);
    int b_size = EXT2_BLOCK_SIZE/4;
    int k = 0;
    f_inode->i_block[12] = indirect_block_num;
    f_inode->i_blocks += 2;
    while (k < b_size){
        j++;
        indirect_block[i] = f_array[12 + k];
        if (j== f_array_size) {return;}
        k ++;
    }
}


int do_copy(struct ext2_inode *f_inode, unsigned char *s_file, int s_size)
{
    
    int *b_array = build_block_array(s_size);
    int b_array_size = sizeof(b_array)/sizeof(int);
    int i =0;
    
    while (i < b_array_size){
        b_array[i] = allocate_block();
        if (b_array[i] < 0) {return ENOSPC;}
        i ++;
    }

    f_inode->i_blocks = b_array_size * 2;
    
    array_to_block(f_inode, b_array, b_array_size);

    copy_data(s_file,b_array,b_array_size);
    
    free(b_array);
    
    return 0;
}


int get_inode_with_name(int p_num, char *t_name){
    
    struct ext2_dir_entry *dir_entry;
    struct ext2_inode *in_which_inode = get_inode(p_num);
    int mem_total;
    int count = 0;
    
    while (count< 15) {
        if (in_which_inode ->i_block[count]!=0){
            mem_total = 0;
            while (mem_total < EXT2_BLOCK_SIZE) {
                dir_entry = (struct ext2_dir_entry *)(EXT2_BLOCK_SIZE*in_which_inode->i_block[count]+mem_total+ disk);
                if (strncmp(dir_entry->name, t_name, dir_entry->name_len) == 0) {
                    return dir_entry->inode;
                }
                mem_total += dir_entry->rec_len;
            }
        }
        count ++;
    }
    return -1;
}


int get_inode_with_path(char *path){
    
    char t_path[1024];
    int path_len = strlen(path);
    if (strlen(path) == 1 && path[0] == '/'){return EXT2_ROOT_INO ;}
    strcpy(t_path, path);
    t_path[path_len + 1] = '\0';
    unsigned int num = 0;
    if (path_len == 1){ if (t_path[0] == '/'){return num; } }
    if (path_len != 1){
        char* temp = strtok(t_path, "/");
        while (temp){
            num = get_inode_with_name(2, temp);
            temp = strtok(NULL, "/");
            if (!num){ return -1; }
        }
    }
    
    return num;
}




int main(int argc, const char * argv[]) {
    
    
    if(argc != 4) {
        fprintf(stderr, "Usage: <image file name> <src> <dst>\n");
        exit(1);
    }
    
    int image_file_name = open(argv[1], O_RDWR);
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, image_file_name, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    
    int path_to_source_file = open(argv[2], O_RDWR);
    struct stat source;
    if (lstat(argv[2], &source) == -1) {return ENOENT;}
    
    int s_size = (int)source.st_size;
    unsigned char *source_file = mmap(NULL, s_size, PROT_READ | PROT_WRITE, MAP_SHARED, path_to_source_file, 0);
    if(source_file == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }


    super_b = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    group_d = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE + sizeof(struct ext2_super_block));



    char t_path[1024];
    char t_name[EXT2_NAME_LEN];
    char t_parent[1024];
    strcpy(t_path, argv[3]);
    int path_len = strlen(t_path);
    
    if (t_path[path_len - 1] == '/') {
        t_path[path_len - 1] = '\0';
    }
    int i = 0;
    int j = 0;
    while (i < path_len){
        if (t_path[i] == '/') {
            j = i;
        }
        i++;
    }
    if (j == 0) {
        t_parent[0] = '/';
        t_parent[1] = '\0';
    }
    if (j != 0){
        memcpy(t_parent, t_path, j);
        t_parent[j] = '\0';
    }
    strncpy(t_name, t_path + j + 1, EXT2_NAME_LEN);

    int p_num = get_inode_with_path(t_parent);
    if (p_num < 0) {return ENOENT;}
    if (get_inode_with_name(p_num, t_name) > 0) {return EEXIST;}

    struct ext2_inode *t_parent_inode = get_inode(p_num);
    int allocate_inodes = allocate_inode();
    if (allocate_inodes){
        struct ext2_inode *allocated_inode = get_inode(allocate_inodes);
        allocated_inode->i_mode           = 0x0000 | EXT2_S_IFREG;
        allocated_inode->i_uid            = 0;
        allocated_inode->i_size           = s_size;
        allocated_inode->i_ctime          = 0;
        allocated_inode->i_mtime          = 0;
        allocated_inode->i_dtime          = 0;
        allocated_inode->i_gid            = 0;
        allocated_inode->i_links_count    = 0;
        allocated_inode->i_blocks         = 0;
        allocated_inode->i_flags          = 0;
        allocated_inode->osd1             = 0;
        allocated_inode->i_generation     = 0;
        allocated_inode->i_file_acl       = 0;
        allocated_inode->i_dir_acl        = 0;
        memset(allocated_inode->extra, 0, 3);
    }
    
    struct ext2_inode *f_inode = get_inode(allocate_inodes);
    do_copy(f_inode, source_file, s_size);
    add_entry(t_parent_inode, allocate_inodes, t_name);
    
}
