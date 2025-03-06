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

// Function that combines blockno and extent into a single 64-bit variable.
// Returns: The entry made from blockno and extent.
uintptr_t create_entry(uint32_t blockno, uint32_t extent) {
  return ((uintptr_t)blockno << 32) | extent;
}

// Function that stores the blockno and extent in an entry to blockno and extent
// pointers. Is NULL-safe.
void unpack_entry(uintptr_t entry, uint32_t *blockno, uint32_t *extent) {
  if (blockno != NULL)
    *blockno = (uint32_t)(entry >> 32);
  if (extent != NULL)
    *extent = (uint32_t)entry;
}

// Function that copies a string to heap and returns pointer to the new string
char *copy_string(const char *s) {
  char *copy;
  if ((copy = malloc(strlen(s) + 1)) == NULL)
    return NULL;

  return strcpy(copy, s);
}

// Function that frees all allocated memory and blocks for a file.
void free_file(struct inode *file, uintptr_t *entries, char *name,
               uint32_t no_entries) {
  for (uint32_t i = 0; i < no_entries; i++) {
    uint32_t blockno;
    uint32_t extent;
    unpack_entry(entries[i], &blockno, &extent);
    for (uint32_t j = 0; j < extent; j++)
      free_block(blockno + j);
  }
  free(file);
  free(entries);
  free(name);
}

// Function that deletes an inode node from inode parent by moving it to
// the end of the entries array and redusing the num_entries.
// IMPORTANT: Only use on empty directories or files where all blocks are freed
// and when certain that node is in the directory parent.
// Returns 0 on success and -1 on failure.
int delete_inode(struct inode *parent, struct inode *node) {

  // Overwrite the pointer to the inode to delete with the one in the last
  // position. If the order of the entries is relevant, just bubble it up.
  for (int i = 0; i < (*parent).num_entries; i++) {
    struct inode *entry = (struct inode *)((*parent).entries[i]);
    if ((*entry).id == (*node).id) {
      (*parent).entries[i] = (*parent).entries[(*parent).num_entries - 1];
      break;
    }
  }

  (*parent).num_entries--;

  uintptr_t *new_entries_parent;
  if ((new_entries_parent = realloc((*parent).entries,
                                    (*parent).num_entries *
                                        sizeof((*parent).entries[0]))) == NULL)
    return -1;

  (*parent).entries = new_entries_parent;
  return 0;
}

// Function to add new to parent inode's entries.
// Returns 0 on success and -1 on failure.
// upon success.
int add_inode(struct inode *parent, struct inode *new) {
  uintptr_t *new_entries;

  // Reallocate entries array with room for one more entry
  if ((new_entries = realloc((*parent).entries,
                             ((*parent).num_entries + 1) *
                                 sizeof((*parent).entries[0]))) == NULL)
    return -1;

  // Add the new entry
  new_entries[(*parent).num_entries] = (uintptr_t)new;

  // Update the pointer and number of entries
  (*parent).entries = new_entries;
  (*parent).num_entries++;

  return 0;
}

// Function to allocate the blocks needed for a file. Entries must have room for
// ceil(filesize/BLOCKSIZE) entries.
// IMPORTANT: Does not free allocated blocks if it fails. Does, however, keep
// track of the number of entries so they can be freed by free_file.
// Returns pointer to the last added entry on success, NULL on failure.
uintptr_t *allocate_blocks(uintptr_t *entries, uint32_t *num_entries,
                           uint32_t blocks_to_allocate) {
  uint32_t blockno;
  uint32_t extent = (blocks_to_allocate <= 4) ? blocks_to_allocate : 4;

  // If successfully allocating extent blocks, add to entries
  if (!(blockno = allocate_block(extent))) {
    *entries = create_entry(blockno, extent);
    entries++;
    (*num_entries)++;
  }
  // If allocating one block has failed, there is no more room on the disk or
  // other failure, return failure value
  else if (extent == 1) {
    return NULL;
  }
  // If allocating extent > 1 blocks fails, allocate twice with extents 1 and
  // extent-1.
  else {
    // If allocating one block fails, return failure
    if ((blockno = allocate_block(1)))
      return NULL;

    // Adding allocated block to entries
    *entries = create_entry(blockno, 1);
    (*num_entries)++;
    entries++;

    // Allocating the rest of the blocks by this function. If it fails, return
    // failure.
    if ((entries = allocate_blocks(entries, num_entries, extent - 1)) == NULL)
      return NULL;

    entries++;
  }

  // If there are more blocks to allocate, continue recursively
  if (!(blocks_to_allocate - extent)) {
    return allocate_blocks(entries + extent, num_entries,
                           blocks_to_allocate - extent);
  }

  // If there are no more blocks to allocate, return success.
  return entries - 1;
}

