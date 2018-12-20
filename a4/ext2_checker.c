#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

#include <limits.h>
#include <string.h>


#define END_OF_ARRAY -1

unsigned char *disk;

struct ext2_super_block *super_b;
struct ext2_group_desc *group_d;
unsigned char *block_bitmap;
unsigned char *inode_bitmap;


struct ext2_inode * get_inode(int index){
    //get inode by index
    struct ext2_inode *inode_table;
    inode_table = (struct ext2_inode *)(disk + (group_d->bg_inode_table * EXT2_BLOCK_SIZE));
    struct ext2_inode *this_inode = inode_table + index - 1;
    return this_inode;
}



void find_position (int i_num, int *byte_offset, int *bit_offset){
    //set byte and bit offset
    *byte_offset = i_num/8;
    *bit_offset = i_num%8;
    if (*bit_offset != 0){
        *byte_offset += 1;
    }
    if (*bit_offset == 0){
        *bit_offset = 8;
    }
}



void check_i_bitmap(int i_num ,int *total_fixed) {

    /***
     check that its inode is marked as allocated in the inode bitmap.
     If it isn't, then the inode bitmap must be updated to indicate
     that the inode is in use. You should also update the corresponding
     counters in the block group and superblock
     ***/
    int byte_offset;
    int bit_offset;

    find_position(i_num,&byte_offset,&bit_offset);

    if ((*(inode_bitmap + (byte_offset - 1)))>>(bit_offset - 1) & 1) {
        return;
    }else {
    *(inode_bitmap + (byte_offset - 1)) = *(inode_bitmap + (byte_offset - 1)) | (1<<(bit_offset - 1));
    group_d->bg_free_inodes_count--;
    super_b->s_free_inodes_count--;
    *total_fixed += 1;
    printf("Fixed: inode [%d] not marked as in-use\n", i_num);
    return;
    }
}


int *block_to_array(struct ext2_inode *inode) {
    int j = 0;
    int i = 0;
    int k = 0;
    int b_size = EXT2_BLOCK_SIZE/4;
    int *b_array = malloc(sizeof(int) * ((inode->i_blocks/2) + 1));
    unsigned int *indirect_block = (unsigned int *)(disk + EXT2_BLOCK_SIZE * inode->i_block[12]);

    while(i < 12){
        b_array[i] = inode->i_block[i];
        j++;
        if (j != (inode->i_blocks/2)) {i++;}
        else {
            b_array[i + 1] = END_OF_ARRAY;
            return b_array;
        }
    }
    j++;
    while(k < b_size) {
        b_array[12 + k] = indirect_block[k];
        j++;
        if (j != (inode->i_blocks/2)) {k++;}
        else{
            b_array[13 + k ] = END_OF_ARRAY;
            return b_array;
        }

    }

    free(b_array);
    return NULL;
}


void check_b_bitmap(int b_num ,int *total_fixed , int cur_i) {

    /***
     check that all its data blocks are allocated in the data bitmap.
     If any of its blocks is not allocated, you must fix this by
     updating the data bitmap. You should also update the corresponding
     counters in the block group and superblock
     ***/

    b_num++;
    int byte_offset;
    int bit_offset;
    find_position(b_num,&byte_offset,&bit_offset);
    if ((*(block_bitmap + (byte_offset - 1)))>>(bit_offset - 1) & 1) {
        return;
    }else {
        *(block_bitmap + (byte_offset - 1)) = *(block_bitmap + (byte_offset - 1)) | (1<<(bit_offset - 1));
        group_d->bg_free_inodes_count--;
        super_b->s_free_inodes_count--;
        *total_fixed += 1;
        printf("Fixed: %d in-use data blocks not marked in data bitmap for inode: [%d]\n", b_num, cur_i);
        return;
    }
}



int inode_count() {

    //count inode

    int i;
    int j = 0;
    int count = 0;

    while (j < (super_b->s_inodes_count/8)) {
        unsigned char *i_ptr = (unsigned char *)(disk + (group_d->bg_inode_bitmap * EXT2_BLOCK_SIZE)) + j;
        i = 0;
        while(i < 8) {
            if (!(*i_ptr>>i & 1)) {
                count++;
            }
            i++;
        }
        j++;
    }
    return count;
}


int block_count() {

    //count block

    int i;
    int j = 0;
    int count = 0;

    while (j < (super_b->s_blocks_count/8)) {
        unsigned char *b_ptr = (unsigned char *)(disk + (group_d->bg_block_bitmap * EXT2_BLOCK_SIZE)) + j;
        i = 0;
        while(i < 8) {
            if (!(*b_ptr>>i & 1)) {
                count++;
            }
            i++;
        }
        j++;
    }
    return count;
}


int check_i_mode ( int i_num, unsigned char *d_type,char *i_type){

    /***
     check if its inode's i_mode matches the directory entry file_type.
     If it does not, then you shall trust the inode's i_mode and fix
     the file_type to match.
     ***/


    struct ext2_inode *current = get_inode(i_num);
    int total_fixed = 0;

    if ((current->i_mode & EXT2_S_IFLNK) == EXT2_S_IFLNK) {
        if (*d_type != EXT2_FT_SYMLINK){
            *d_type = EXT2_FT_SYMLINK;
            total_fixed++;
            printf("Fixed: Entry type vs inode mismatch: inode [%d]\n", i_num);
        }
        *i_type = 's';
    } else if (current->i_mode & EXT2_S_IFREG) {
        if (*d_type != EXT2_FT_REG_FILE){
            *d_type = EXT2_FT_REG_FILE;
            total_fixed++;
            printf("Fixed: Entry type vs inode mismatch: inode [%d]\n", i_num);
        }
        *i_type = 'f';
    } else {
        if (*d_type != EXT2_FT_DIR){
            *d_type = EXT2_FT_DIR;
            total_fixed++;
            printf("Fixed: Entry type vs inode mismatch: inode [%d]\n", i_num);
        }
        *i_type = 'd';
    }
    return total_fixed;
}

