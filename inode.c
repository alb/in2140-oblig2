#include "inode.h"
#include "block_allocation.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t get_new_id() {
  fprintf(stderr, "%s is not implemented\n", __FUNCTION__);
  return 0;
}

char *copy_string(const char *s) {
  char *copy;
  if ((copy = malloc(strlen(s))) == NULL)
    return NULL;

  int i = 0;
  while (s[i] != '\0') {
    copy[i] = s[i];
    i++;
  }

  copy[i] = '\0';
  return copy;
}

void free_create_file_resources(struct inode *i, uintptr_t *e, char *name,
                                uint32_t block) {
  free(i);
  free(e);
  free(name);
  free_block(block);
}

/*
 * The function takes as parameter a pointer to the inode of the directory
that will contain the new file. Within this directory, the name must be
unique. If there is a file or directory with the same name there already,
then the function should return NULL without doing anything. The parameter
size_in_bytes gives the number of bytes that must be stored on the simulated
disk for this file. The necessary number of blocks must allocated using the
function allocate_block, which is implemented in allocation.c. It is
possible that there is not enough space on the simulated disk, meaning that
a call to allocate_block will fail. You should release all resources in that
case and return NULL.
 */
struct inode *create_file(struct inode *parent, const char *name, char readonly,
                          int size_in_bytes) {
  uint32_t blockno = -1;
  struct inode *return_inode = NULL;
  uint32_t num_entries = 1;
  uintptr_t *entries = NULL;
  char *name_pointer = NULL;

  uint32_t extent = (size_in_bytes + BLOCKSIZE - 1) / BLOCKSIZE;

  // If file already exists, do nothing
  if (find_inode_by_name(parent, name) != NULL)
    return NULL;

  // If block allocation fails, do nothing
  if ((blockno = allocate_block(extent)) == -1) {
    fprintf(stderr, "Failed block allocation. Note that splitting file's "
                    "blocks is not yet implemented.");
    return NULL;
  }

  // If memory allocation fails, do nothing
  if ((name_pointer = copy_string(name)) == NULL) {
    free_create_file_resources(return_inode, entries, name_pointer, blockno);
    return NULL;
  }
  if ((return_inode = malloc(sizeof(struct inode))) == NULL) {
    free_create_file_resources(return_inode, entries, name_pointer, blockno);
    return NULL;
  }
  if ((entries = malloc(sizeof(uintptr_t) * extent)) == NULL) {
    free_create_file_resources(return_inode, entries, name_pointer, blockno);
    return NULL;
  }

  // creating a 64-bit integer with the block number as the first 32 bits and
  // the extent as the last.
  *entries = ((uintptr_t)blockno << 32) | extent;

  *return_inode = (struct inode){.id = get_new_id(),
                                 .name = name_pointer,
                                 .is_directory = 0,
                                 .is_readonly = readonly,
                                 .filesize = (uint32_t)size_in_bytes,
                                 .num_entries = num_entries,
                                 .entries = entries};

  return return_inode;
}

struct inode *create_dir(struct inode *parent, const char *name) {
  fprintf(stderr, "%s is not implemented\n", __FUNCTION__);
  return NULL;
}

struct inode *find_inode_by_name(struct inode *parent, const char *name) {
  fprintf(stderr, "%s is not implemented\n", __FUNCTION__);
  return NULL;
}

int delete_file(struct inode *parent, struct inode *node) {
  fprintf(stderr, "%s is not implemented\n", __FUNCTION__);
  return -1;
}

int delete_dir(struct inode *parent, struct inode *node) {
  fprintf(stderr, "%s is not implemented\n", __FUNCTION__);
  return -1;
}

void save_inodes(const char *master_file_table, struct inode *root) {
  fprintf(stderr, "%s is not implemented\n", __FUNCTION__);
  return;
}

struct inode *load_inodes(const char *master_file_table) {
  fprintf(stderr, "%s is not implemented\n", __FUNCTION__);
  return NULL;
}

void fs_shutdown(struct inode *inode) {
  if ((*inode).is_directory)
    for (int i = 0; i < (*inode).num_entries; i++) {
      fs_shutdown((struct inode *)(*inode).entries[i]);
    }

  free(inode);
  return;
}

/* This static variable is used to change the indentation while debug_fs
 * is walking through the tree of inodes and prints information.
 */
static int indent = 0;

static void debug_fs_print_table(const char *table);
static void debug_fs_tree_walk(struct inode *node, char *table);

void debug_fs(struct inode *node) {
  char *table = calloc(NUM_BLOCKS, 1);
  debug_fs_tree_walk(node, table);
  debug_fs_print_table(table);
  free(table);
}

static void debug_fs_tree_walk(struct inode *node, char *table) {
  if (node == NULL)
    return;
  for (int i = 0; i < indent; i++)
    printf("  ");
  if (node->is_directory) {
    printf("%s (id %d)\n", node->name, node->id);
    indent++;
    for (int i = 0; i < node->num_entries; i++) {
      struct inode *child = (struct inode *)node->entries[i];
      debug_fs_tree_walk(child, table);
    }
    indent--;
  } else {
    printf("%s (id %d size %d)\n", node->name, node->id, node->filesize);

    /* The following is an ugly solution. We expect you to discover a
     * better way of handling extents in the node->entries array, and did
     * it like this because we don't want to give away a good solution here.
     */
    uint32_t *extents = (uint32_t *)node->entries;

    for (int i = 0; i < node->num_entries; i++) {
      for (int j = 0; j < extents[2 * i + 1]; j++) {
        table[extents[2 * i] + j] = 1;
      }
    }
  }
}

static void debug_fs_print_table(const char *table) {
  printf("Blocks recorded in master file table:");
  for (int i = 0; i < NUM_BLOCKS; i++) {
    if (i % 20 == 0)
      printf("\n%03d: ", i);
    printf("%d", table[i]);
  }
  printf("\n\n");
}