// Function that creates a new file in folder parent, with name name, is
// readonly if readonly with size size_in_bytes.
// Returns NULL upon failure and the new file upon success.
struct inode *create_file(struct inode *parent, const char *name, char readonly,
                          int size_in_bytes) {
  struct inode *new_file = NULL;
  uint32_t num_entries = 0;
  uintptr_t *entries = NULL;
  uintptr_t *realloc_entries;
  char *name_pointer = NULL;
  // Calculates ceil(size_in_bytes/BLOCKSIZE)
  uint32_t entire_file_blockno = (size_in_bytes + BLOCKSIZE - 1) / BLOCKSIZE;

  // If file already exists or size is 0, do nothing
  if (find_inode_by_name(parent, name) != NULL || !size_in_bytes)
    return NULL;

  if ((entries = malloc(sizeof(uintptr_t) * entire_file_blockno)) == NULL) {
    return NULL;
  }
  // If block allocation fails, do nothing
  if (allocate_blocks(entries, &num_entries, entire_file_blockno) == NULL) {
    free_file(new_file, entries, name_pointer, num_entries);
    return NULL;
  }

  // Reallocate entries array to be only the used size.
  if ((realloc_entries = realloc(entries, sizeof(uintptr_t) * num_entries)) ==
      NULL) {
    free_file(new_file, entries, name_pointer, num_entries);
    return NULL;
  }

  // If memory allocation fails, do nothing
  if ((name_pointer = copy_string(name)) == NULL) {
    free_file(new_file, entries, name_pointer, num_entries);
    return NULL;
  }
  if ((new_file = malloc(sizeof(struct inode))) == NULL) {
    free_file(new_file, entries, name_pointer, num_entries);
    return NULL;
  }

  *new_file = (struct inode){.id = get_new_id(),
                             .name = name_pointer,
                             .is_directory = 0,
                             .is_readonly = readonly,
                             .filesize = (uint32_t)size_in_bytes,
                             .num_entries = num_entries,
                             .entries = entries};

  if (!add_inode(parent, new_file)) {
    free_file(new_file, entries, name_pointer, num_entries);
    return NULL;
  }

  return new_file;
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

  if (!add_inode(parent, new_dir)) {
    free(name_pointer);
    free(new_dir);
    return NULL;
  }

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

  free_file(node, (*node).entries, (*node).name, (*node).num_entries);

  return delete_inode(parent, node);
}

// Function that deletes an empty directory.
// Returns 0 on success and -1 on failure.
int delete_dir(struct inode *parent, struct inode *node) {
  if (!(*parent).is_directory || !(*node).is_directory ||
      (*node).num_entries != 0 ||
      find_inode_by_name(parent, (*node).name) == NULL)
    return -1;

  free((*node).name);

  return delete_inode(parent, node);
}

// Function that writes bytes to writer in little-endian order. 
char * write(char *writer, uint32_t bytes) {
  for (int i = 0; i < 4; i++) {
    *writer = (char) (bytes >> i*8);
    writer++;
  }

  return writer;
}

// Function that writes inode and all its children to writer 
char *save_inodes_recursive(char *writer, struct inode *inode) {
  // Write id and name-length to writer. 
  // TODO: Should there be room for a termination character?
  writer = write(writer, (*inode).id);
  writer = write(writer, strlen((*inode).name));

  for (int i = 0; i < strlen((*inode).name); i++) {
    *writer = (*inode).name[i];
    writer++;
  }
  *writer = (*inode).is_directory;
  writer++;
  *writer = (*inode).is_readonly;
  writer++;

  write(writer, (*inode).num_entries);
  for (int i = 0; i < (*inode).num_entries; i++) {
    if ((*inode).is_directory) {
	writer = write(writer, (*inode).entries[i].id);
	writer = write(writer, 0);
    }
    else {
	uint32_t blockno;
	uint32_t extent;

	unpack_entry((*inode).entries[i], blockno, extent);
	writer = write(writer, blockno);
	writer = write(writer, extent);

    }

  }

  // If the inode is not a directory, return
  if (!(*inode).is_directory)
    return writer;


  // If the inode is a directory, write each inode.
  for (int i = 0; i < (*inode).num_entries;i++) {
    writer = save_inodes_recursive(writer, (struct inode *)(*inode).entries[i]);
  }

  return writer;
}

void save_inodes(const char *master_file_table, struct inode *root) {
  *save_inodes_recursive((char *) master_file_table, root) = '\0';
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