void check_dtime(int i_num, int *total_fixed){

    /***
     check that its inode's i_dtime is set to 0. If it isn't,
     you must reset (to 0), to indicate that the file should
     not be marked for removal.
     ***/

    struct ext2_inode *current = get_inode(i_num);
    if (current->i_dtime == 0) {
        return;
    }else{
        current->i_dtime = 0;
        *total_fixed += 1;
        printf("Fixed: valid inode marked for deletion: [%d]\n", i_num);
        return;
    }
}



int inode_check(int i_num, int i_parent, unsigned char *d_type) {


    int total_fixed = 0;
    super_b = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    group_d = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE + sizeof(struct ext2_super_block));
    block_bitmap = (unsigned char *)(disk + (group_d->bg_block_bitmap * EXT2_BLOCK_SIZE));
    inode_bitmap = (unsigned char *)(disk + (group_d->bg_inode_bitmap * EXT2_BLOCK_SIZE));

    // b) check if its inode's i_mode matches the directory entry file_type. Add to total fixed
    char i_type;
    int fixed = check_i_mode(i_num, d_type, &i_type);
    total_fixed += fixed;

    // c) check that its inode is marked as allocated in the inode bitmap. Add to total fixed.
    struct ext2_inode *current = get_inode(i_num);
    check_i_bitmap(i_num, &total_fixed);

    // d) check that its inode's i_dtime is set to 0.
    check_dtime(i_num,&total_fixed);

    // e) check that all its data blocks are allocated in the data bitmap.
    int *b_array = block_to_array(current);
    int i = 0;
    while (b_array[i] != END_OF_ARRAY) {
        check_b_bitmap(b_array[i],&total_fixed,i_num);
        i++;
    }


    //if type is s or f, return.
    if (i_type == 's' || i_type == 'f' ) {
        return total_fixed;
    }

    //if type is d, continue.
    i = 0;
    while (b_array[i] != END_OF_ARRAY) {
        int mem_total = 0;
        struct ext2_dir_entry *entry;
        while (mem_total != EXT2_BLOCK_SIZE) {
            entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * b_array[i] + mem_total);
            if (entry->rec_len != EXT2_BLOCK_SIZE){
                if (entry->inode != i_parent){
                    if (entry->inode != i_num){
                        total_fixed += inode_check(entry->inode, i_num, &entry->file_type);
                    }
                }
            mem_total += entry->rec_len;
            } else {
                break;
            }
        }
        i++;
    }
    return total_fixed;
}


int absolute_value(int a, int b){
    if (a > b){
        return a-b;

    }else {
        return b-a;
    }
    //ERROR
    return -1;
}


int bitmap_check(){

    /***
     the superblock and block group counters for free blocks and free
     inodes must match the number of free inodes and data blocks as
     indicated in the respective bitmaps. If an inconsistency is detected,
     the checker will trust the bitmaps and update the counters
     ***/

    int total_fixed = 0;
    super_b = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    group_d = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE + sizeof(struct ext2_super_block));

    int free_inode = inode_count();
    int free_block = block_count();

    if (super_b->s_free_blocks_count != free_block) {
        int diff = absolute_value(super_b->s_free_blocks_count,free_block);
        total_fixed += diff;
        super_b->s_free_blocks_count = free_block;
        printf("Fixed: superblock's free blocks counter was off by %d compared to the bitmap\n", diff);
    }

    if (super_b->s_free_inodes_count != free_inode) {
        int diff = absolute_value(super_b->s_free_inodes_count,free_inode);
        total_fixed += diff;
        super_b->s_free_inodes_count = free_inode;
        printf("Fixed: superblock's free inodes counter was off by %d compared to the bitmap\n", diff);
    }

    if (group_d->bg_free_blocks_count != free_block) {
        int difff= absolute_value(group_d->bg_free_blocks_count,free_block);
        total_fixed += difff;
        group_d->bg_free_blocks_count = free_block;
        printf("Fixed: block group's free blocks counter was off by %d compared to the bitmap\n", difff);
    }

    if (group_d->bg_free_inodes_count != free_inode) {
        int diff = absolute_value(group_d->bg_free_inodes_count,free_inode);
        total_fixed += diff;
        group_d->bg_free_inodes_count = free_inode;
        printf("Fixed: block group's free inodes counter was off by %d compared to the bitmap\n", diff);
    }
    return total_fixed;

}




int main(int argc, const char * argv[]) {

    if(argc != 2) {
        fprintf(stderr, "Usage: <image name>\n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // a) check and fix super block and gourp descriptor, return total fixed.
    int total_fixed = 0;
    total_fixed += bitmap_check();

    // b~e) check and fix inodes attributes,return total fixed.
    unsigned char first_type = EXT2_FT_DIR;
    total_fixed += inode_check(EXT2_ROOT_INO, EXT2_ROOT_INO, &first_type);


    if (total_fixed) {
        printf("%d file system inconsistencies repaired!\n", total_fixed);
    } else {
        printf("No file system inconsistencies detected!\n");
    }


    return 0;

}
