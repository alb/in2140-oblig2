#include "inode.h"
#include "block_allocation.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Function that gets a new inode id incrementally.
// *NOT IMPLEMENTED*
uint32_t get_new_id() {
  fprintf(stderr, "%s is not implemented\n", __FUNCTION__);
  return 0;
}

// Function that copies a string to heap and returns pointer to the new string
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

// Function that frees all allocated memory and blocks for create_file upon
// failure
void free_create_file_resources(struct inode *i, uintptr_t *e, char *name,
                                uint32_t block) {
  free(i);
  free(e);
  free(name);
  free_block(block);
}

// Function that deletes an inode node from inode parent by moving it to
// the end of the entries array and redusing the num_entries.
// IMPORTANT: Only use on empty directories or files where all blocks are freed
// and when certain that node is in the directory parent.
void delete_inode(struct inode *parent, struct inode *node) {

  free((*node).name);
  free((*node).entries);

  // Overwrite the pointer to the inode to delete with the one in the last
  // position.
  for (int i = 0; i < (*parent).num_entries; i++) {
    struct inode *entry = (struct inode *)((*parent).entries[i]);
    if ((*entry).id == (*node).id) {
      (*parent).entries[i] = (*parent).entries[(*parent).num_entries - 1];
      break;
    }
  }

  (*parent).num_entries--;
}

// Function to add new to parent inode's entries. Returns 1 upon failure and 0
// upon success.
int add_inode(struct inode *parent, struct inode *new) {
  uintptr_t *new_entries;

  // Reallocate entries array with room for one more entry
  if ((new_entries = realloc((*parent).entries,
                             ((*parent).num_entries + 1) *
                                 sizeof((*parent).entries[0]))) == NULL)
    return 1;

  // Add the new entry
  new_entries[(*parent).num_entries] = (uintptr_t)new;

  // Update the pointer and number of entries
  (*parent).entries = new_entries;
  (*parent).num_entries++;

  return 0;
}

// Function that creates a new file in folder parent, with name name, is
// readonly if readonly with size size_in_bytes.
// Returns NULL upon failure and the new file upon success.
struct inode *create_file(struct inode *parent, const char *name, char readonly,
                          int size_in_bytes) {
  uint32_t blockno = -1;
  struct inode *return_inode = NULL;
  uint32_t num_entries = 1;
  uintptr_t *entries = NULL;
  char *name_pointer = NULL;
  uint32_t extent;

  // If file already exists, do nothing
  if (find_inode_by_name(parent, name) != NULL)
    return NULL;

  // Calculates ceil(size_in_bytes/BLOCKSIZE)
  extent = (size_in_bytes + BLOCKSIZE - 1) / BLOCKSIZE;
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

  if (!add_inode(parent, return_inode)) {
    free_create_file_resources(return_inode, entries, name_pointer, blockno);
    return NULL;
  }

  return return_inode;
}

// Function that creates a new directory in directory parent with name name.
// Returns NULL upon failure and the new directory upon success.
struct inode *create_dir(struct inode *parent, const char *name) {
  struct inode *new_dir = NULL;
  char *name_pointer = NULL;

  // If memory allocation fails, do nothing
  if ((name_pointer = copy_string(name)) == NULL)
    return NULL;
  if ((new_dir = malloc(sizeof(struct inode))) == NULL) {
    free(name_pointer);
    return NULL;
  }

  *new_dir = (struct inode){
      .id = get_new_id(),
      .name = name_pointer,
      .is_directory = 1,
      .is_readonly = 0,
      .filesize = 0,
      .num_entries = 0,
      .entries = NULL,
  };

  add_inode(parent, new_dir);

  return new_dir;
}

struct inode *find_inode_by_name(struct inode *parent, const char *name) {
  fprintf(stderr, "%s is not implemented\n", __FUNCTION__);
  return NULL;
}

// Function that deletes a file.
// Returns 0 on success and -1 on failure.
int delete_file(struct inode *parent, struct inode *node) {
  if (!(*parent).is_directory || (*node).is_directory ||
      find_inode_by_name(parent, (*node).name) == NULL)
    return -1;
  // Free all the file's memory blocks
  for (int i = 0; i < (*node).num_entries; i++) {
    int extent = (int)(*node).entries[i];
    int blockno = (int)((*node).entries[i] >> 32);
    for (int j = 0; j < extent; j++) {
      free_block(blockno + j);
    }
  }

  delete_inode(parent, node);

  return 0;
}

// Function that deletes an empty directory.
// Returns 0 on success and -1 on failure.
int delete_dir(struct inode *parent, struct inode *node) {
  if (!(*parent).is_directory || !(*node).is_directory ||
      (*node).num_entries != 0 ||
      find_inode_by_name(parent, (*node).name) == NULL)
    return -1;

  delete_inode(parent, node);

  return 0;
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

  free((*inode).name);
  free((*inode).entries);
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
